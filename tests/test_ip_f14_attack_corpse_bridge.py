"""Source contracts for F14 event callbacks in disposable Core and disabled IP branch."""
import os
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = (ROOT / "tests/core_patches/f14_attack_corpse_event_bridge_prototype.patch").read_text()
P = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
CORE = (ROOT / "src/IndividualProgression.cpp").read_text()

def method(text, head):
    start = text.index(head)
    begin = text.index("{", start)
    depth = 0
    for n in range(begin, len(text)):
        if text[n] == "{": depth += 1
        if text[n] == "}":
            depth -= 1
            if not depth: return text[start:n+1]
    raise AssertionError("unbalanced method: " + head)

class F14TransitionBridge(unittest.TestCase):
    def test_disposable_patch_scope(self):
        paths = re.findall(r"^diff --git a/(\S+) b/\1$", PATCH, re.M)
        self.assertEqual(set(paths), {
            "src/server/game/Scripting/ScriptDefines/UnitScript.h",
            "src/server/game/Scripting/ScriptDefines/UnitScript.cpp",
            "src/server/game/Scripting/ScriptMgr.h",
            "src/server/game/Entities/Unit/Unit.cpp",
            "src/server/game/Entities/Creature/Creature.cpp"
        })
        self.assertEqual(len(paths), 5)
        self.assertEqual(PATCH.count("@@ -"), 8)

    def test_runner_allowlists_match_the_actual_patch_sets(self):
        # Source-only CI applies exactly three F14 patches; the full native
        # job ALSO applies a separate two-file paid-row Core safety patch.
        patches = (
            PATCH,
            (ROOT / "tests/core_patches/f14_observer_visibility_default_allow_prototype.patch").read_text(),
            (ROOT / "tests/core_patches/f14_who_cached_entry_prototype.patch").read_text(),
        )
        exact = set()
        for content in patches:
            exact.update(re.findall(r"^diff --git a/(\S+) b/\1$", content, re.M))
        self.assertEqual(len(exact), 10)

        source = (ROOT / ".github/workflows/ip-f14-attack-corpse-bridge-source-20261103.yml").read_text()
        start = source.index("src/server/game/Entities/Object/Object.cpp")
        end = source.index("| sort >/tmp/f14-core-expected.txt", start)
        source_allow = set(re.findall(r"src/server/game/[A-Za-z0-9_./]+\.(?:cpp|h)",
                                          source[start:end]))
        self.assertEqual(source_allow, exact)

        native = (ROOT / ".github/workflows/ip-f14-attack-corpse-bridge-joint-native-20261103.yml").read_text()
        start = native.index("src/server/game/Entities/Object/Object.cpp",
                             native.index("Apply F14 attack-target + corpse-ready event bridge"))
        end = native.index("| sort > /tmp/f14-three-patches-expected.txt", start)
        native_allow = set(re.findall(r"src/server/game/[A-Za-z0-9_./]+\.(?:cpp|h)",
                                          native[start:end]))
        self.assertEqual(native_allow, exact | {
            "src/server/game/Entities/Player/PlayerSettings.cpp",
            "src/server/game/Entities/Player/Player.h",
        })

    def test_distinct_partial_contract(self):
        self.assertIn("#define COSMIC_F14_ATTACK_EVENT_HOOK_VERSION 1", PATCH)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", PATCH)
        self.assertIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", CORE)
        self.assertIn("#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION)", P)
        self.assertIn("new IndividualProgressionF14AttackEventScript();", P)
        self.assertIn("UNITHOOK_ON_F14_ATTACK_TARGET_TRANSITION", PATCH)
        self.assertIn("UNITHOOK_ON_F14_CREATURE_CORPSE_READY", PATCH)
        self.assertIn("UNITHOOK_ON_F14_CREATURE_RESPAWNED", PATCH)
        self.assertIn("void OnF14CreatureRespawned(", PATCH)

    def test_attack_event_positions(self):
        unit = PATCH.split("diff --git a/src/server/game/Entities/Unit/Unit.cpp", 1)[1].split("\ndiff --git a/", 1)[0]
        # A unified diff has both REMOVED (-) and ADDED (+) old code.
        # Checking raw string indexes would accidentally compare an old
        # deleted InterruptSpell() with a new added callback, i.e. a false
        # failure even when the actually patched Core ordering is correct.
        patched = "\n".join(line[1:] for line in unit.splitlines()
                            if line.startswith((" ", "+")) and not line.startswith("+++"))
        for expected in (
            "Unit* const f14PreviousVictim = m_attacking;",
            "m_attacking = victim;",
            "sScriptMgr->OnF14AttackTargetTransition(this, f14PreviousVictim, victim);",
            "m_attacking = nullptr;",
            "sScriptMgr->OnF14AttackTargetTransition(this, victim, nullptr);",
            "InterruptSpell(CURRENT_MELEE_SPELL);",
            "SendMeleeAttackStop(victim);",
            "Unit* owner = GetCharmerOrOwner();",
        ):
            self.assertIn(expected, patched)
        self.assertLess(patched.index("m_attacking = victim;"),
                        patched.index("sScriptMgr->OnF14AttackTargetTransition(this, f14PreviousVictim, victim);"))
        self.assertLess(patched.index("sScriptMgr->OnF14AttackTargetTransition(this, f14PreviousVictim, victim);"),
                        patched.index("Unit* owner = GetCharmerOrOwner();"),
                        "Attack begin/target switch notify before owner/AI callbacks")
        self.assertLess(patched.index("m_attacking = nullptr;"),
                        patched.index("sScriptMgr->OnF14AttackTargetTransition(this, victim, nullptr);"))
        self.assertLess(patched.index("sScriptMgr->OnF14AttackTargetTransition(this, victim, nullptr);"),
                        patched.index("InterruptSpell(CURRENT_MELEE_SPELL);"),
                        "AttackStop event must precede spell/AI cleanup")
        self.assertLess(patched.index("sScriptMgr->OnF14AttackTargetTransition(this, victim, nullptr);"),
                        patched.index("SendMeleeAttackStop(victim);"),
                        "AttackStop event must precede stop packet")

    def test_corpse_after_state_promotion(self):
        corpse = PATCH.split("diff --git a/src/server/game/Entities/Creature/Creature.cpp", 1)[1]
        patched = "\n".join(line[1:] for line in corpse.splitlines()
                            if line.startswith((" ", "+")) and not line.startswith("+++"))
        self.assertIn("Unit::setDeathState(DeathState::Corpse, despawn);", patched)
        self.assertIn("sScriptMgr->OnF14CreatureCorpseReady(this);", patched)
        self.assertLess(patched.index("Unit::setDeathState(DeathState::Corpse, despawn);"),
                        patched.index("sScriptMgr->OnF14CreatureCorpseReady(this);"),
                        "Corpse-ready event must occur AFTER the corpse state is committed")

    def test_respawn_refresh_after_alive_movement_and_phase_restoration(self):
        creature = PATCH.split("diff --git a/src/server/game/Entities/Creature/Creature.cpp", 1)[1]
        patched = "\n".join(line[1:] for line in creature.splitlines()
                            if line.startswith((" ", "+")) and not line.startswith("+++"))
        for expected in (
            "Unit::setDeathState(DeathState::Alive, despawn);",
            "Motion_Initialize();",
            "LoadCreaturesAddon(true);",
            "SetPhaseMask(GetCreatureData()->phaseMask, false);",
            "sScriptMgr->OnF14CreatureRespawned(this);",
        ):
            self.assertIn(expected, patched)
        for before in (
            "Unit::setDeathState(DeathState::Alive, despawn);",
            "Motion_Initialize();",
            "LoadCreaturesAddon(true);",
            "SetPhaseMask(GetCreatureData()->phaseMask, false);",
        ):
            self.assertLess(patched.index(before),
                            patched.index("sScriptMgr->OnF14CreatureRespawned(this);"),
                            f"Respawn hook must follow {before}")

    def test_verified_rndbot_only_and_force_immediate(self):
        cls = P.split("class IndividualProgressionF14AttackEventScript",1)[1].split("\n#endif",1)[0]
        attack = method(cls, "void OnF14AttackTargetTransition(")
        for yes in ("!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility",
                    "attacker->IsPlayer()", "candidate->IsInWorld()",
                    "candidate->GetSession()->IsHeadless()",
                    "candidate->getRace() != RACE_DRAENEI",
                    "candidate->getRace() != RACE_BLOODELF",
                    "HasF14VerifiedRandomBotIdentity(candidate)",
                    "oldVictim != newVictim", "oldVictim->UpdateObjectVisibility(true)",
                    "newVictim->UpdateObjectVisibility(true)"):
            self.assertIn(yes, attack)
        corpse = method(cls, "void OnF14CreatureCorpseReady(")
        respawn = method(cls, "void OnF14CreatureRespawned(")
        self.assertIn("!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility", corpse)
        self.assertIn("creature->UpdateObjectVisibility(true)", corpse)
        self.assertIn("!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility", respawn)
        self.assertIn("!creature || !creature->IsInWorld()", respawn)
        self.assertIn("creature->UpdateObjectVisibility(true)", respawn)
        for code in ("UpdateObjectVisibility(false)", "sRandomPlayerbotMgr", "ModifyMoney(", "RewardQuest(",
                     "SetPhaseMask(", "DestroyForVisiblePlayers(", "SaveToDB(", "CharacterDatabase"):
            self.assertNotIn(code, attack.replace("Unit::UpdateObjectVisibility(false)'s delayed notification", ""))
            self.assertNotIn(code, corpse)
            self.assertNotIn(code, respawn)

    def test_attack_start_requires_positive_identity_plus_login_incarnation(self):
        cls = P.split("class IndividualProgressionF14AttackEventScript", 1)[1].split("\n#endif", 1)[0]
        attack = method(cls, "void OnF14AttackTargetTransition(")
        gate = "GetF14VerifiedRandomBotLoginEpoch(candidate->GetGUID())"
        self.assertIn("HasF14VerifiedRandomBotIdentity(candidate)", attack)
        self.assertIn(gate, attack)
        self.assertLess(attack.index("HasF14VerifiedRandomBotIdentity(candidate)"),
                        attack.index(gate))
        self.assertLess(attack.index(gate),
                        attack.index("newVictim->UpdateObjectVisibility(true);"))
        # A logged-out bot can still trigger an old target SHOW refresh;
        # only the START/SWITCH path requires the current epoch.
        self.assertLess(attack.index("if (!newVictim)"), attack.index(gate))
        self.assertNotIn(gate, attack[:attack.index("if (!newVictim)")])

    def test_attack_stop_fails_visible_without_bot_login_identity(self):
        cls = P.split("class IndividualProgressionF14AttackEventScript", 1)[1].split("\n#endif", 1)[0]
        attack = method(cls, "void OnF14AttackTargetTransition(")
        # Stop is invoked after Unit::AttackStop has unlinked its target; a
        # logged-out actor may lack both its session and positive identity.
        # Only the SHOW/refresh path may bypass verification. Start/switch
        # still requires verified rndbot; neither may hide human actors.
        stop_index = attack.index("if (!newVictim)")
        checks_index = attack.index("HasF14VerifiedRandomBotIdentity(candidate)")
        self.assertLess(stop_index, checks_index)
        # Bound this proof to the actual STOP branch, not later START/SWITCH
        # identity gates. The old slice included allowed new-victim session
        # checks and produced a false failure even though STOP returns first.
        stop_end = attack.index("return;", stop_index) + len("return;")
        self.assertLess(stop_end, checks_index)
        stop_block = attack[stop_index:stop_end]
        self.assertIn("oldVictim->IsCreature()", stop_block)
        self.assertIn("oldVictim->IsInWorld()", stop_block)
        self.assertIn("oldVictim->UpdateObjectVisibility(true);", stop_block)
        self.assertIn("return;", stop_block)
        self.assertNotIn("GetSession()", stop_block)
        self.assertNotIn("GetEffectiveProgression(", stop_block)
        self.assertNotIn("SetPhaseMask(", stop_block)
        self.assertIn("!attacker || !attacker->IsPlayer()", attack[:stop_index])
        self.assertIn("!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility",
                      attack[:stop_index])
        self.assertIn("candidate->GetSession()->IsHeadless()", attack[checks_index - 450:checks_index])
        self.assertIn("HasF14VerifiedRandomBotIdentity(candidate)", attack[checks_index:])

    def test_existing_world_add_hook_recovers_pre_world_respawn(self):
        # The post-Alive Core respawn event can fire before AddToWorld, when
        # IsInWorld() correctly makes its refresh a no-op. Use existing
        # AllCreatureScript AddWorld callback, not a fourth Core patch seam.
        self.assertIn('#include "AllCreatureScript.h"', P)
        self.assertIn("class IndividualProgressionF14WorldAddScript final : public AllCreatureScript", P)
        self.assertIn("new IndividualProgressionF14WorldAddScript();", P)
        self.assertIn("new IndividualProgressionF14AttackEventScript();", P)
        added = method(P, "void OnCreatureAddWorld(Creature* creature) override")
        for condition in ("!sIndividualProgression->enabled",
                          "!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility",
                          "!creature || !creature->IsInWorld()"):
            self.assertIn(condition, added)
        self.assertIn("creature->UpdateObjectVisibility(true);", added)
        for forbidden in ("SetPhaseMask(", "GetLootRecipient", "GetLoot", "sRandomPlayerbotMgr",
                          "DestroyForVisiblePlayers(", "SaveToDB("):
            self.assertNotIn(forbidden, added)
        self.assertNotIn("void OnCreatureRemoveWorld(", P,
                         "Removal must not send packets while Core tears down the creature")

    def test_logout_avoids_unreviewed_cross_thread_visibility_work(self):
        # Core/Playerbots session teardown can invalidate a verified actor
        # without an AttackStop callback carrying an eligible headless
        # session. We intentionally DO NOT update NPC visibility directly
        # inside an unverified OnPlayerLogout worker; a separate audited
        # map-thread refresh/identity-expiry bridge is mandatory before F14 ON.
        logout = method(P, "void OnPlayerLogout(Player* player) override")
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player)", logout)
        self.assertNotIn("UpdateObjectVisibility(", logout)
        self.assertNotIn("SetPhaseMask(", logout)
        self.assertNotIn("sRandomPlayerbotMgr", logout)
        self.assertIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", CORE)
        self.assertIn("f14ExperimentalRndbotPlayerVisibility", CORE)

    def test_real_patched_core_when_available(self):
        path = os.environ.get("COSMIC_F14_PATCHED_CORE")
        if not path:
            self.skipTest("disposable Core not supplied")
        root = Path(path) / "src/server/game"
        unit = (root/"Entities/Unit/Unit.cpp").read_text()
        creature = (root/"Entities/Creature/Creature.cpp").read_text()
        script = (root/"Scripting/ScriptDefines/UnitScript.h").read_text()
        mgr = (root/"Scripting/ScriptMgr.h").read_text()
        self.assertIn("COSMIC_F14_ATTACK_EVENT_HOOK_VERSION", script)
        self.assertIn("void OnF14AttackTargetTransition(", mgr)
        self.assertEqual(unit.count("sScriptMgr->OnF14AttackTargetTransition(this,"), 2)
        self.assertEqual(creature.count("sScriptMgr->OnF14CreatureCorpseReady(this);"), 1)
        self.assertEqual(creature.count("sScriptMgr->OnF14CreatureRespawned(this);"), 1)
        self.assertIn("UNITHOOK_ON_F14_CREATURE_RESPAWNED", script)
        self.assertIn("void OnF14CreatureRespawned(", mgr)
        allcreature = (root / "Scripting/ScriptDefines/AllCreatureScript.h").read_text()
        allcreaturecpp = (root / "Scripting/ScriptDefines/AllCreatureScript.cpp").read_text()
        self.assertIn("virtual void OnCreatureAddWorld(Creature*", allcreature)
        self.assertIn("virtual void OnCreatureRemoveWorld(Creature*", allcreature)
        self.assertIn("script->OnCreatureAddWorld(creature);", allcreaturecpp)
        self.assertIn("script->OnCreatureRemoveWorld(creature);", allcreaturecpp)
        added = method(creature, "void Creature::AddToWorld()")
        self.assertLess(added.index("Unit::AddToWorld();"),
                        added.index("sScriptMgr->OnCreatureAddWorld(this);"))
        self.assertLess(added.index("AIM_Initialize();"),
                        added.index("sScriptMgr->OnCreatureAddWorld(this);"))
        removed = method(creature, "void Creature::RemoveFromWorld()")
        self.assertLess(removed.index("sScriptMgr->OnCreatureRemoveWorld(this);"),
                        removed.index("Unit::RemoveFromWorld();"))
        # Pinned Core logout ordering explains why a positive bot GUID cache
        # can disappear before the eventual AttackStop, and why the stop
        # fallback must not depend on the old headless session. This order
        # is NOT proof of a safe map-thread client visibility notification.
        logout_src = (root / "Server/WorldSession.cpp").read_text()
        logout = method(logout_src, "void WorldSession::LogoutPlayer(")
        self.assertLess(logout.index("sScriptMgr->OnPlayerLogout(_player);"),
                        logout.index("_player->CleanupsBeforeDelete();"))
        cleanup = method(unit, "void Unit::CleanupBeforeRemoveFromMap(")
        self.assertLess(cleanup.index("RemoveFromWorld();"),
                        cleanup.index("CombatStop();"),
                        "Core commonly stops combat AFTER world removal on logout")
        combat = method(unit, "void Unit::CombatStop(")
        self.assertIn("AttackStop();", combat)
        self.assertNotIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", script)

if __name__ == "__main__":
    unittest.main()
