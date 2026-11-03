"""F14 group sync containment: source contracts only. NOT a gameplay test."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "src/IndividualProgression.cpp").read_text()
PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
HEADER = (ROOT / "src/IndividualProgression.h").read_text()
POLICY = (ROOT / "src/IndividualProgressionF14LoginPolicy.h").read_text()
CONF = (ROOT / "conf/individualProgression.conf.dist").read_text()

KEY = "IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor"

def function(src, signature):
    start = src.index(signature)
    pos = src.index("{", start)
    depth = 0
    for i in range(pos, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i+1]
    raise AssertionError(signature)


class ConfigSafety(unittest.TestCase):
    def test_switch_off_in_both_config_and_runtime_fallback(self):
        self.assertEqual(len(re.findall(rf"^{re.escape(KEY)}\s*=\s*0\s*$", CONF, flags=re.M)), 1)
        self.assertIn(f'"{KEY}", false)', CORE)
        self.assertIn("f14ProtectVerifiedRandomBotGroupFloor = false", HEADER)
        self.assertIn("f14ProtectVerifiedRandomBotGroupFloor = false;", CORE)
        self.assertIn('#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")', CORE)

    def test_existing_f14_flag_and_enforce_rules_unchanged(self):
        for old in ("IndividualProgression.EnforceGroupRules = 0",
                    "IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin = 0"):
            self.assertIn(old, CONF)
        self.assertIn('GetOption<bool>("IndividualProgression.EnforceGroupRules", false)', CORE)


class IdentitySafety(unittest.TestCase):
    def test_random_bot_identity_read_only_in_login(self):
        login = function(PLAYER, "void OnPlayerLogin(Player* player) override")
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);", login)
        self.assertIn("f14ProtectVerifiedRandomBotGroupFloor", login)
        self.assertIn("sIndividualProgression->isBotAccount(player)", login)
        self.assertIn("player->GetSession()->IsHeadless()", login)
        self.assertIn("player->getRace() == RACE_DRAENEI", login)
        self.assertIn("player->getRace() == RACE_BLOODELF", login)
        self.assertIn("sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())", login)
        self.assertIn("RememberF14VerifiedRandomBotGroupIdentity(", login)
        self.assertIn("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE", login)
        # The older optional F14 login preserve may also check PB identity;
        # never call it from the group/map or logout hooks.
        self.assertNotIn("sRandomPlayerbotMgr", CORE)
        self.assertNotIn("sRandomPlayerbotMgr", function(PLAYER, "bool OnPlayerCanGroupAccept("))
        self.assertNotIn("sRandomPlayerbotMgr", function(PLAYER, "void OnPlayerLogout(Player* player) override"))
        self.assertNotIn("sRandomPlayerbotMgr", function(CORE, "bool IndividualProgression::ShouldSkipF14VerifiedRandomBotGroupDowngrade("))

    def test_refresh_on_login_and_cleanup_on_logout(self):
        logout = function(PLAYER, "void OnPlayerLogout(Player* player) override")
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);", logout)
        record = function(CORE, "void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(")
        self.assertIn("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);", record)
        self.assertIn("_f14VerifiedRandomBotGroupCache.erase(guid);", record)
        self.assertIn("_f14VerifiedRandomBotGroupCache[guid] = true;", record)
        should_skip = function(CORE, "bool IndividualProgression::ShouldSkipF14VerifiedRandomBotGroupDowngrade(")
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);", should_skip)
        for restriction in ("!enabled", "!f14ProtectVerifiedRandomBotGroupFloor", "enforceGroupRules",
                            "!player->IsInWorld()", "!isBotAccount(player)", "IsExpansionRace(player->getRace())",
                            "player->GetSession()->IsHeadless()"):
            self.assertIn(restriction, should_skip)
        self.assertIn("SkipVerifiedGroupDowngrade(", should_skip)


class NoDestructiveSync(unittest.TestCase):
    def test_only_two_group_writes_receive_guard(self):
        sync = function(CORE, "void IndividualProgression::SyncBotsProgressionToLeader(")
        self.assertIn("if (!member || isNormalAccount(member))", sync)
        self.assertIn("if (enforceGroupRules)", sync)
        pos = sync.index("ShouldSkipF14VerifiedRandomBotGroupDowngrade(member, permittedState)")
        self.assertLess(pos, sync.index("ForceUpdateProgressionState(member,"))
        self.assertRegex(sync[pos:], r"if \(ShouldSkipF14VerifiedRandomBotGroupDowngrade\(member, permittedState\)\)\s+continue;")
        accepted = function(PLAYER, "bool OnPlayerCanGroupAccept(")
        self.assertIn("if (sIndividualProgression->enforceGroupRules)", accepted)
        self.assertIn("!sIndividualProgression->isNormalAccount(player) && joiningState != leaderState &&", accepted)
        self.assertIn("sIndividualProgression->ShouldSkipF14VerifiedRandomBotGroupDowngrade(player, leaderState)", accepted)
        self.assertRegex(accepted, r"ShouldSkipF14VerifiedRandomBotGroupDowngrade\(player, leaderState\)\)\s+return true;")
        self.assertLess(accepted.index("ShouldSkipF14VerifiedRandomBotGroupDowngrade("),
                        accepted.index("ForceUpdateProgressionState("))
        # Preserve the ORIGINAL OFF-path write as a byte-for-byte guard.
        self.assertIn("if (!sIndividualProgression->isNormalAccount(player) && joiningState != leaderState)\n            sIndividualProgression->ForceUpdateProgressionState(", accepted)
        self.assertEqual(CORE.count("ShouldSkipF14VerifiedRandomBotGroupDowngrade(member, permittedState)"), 1)
        self.assertEqual(PLAYER.count("ShouldSkipF14VerifiedRandomBotGroupDowngrade(player, leaderState)"), 1)

    def test_pure_floor_only_skips_down_without_granting(self):
        self.assertIn("return level > 70 ? 13 : 8;", POLICY)
        self.assertIn("return currentStage >= floor && requestedStage < floor;", POLICY)
        self.assertNotIn("RewardQuest(", POLICY)
        self.assertNotIn("ForceUpdateProgressionState(", POLICY)
        skip = function(CORE, "bool IndividualProgression::ShouldSkipF14VerifiedRandomBotGroupDowngrade(")
        for banned in ("RewardQuest(", "ForceUpdateProgressionState(", "UpdateProgressionState(", "ModifyMoney("):
            self.assertNotIn(banned, skip)

if __name__ == "__main__":
    unittest.main()
