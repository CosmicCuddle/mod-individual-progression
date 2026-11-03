"""F14 player-only verified-rndbot observer wiring: source contract, NOT gameplay.
The new Core observer seam is a disposable CI patch; /who + refresh remain pending.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
CORE = (ROOT / "src/IndividualProgression.cpp").read_text()
HEADER = (ROOT / "src/IndividualProgression.h").read_text()
CONF = (ROOT / "conf/individualProgression.conf.dist").read_text()
PATCH = (ROOT / "tests/core_patches/f14_observer_visibility_default_allow_prototype.patch").read_text()

KEY = "IndividualProgression.F14.ExperimentalRndbotPlayerVisibility"


def method(source, start):
    pos = source.index(start)
    opening = source.index("{", pos)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[pos:i + 1]
    raise AssertionError("Unbalanced method " + start)


class F14RuntimePilot(unittest.TestCase):
    def test_default_off_and_missing_dependencies_force_off(self):
        self.assertEqual(len(re.findall(r"^" + re.escape(KEY) + r"\s*=\s*0\s*$",
                                        CONF, re.M)), 1)
        self.assertIn("std::atomic<bool> f14ExperimentalRndbotPlayerVisibility{false};", HEADER)
        self.assertIn('"' + KEY + '", false)', CORE)
        self.assertIn("#if !defined(COSMIC_F14_OBSERVER_HOOK_VERSION) || !defined(COSMIC_F14_WHO_FILTER_HOOK_VERSION) || !defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION)", CORE)
        self.assertNotIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", PATCH,
                         "Current Core prototype must never claim immediate refresh")
        self.assertNotIn("COSMIC_F14_WHO_FILTER_HOOK_VERSION", PATCH,
                         "Unimplemented WHO seam must not be falsely advertised by Core patch")
        self.assertIn("f14ExperimentalRndbotPlayerVisibility = false;", CORE)
        self.assertIn("F14 experimental RandomBot world visibility requires Playerbots identity API", CORE)
        self.assertIn("#define COSMIC_F14_OBSERVER_HOOK_VERSION 1", PATCH)
        self.assertIn("NOT full F14", CONF)
        self.assertIn("/who", CONF)

    def test_identity_is_positive_headless_rndbot_only(self):
        login = method(PLAYER, "void OnPlayerLogin(Player* player) override")
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);", login)
        self.assertIn("f14ExperimentalRndbotPlayerVisibility", login)
        self.assertIn("sIndividualProgression->isBotAccount(player)", login)
        self.assertIn("player->GetSession()->IsHeadless()", login)
        self.assertIn("player->getRace() == RACE_DRAENEI", login)
        self.assertIn("player->getRace() == RACE_BLOODELF", login)
        self.assertIn("sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())", login)
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);",
                      method(PLAYER, "void OnPlayerLogout(Player* player) override"))
        reader = method(CORE, "bool IndividualProgression::HasF14VerifiedRandomBotIdentity(")
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);", reader)
        self.assertIn("_f14VerifiedRandomBotGroupCache.find(", reader)
        self.assertNotIn("sRandomPlayerbotMgr", reader)
        self.assertNotIn("CharacterDatabase", reader)
        self.assertNotIn("WorldDatabase", reader)

    def test_player_visibility_is_narrow_and_gated(self):
        code = method(PLAYER, "bool OnWorldObjectCanBeSeenByPlayer(")
        for required in (
            "!sIndividualProgression->enabled",
            "!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility",
            "!observer || !target || !target->IsPlayer()",
            "candidate == observer",
            "!candidate->IsInWorld()",
            "candidate->getRace() != RACE_DRAENEI",
            "candidate->getRace() != RACE_BLOODELF",
            "!candidate->GetSession()->IsHeadless()",
            "!sIndividualProgression->HasF14VerifiedRandomBotIdentity(candidate)",
            "!observer->IsInWorld()",
            "observer->GetSession()->IsHeadless()",
            "!sIndividualProgression->isNormalAccount(viewer)",
            "GetEffectiveProgression(viewer) >= PROGRESSION_PRE_TBC",
        ):
            self.assertIn(required, code, required)
        self.assertLess(code.index("HasF14VerifiedRandomBotIdentity"),
                        code.index("GetEffectiveProgression(viewer)"))
        for banned in ("RewardQuest(", "ForceUpdateProgressionState(",
                       "ModifyMoney(", "SetPhaseMask(", "DespawnOrUnsummon(",
                       "sRandomPlayerbotMgr", "ResetQuests(", "DestroyForNearbyPlayers("):
            self.assertNotIn(banned, code)
        self.assertEqual(code.count("return false;"), 0,
                         "Only the final effective-stage comparison can veto")

    def test_script_compilation_and_registration_are_core_macro_guarded(self):
        self.assertIn("#if defined(COSMIC_F14_OBSERVER_HOOK_VERSION)\nclass IndividualProgressionF14ObserverScript", PLAYER)
        self.assertIn("WORLDOBJECTHOOK_ON_WORLD_OBJECT_CAN_BE_SEEN_BY_PLAYER", PLAYER)
        self.assertIn("#if defined(COSMIC_F14_OBSERVER_HOOK_VERSION)\n    new IndividualProgressionF14ObserverScript();\n#endif", PLAYER)
        self.assertEqual(PLAYER.count("new IndividualProgressionF14ObserverScript();"), 1)
        self.assertIn("return true;", method(PLAYER, "bool OnWorldObjectCanBeSeenByPlayer("))
        self.assertIn("return true; }", PATCH)
        self.assertNotIn("CreatureSnapshot", method(PLAYER, "bool OnWorldObjectCanBeSeenByPlayer("))

    def test_no_new_database_or_playerbot_writes(self):
        code = method(PLAYER, "bool OnWorldObjectCanBeSeenByPlayer(")
        for banned in ("SaveToDB(", "UpdatePlayerSetting(", "RemoveRewardedQuest("):
            self.assertNotIn(banned, code)
        self.assertIn("Only this existing", PLAYER)

if __name__ == "__main__":
    unittest.main()
