"""Frozen source contracts for the F14 Core target-scoped visibility request.

This tests an isolated *Core patch asset*. No concealment implementation,
client packet acknowledgement, module runtime edit or host setting is implied.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = (ROOT / "tests/core_patches/f14_event_visibility_refresh_request_prototype.patch").read_text()
DOC = (ROOT / "docs/F14_EVENT_VISIBILITY_REFRESH_REQUEST_2026-11-04.md").read_text()

class RefreshRequestContract(unittest.TestCase):
    def test_core_patch_exact_five_paths(self):
        names = re.findall(r"^diff --git a/(\S+) b/\1$", PATCH, re.M)
        self.assertEqual(names, [
            "src/server/game/Scripting/ScriptDefines/UnitScript.h",
            "src/server/game/Scripting/ScriptDefines/UnitScript.cpp",
            "src/server/game/Scripting/ScriptMgr.h",
            "src/server/game/Entities/Unit/Unit.cpp",
            "src/server/game/Entities/Creature/Creature.cpp",
        ])
        self.assertEqual(len(re.findall(r"^@@ ", PATCH, re.M)), 11)

    def test_default_false_and_opt_in_boolean_aggregate(self):
        self.assertIn("UNITHOOK_ON_F14_CREATURE_VISIBILITY_REFRESH_REQUEST", PATCH)
        self.assertIn("virtual bool OnF14ShouldRefreshCreatureVisibility(", PATCH)
        self.assertIn("{ return false; }", PATCH)
        self.assertIn("CALL_ENABLED_BOOLEAN_HOOKS_WITH_DEFAULT_FALSE(", PATCH)
        self.assertIn("script->OnF14ShouldRefreshCreatureVisibility(creature)", PATCH)
        self.assertIn("bool ScriptMgr::ShouldF14RefreshCreatureVisibility(", PATCH)
        self.assertNotIn("bool& /*request*/", PATCH)
        self.assertNotIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION 1", PATCH)

    def test_attack_switch_uses_guid_before_lifecycle_callback(self):
        self.assertIn("ObjectGuid const f14PreviousGuid = f14PreviousVictim ?", PATCH)
        self.assertIn("ObjectGuid const f14CurrentGuid = victim->GetGUID();", PATCH)
        attack = PATCH.index("sScriptMgr->OnF14AttackTargetTransition(this, f14PreviousVictim, victim);")
        self.assertLess(PATCH.index("ObjectGuid const f14CurrentGuid"), attack)
        self.assertLess(attack, PATCH.index("F14MaybeRefreshCreatureVisibility(this, f14PreviousGuid);"))
        self.assertIn("if (f14PreviousGuid != f14CurrentGuid)", PATCH)
        self.assertIn("F14MaybeRefreshCreatureVisibility(this, f14CurrentGuid);", PATCH)

    def test_attack_stop_captures_guid_before_detach_and_callback(self):
        self.assertIn("Unit* victim = m_attacking;", PATCH)
        self.assertIn("ObjectGuid const f14VictimGuid = victim->GetGUID();", PATCH)
        self.assertIn("sScriptMgr->OnF14AttackTargetTransition(this, victim, nullptr);", PATCH)
        self.assertIn("F14MaybeRefreshCreatureVisibility(this, f14VictimGuid);", PATCH)
        self.assertNotIn("+    F14MaybeRefreshCreatureVisibility(this, victim->GetGUID())", PATCH)
        self.assertNotIn("+    F14MaybeRefreshCreatureVisibility(this, f14PreviousVictim ?", PATCH)

    def test_refresh_re_resolves_and_does_not_hold_unit_pointer(self):
        self.assertIn("targetGuid.IsAnyTypeCreature()", PATCH)
        self.assertIn("attacker->IsInWorld()", PATCH)
        self.assertIn("Map* map = attacker->FindMap();", PATCH)
        self.assertIn("Creature* target = map->GetCreature(targetGuid);", PATCH)
        self.assertIn("target->IsPet()", PATCH)
        self.assertIn("sScriptMgr->ShouldF14RefreshCreatureVisibility(target)", PATCH)
        self.assertIn("target->UpdateObjectVisibility(true);", PATCH)
        self.assertLess(PATCH.index("ShouldF14RefreshCreatureVisibility(target)"),
                        PATCH.index("target->UpdateObjectVisibility(true);"))

    def test_corpse_and_respawn_post_bookkeeping(self):
        for event in ("OnF14CreatureCorpseReady(this)", "OnF14CreatureRespawned(this)"):
            self.assertIn("sScriptMgr->" + event, PATCH)
            self.assertLess(PATCH.index("sScriptMgr->" + event),
                            PATCH.index("ShouldF14RefreshCreatureVisibility(this)", PATCH.index("sScriptMgr->" + event)))
        self.assertEqual(PATCH.count("if (IsInWorld() && !IsPet() && sScriptMgr->ShouldF14RefreshCreatureVisibility(this))"), 2)
        self.assertEqual(PATCH.count("UpdateObjectVisibility(true);"), 3)

    def test_fail_visible_default_and_no_gameplay_activation(self):
        for forbidden in ("+    sScriptMgr->OnWorldObjectCanBeSeenByPlayer(",
                          "+#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION",
                          "+    ForceUpdateProgressionState(", "+    DestroyForPlayer(",
                          "+    SendDirectMessage(", "+    SetPhaseMask("):
            self.assertNotIn(forbidden, PATCH)
        self.assertIn("No creature hiding", DOC)
        self.assertIn("never held during visibility", DOC)
        self.assertIn("default FALSE", DOC)
        self.assertIn("single combined game test", DOC)

    def test_patch_format_clean(self):
        for line in PATCH.splitlines():
            self.assertFalse(line.endswith((" ", "\t")), repr(line))
        self.assertEqual(PATCH.count("diff --git a/"), 5)
        # Encoded empty diff contexts are restored by CI BEFORE git apply.
        self.assertEqual(PATCH.count("@F14_BLANK_CONTEXT@"), 4)
        self.assertNotIn("\\n-\\n+\\n", PATCH)

if __name__ == "__main__":
    unittest.main()
