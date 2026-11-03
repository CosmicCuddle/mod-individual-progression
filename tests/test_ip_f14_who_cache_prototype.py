"""F14 WHO cached-entry Core seam and IP source contract. DRAFT, OFF, NO GAMEPLAY."""
import os
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = (ROOT / "tests/core_patches/f14_who_cached_entry_prototype.patch").read_text()
CORE = (ROOT / "src/IndividualProgression.cpp").read_text()
HEADER = (ROOT / "src/IndividualProgression.h").read_text()
PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
CONFIG = (ROOT / "conf/individualProgression.conf.dist").read_text()


def method(src, signature):
    start = src.index(signature)
    brace = src.index("{", start)
    depth = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        if src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i+1]
    raise AssertionError("Unbalanced method " + signature)


class WHOPrototypeBoundary(unittest.TestCase):
    def test_pinned_core_only_four_paths_six_nonoverlapping_hunks(self):
        paths = re.findall(r"^diff --git a/(\S+) b/\1$", PATCH, re.M)
        self.assertEqual(set(paths), {
            "src/server/game/Scripting/ScriptMgr.h",
            "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
            "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
            "src/server/game/Handlers/MiscHandler.cpp",
        })
        self.assertEqual(len(paths), 4)
        self.assertEqual(PATCH.count("@@ -"), 6)

    def test_positive_cache_only_no_unsafe_target_lookup(self):
        guid = method(CORE, "bool IndividualProgression::HasF14VerifiedRandomBotGuid(")
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);", guid)
        self.assertIn("_f14VerifiedRandomBotGroupCache.find(guid.GetCounter())", guid)
        for bad in ("FindPlayer(", "GetPlayer(", "sRandomPlayerbotMgr", "CharacterDatabase",
                    "WorldDatabase", "GetSession(", "GetEffectiveProgression("):
            self.assertNotIn(bad, guid)
        self.assertIn("bool HasF14VerifiedRandomBotGuid(ObjectGuid const& guid);", HEADER)

    def test_who_hook_gated_and_registered_for_specific_hook_id(self):
        self.assertIn("#if defined(COSMIC_F14_WHO_FILTER_HOOK_VERSION)", PLAYER)
        self.assertIn("WORLDOBJECTHOOK_ON_WHO_LIST_ENTRY_CAN_BE_SEEN_BY_PLAYER", PLAYER)
        self.assertIn("OnWhoListEntryCanBeSeenByPlayer(Player const* observer", PLAYER)
        who = method(PLAYER, "bool OnWhoListEntryCanBeSeenByPlayer(")
        for yes in (
            "!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility",
            "!observer || !targetGuid",
            "observer->GetGUID() == targetGuid",
            "targetRace != RACE_DRAENEI",
            "targetRace != RACE_BLOODELF",
            "HasF14VerifiedRandomBotGuid(targetGuid)",
            "!observer->GetSession()",
            "observer->GetSession()->IsHeadless()",
            "!sIndividualProgression->isNormalAccount(viewer)",
            "GetEffectiveProgression(viewer) >= PROGRESSION_PRE_TBC",
        ):
            self.assertIn(yes, who)
        for bad in ("target->", "FindPlayer(", "ObjectAccessor", "sRandomPlayerbotMgr",
                    "ModifyMoney(", "RewardQuest(", "SaveToDB(", "ForceUpdateProgressionState("):
            self.assertNotIn(bad, who)

    def test_core_callback_only_reads_cached_who_entry(self):
        self.assertIn("WORLDOBJECTHOOK_ON_WHO_LIST_ENTRY_CAN_BE_SEEN_BY_PLAYER", PATCH)
        self.assertIn("#define COSMIC_F14_WHO_FILTER_HOOK_VERSION 1", PATCH)
        self.assertIn("return true; }", PATCH)
        self.assertIn("CALL_ENABLED_BOOLEAN_HOOKS(WorldObjectScript,", PATCH)
        self.assertIn("!script->OnWhoListEntryCanBeSeenByPlayer(observer, targetGuid, targetRace)", PATCH)
        self.assertIn("target.GetGuid(), target.GetRace()", PATCH)
        who_section = PATCH.split("diff --git a/src/server/game/Handlers/MiscHandler.cpp", 1)[1]
        self.assertIn("AccountMgr::IsPlayerAccount(GetSecurity())", who_section)
        self.assertNotIn("ObjectAccessor::FindPlayer(", who_section)
        self.assertNotIn("ObjectAccessor::FindConnectedPlayer(", who_section)

    def test_no_partial_activation_without_immediate_refresh(self):
        self.assertIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", CORE)
        self.assertNotIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", PATCH)
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", CONFIG)
        self.assertIn("f14ExperimentalRndbotPlayerVisibility = false;", CORE)

    def test_actual_disposable_core_when_provided(self):
        path = os.environ.get("COSMIC_F14_PATCHED_CORE")
        if not path:
            self.skipTest("no disposable Core checkout provided")
        core = Path(path)
        script = (core / "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h").read_text()
        mgr = (core / "src/server/game/Scripting/ScriptMgr.h").read_text()
        misc = (core / "src/server/game/Handlers/MiscHandler.cpp").read_text()
        self.assertIn("COSMIC_F14_WHO_FILTER_HOOK_VERSION", script)
        self.assertIn("bool OnWhoListEntryCanBeSeenByPlayer(", mgr)
        self.assertEqual(misc.count("OnWhoListEntryCanBeSeenByPlayer(_player, target.GetGuid(), target.GetRace())"), 1)
        self.assertNotIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", script)

if __name__ == "__main__":
    unittest.main()
