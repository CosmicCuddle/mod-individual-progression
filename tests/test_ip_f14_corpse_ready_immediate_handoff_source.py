#!/usr/bin/env python3
"""F14 #115 source contracts for EXISTING early corpse-ready refresh handoff.

This is NOT live client proof and never edits the Core or a protocol. Checks
exact pinned Grim source plus the ORIGINAL #56/#77 disposable Core patches
already tested in native #112/#113. Missing source inputs => test is BLOCKED.
"""
import os
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CREATURE = "src/server/game/Entities/Creature/Creature.cpp"
ATTACK_PATCH = "tests/core_patches/f14_attack_corpse_event_bridge_prototype.patch"
REFRESH_PATCH = "tests/core_patches/f14_event_visibility_refresh_request_prototype.patch"
CORE_OBJECT = "src/server/game/Entities/Object/Object.cpp"
CORE_PLAYER_UPDATES = "src/server/game/Entities/Player/PlayerUpdates.cpp"
CORE_NOTIFIER = "src/server/game/Grids/Notifiers/GridNotifiers.cpp"

EXPECTED_ORIGINAL = """        Unit::setDeathState(DeathState::Corpse, despawn);
    }
    else if (state == DeathState::JustRespawned)"""
EXPECTED_ATTACK_FRAGMENT = """+        Unit::setDeathState(DeathState::Corpse, despawn);
+        // Corpse-ready event occurs after all transition-to-corpse updates.
+        sScriptMgr->OnF14CreatureCorpseReady(this);"""
EXPECTED_REFRESH_FRAGMENT = """         sScriptMgr->OnF14CreatureCorpseReady(this);
+        // Corpse loot/quest rights require restoring the viewer's object.
+        if (IsInWorld() && !IsPet() && sScriptMgr->ShouldF14RefreshCreatureVisibility(this))
+            UpdateObjectVisibility(true);"""


def exact_once(haystack, needle, label):
    if haystack.count(needle) != 1:
        raise ValueError(f"{label}: expected exact one pinned anchor, got {haystack.count(needle)}")


def validate_chain(creature, attack, refresh, lifecycle):
    exact_once(creature, EXPECTED_ORIGINAL, "original Grim corpse transition")
    exact_once(attack, EXPECTED_ATTACK_FRAGMENT, "original attack/corpse Core patch")
    exact_once(refresh, EXPECTED_REFRESH_FRAGMENT, "original after-event refresh Core patch")
    exact_once(creature, "void Creature::setDeathState(DeathState state, bool despawn)",
               "original setDeathState")
    exact_once(creature, "void Creature::RemoveCorpse(bool setSpawnTime, bool skipVisibility)",
               "original RemoveCorpse")
    exact_once(creature, "void Creature::DisappearAndDie()", "disappear-and-die path")
    set_body = creature.split("void Creature::setDeathState(DeathState state, bool despawn)", 1)[1]
    set_body = set_body.split("\n}\n", 1)[0]
    pos = set_body.index("Unit::setDeathState(DeathState::Corpse, despawn);")
    if set_body.count("Unit::setDeathState(DeathState::Corpse, despawn);") != 1:
        raise ValueError("unreviewed additional death-state transition")
    if "RemoveCorpse(" in set_body:
        raise ValueError("unreviewed direct corpse removal inside transition")
    # The two reviewed patch fragments compose at the same exact single
    # anchor. The event must be AFTER promotion to Corpse, BEFORE a
    # same-stack visibility refresh, all BEFORE the caller's RemoveCorpse.
    reconstructed = set_body[:pos] + (
        "Unit::setDeathState(DeathState::Corpse, despawn);\n"
        "        sScriptMgr->OnF14CreatureCorpseReady(this);\n"
        "        if (IsInWorld() && !IsPet() && sScriptMgr->ShouldF14RefreshCreatureVisibility(this))\n"
        "            UpdateObjectVisibility(true);\n"
    ) + set_body[pos + len("Unit::setDeathState(DeathState::Corpse, despawn);"):]
    order = (
        "Unit::setDeathState(DeathState::Corpse, despawn);",
        "sScriptMgr->OnF14CreatureCorpseReady(this);",
        "sScriptMgr->ShouldF14RefreshCreatureVisibility(this)",
        "UpdateObjectVisibility(true);",
    )
    positions = [reconstructed.index(token) for token in order]
    if positions != sorted(positions) or len(set(positions)) != len(positions):
        raise ValueError("incorrect corpse-ready -> refresh temporal order")
    if reconstructed.count("sScriptMgr->OnF14CreatureCorpseReady(this);") != 1:
        raise ValueError("duplicate death callback")

    disappear = creature.split("void Creature::DisappearAndDie()", 1)[1].split("\n}\n", 1)[0]
    if not (disappear.count("setDeathState(DeathState::JustDied, true);") == 1
            and disappear.count("RemoveCorpse(false, true);") == 1
            and disappear.index("setDeathState(DeathState::JustDied, true);")
            < disappear.index("RemoveCorpse(false, true);")):
        raise ValueError("immediate despawn could bypass corpse-ready callback")

    remove = creature.split("void Creature::RemoveCorpse(bool setSpawnTime, bool skipVisibility)", 1)[1]
    remove = remove.split("change the entry of creature until respawn", 1)[0]
    if remove.count("if (_respawnCompatibilityMode)") != 1:
        raise ValueError("corpse removal mode changed")
    compatibility, dynamic = remove.split("    else\n    {", 1)
    if not ("setDeathState(DeathState::Dead);" in compatibility
            and "DestroyForVisiblePlayers();" in compatibility
            and "SaveRespawnTime();" in dynamic
            and "AddObjectToRemoveList();" in dynamic):
        raise ValueError("corpse removal modes changed")

    # This is the ORIGINALLY compiled IP consumer. It is not a prior-hidden
    # ledger consumer and is optional under a development compile macro.
    exact_once(lifecycle, "void OnF14CreatureCorpseReady(Creature* creature) override",
               "F14 corpse callback consumer")
    exact_once(lifecycle, "bool OnF14ShouldRefreshCreatureVisibility(Creature const* creature) override",
               "F14 one-shot refresh consumer")
    start = lifecycle.index("bool OnF14ShouldRefreshCreatureVisibility(Creature const* creature) override")
    end = lifecycle.index("\n#endif", start)
    consumer = lifecycle[start:end]
    for token in (
        "std::lock_guard<std::mutex> guard(LifecycleLock());",
        "!state || !state->everTracked || !state->refreshPending",
        "state->refreshPending = false;",
        "return true;",
    ):
        if token not in consumer:
            raise ValueError(f"F14 one-shot evidence missing {token}")
    retire = lifecycle.split("static void RetireIncarnation(Creature* creature, bool respawn)", 1)[1]
    retire = retire.split("\n};", 1)[0]
    if not (retire.index("MarkPendingRefresh(creature);") <
            retire.index("SetStamp(creature, 0);")):
        raise ValueError("lost post-retirement refresh request")
    if "CompleteRestoreOnlyWithClientProof(" in lifecycle or "ClearFrozenAfterGlobalProof(" in lifecycle:
        raise ValueError("unknown prior-hidden ledger mutation in runtime")
    if "OnF14ShouldSuppressCreatureForViewer(" in lifecycle:
        raise ValueError("positive F14 hide wired; review separately")
    return True


def verify_generic_refresh_delivery(object_source, notifier_source, player_updates):
    # Confirm that Core's existing creature-wide visibility request does
    # reach each nearby Player::UpdateVisibilityOf direct path. This is a
    # necessary server routing fact, NOT client acknowledgement or proof of
    # one viewer's previous-F14-hidden/quest/loot entitlements.
    key = "void WorldObject::UpdateObjectVisibility(bool /*forced*/, bool /*fromUpdate*/)"
    exact_once(object_source, key, "Core WorldObject visibility entry")
    body = object_source.split(key, 1)[1].split("\n}\n", 1)[0]
    expected = ("Acore::VisibleChangesNotifier notifier(*this);",
                "Cell::VisitObjects(this, notifier, GetVisibilityRange());")
    if any(body.count(x) != 1 for x in expected) or body.index(expected[0]) >= body.index(expected[1]):
        raise ValueError("existing Core creature-wide visibility route changed")

    key = "void VisibleChangesNotifier::Visit(PlayerMapType& m)"
    exact_once(notifier_source, key, "nearby human player notifier")
    visit = notifier_source.split(key, 1)[1].split("\n}\n", 1)[0]
    token = "iter->GetSource()->UpdateVisibilityOf(&i_object);"
    if visit.count(token) != 1:
        raise ValueError("missing or duplicated direct player callback")
    # Shared vision observers can also trigger visibility updates, so this
    # cannot be equated with a single current approved human viewer.
    if "HasSharedVision()" not in visit:
        raise ValueError("shared-vision path changed or omitted from audit")

    key = "void Player::UpdateVisibilityOf(WorldObject* target)"
    exact_once(player_updates, key, "direct Core player visibility path")
    direct = player_updates.split(key, 1)[1].split("\n}\n", 1)[0]
    steps = (
        "if (HaveAtClient(target))",
        "else",
        "if (CanSeeOrDetect(target, false, true))",
        "target->SendUpdateToPlayer(this);",
        "GetObjectVisibilityContainer().LinkWorldObjectVisibility(target);",
    )
    if any(direct.count(x) != 1 for x in steps):
        raise ValueError("Core direct branch changed; no inferred server CREATE path")
    positions = [direct.index(x) for x in steps]
    if positions != sorted(positions) or len(set(positions)) != len(positions):
        raise ValueError("direct Core CREATE/link order changed")
    if "GetObjectVisibilityContainer().UnlinkWorldObjectVisibility(target);" not in direct:
        raise ValueError("Core ordinary unlink path missing")
    return True


class PreRemovalHandoffSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        required = [os.environ.get(k) for k in (
            "F14_PINNED_CORE_ROOT", "F14_PINNED_ATTACK_PATCH_ROOT",
            "F14_PINNED_REFRESH_PATCH_ROOT")]
        if not all(required):
            raise unittest.SkipTest("Exact pinned Core and ORIGINAL patch checkouts not provided")
        cls.creature = (Path(required[0]) / CREATURE).read_text(encoding="utf-8")
        cls.core_object = (Path(required[0]) / CORE_OBJECT).read_text(encoding="utf-8")
        cls.core_notifier = (Path(required[0]) / CORE_NOTIFIER).read_text(encoding="utf-8")
        cls.core_player = (Path(required[0]) / CORE_PLAYER_UPDATES).read_text(encoding="utf-8")
        cls.attack = (Path(required[1]) / ATTACK_PATCH).read_text(encoding="utf-8")
        cls.refresh = (Path(required[2]) / REFRESH_PATCH).read_text(encoding="utf-8")
        cls.lifecycle = (ROOT / "src/IndividualProgressionF14LifecycleScript.cpp").read_text(encoding="utf-8")

    def test_original_temporal_contract_with_both_corpse_modes(self):
        self.assertTrue(validate_chain(self.creature, self.attack, self.refresh, self.lifecycle))

    def test_immediate_refresh_reaches_core_direct_player_visibility_create_path(self):
        self.assertTrue(verify_generic_refresh_delivery(self.core_object, self.core_notifier, self.core_player))

    def test_missing_core_player_create_path_is_rejected(self):
        altered = self.core_player.replace("target->SendUpdateToPlayer(this);", "NoClientSend();", 1)
        with self.assertRaises(ValueError):
            verify_generic_refresh_delivery(self.core_object, self.core_notifier, altered)

    def test_missing_notifier_player_visit_is_rejected(self):
        altered = self.core_notifier.replace("iter->GetSource()->UpdateVisibilityOf(&i_object);", "NoViewerCallback();", 1)
        with self.assertRaises(ValueError):
            verify_generic_refresh_delivery(self.core_object, altered, self.core_player)

    def test_removing_attack_corpse_hook_fails(self):
        altered = self.attack.replace(
            "+        sScriptMgr->OnF14CreatureCorpseReady(this);",
            "+        sScriptMgr->OnUnknownCreatureCorpseReady(this);")
        with self.assertRaises(ValueError):
            validate_chain(self.creature, altered, self.refresh, self.lifecycle)

    def test_removing_immediate_refresh_fails(self):
        altered = self.refresh.replace(
            "+            UpdateObjectVisibility(true);",
            "+            NoVisibilityRefresh();")
        with self.assertRaises(ValueError):
            validate_chain(self.creature, self.attack, altered, self.lifecycle)

    def test_bypassing_immediate_disappear_order_fails(self):
        altered = self.creature.replace(
            "    RemoveCorpse(false, true);\n}",
            "    RemoveCorpse(false, true);\n}\n// replacement", 1)
        # Replace the actual original to prove a changed call order is refused.
        altered = altered.replace(
            "        setDeathState(DeathState::JustDied, true);\n    RemoveCorpse(false, true);",
            "        RemoveCorpse(false, true);\n    setDeathState(DeathState::JustDied, true);", 1)
        with self.assertRaises(ValueError):
            validate_chain(altered, self.attack, self.refresh, self.lifecycle)

    def test_unreviewed_compatibility_mode_fails(self):
        altered = self.creature.replace("if (_respawnCompatibilityMode)",
                                        "if (_unexpectedCorpseMode)", 1)
        with self.assertRaises(ValueError):
            validate_chain(altered, self.attack, self.refresh, self.lifecycle)

    def test_wrong_postretirement_refresh_order_fails(self):
        altered = self.lifecycle.replace("        MarkPendingRefresh(creature);\n#endif",
                                         "        MarkPendingRefresh(creature);\n#endif",
                                         1)
        # Replace the exact retire path; unrelated occurrences must remain.
        part = altered.index("static void RetireIncarnation(Creature* creature, bool respawn)")
        before, tail = altered[:part], altered[part:]
        tail = tail.replace("        MarkPendingRefresh(creature);",
                            "        UnknownRefresh(creature);", 1)
        with self.assertRaises((ValueError, AssertionError)):
            validate_chain(self.creature, self.attack, self.refresh, before + tail)

if __name__ == "__main__":
    unittest.main()
