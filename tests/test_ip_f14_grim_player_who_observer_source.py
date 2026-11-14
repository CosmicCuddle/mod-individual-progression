"""F14 player+WHO observer pilot: source contracts, not runtime game proof."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def read(name): return (ROOT/name).read_text(encoding="utf-8")

class F14ObserverSource(unittest.TestCase):
    def test_optional_flag_stays_off_without_full_visibility_refresh(self):
        conf=read("conf/individualProgression.conf.dist")
        core=read("src/IndividualProgression.cpp")
        header=read("src/IndividualProgression.h")
        key="IndividualProgression.F14.ExperimentalRndbotPlayerVisibility"
        self.assertEqual(len(re.findall(r"^"+re.escape(key)+r"\s*=\s*0\s*$",conf,re.M)),1)
        self.assertIn("bool f14ExperimentalRndbotPlayerVisibility = false;",header)
        self.assertIn('"'+key+'", false)',core)
        start=core.index("// F14 observer + WHO pilot remains OFF")
        part=core[start:core.index("#if !__has_include",start)]
        self.assertIn("!defined(COSMIC_F14_OBSERVER_HOOK_VERSION)",part)
        self.assertIn("!defined(COSMIC_F14_WHO_FILTER_HOOK_VERSION)",part)
        self.assertIn("!defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION)",part)
        self.assertIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION < 1",part)
        self.assertIn("f14ExperimentalRndbotPlayerVisibility = false;",part)
        self.assertIn("requires positive Playerbots RandomBot identity; forcing OFF",core)
        self.assertIn("IndividualProgression.F14.ForceVerifiedRandomBotTbcFloorOnLogin = 0",conf)

    def test_login_identity_cached_only_for_positive_headless_rndbot(self):
        player=read("src/IndividualProgressionPlayer.cpp")
        login=player.split("void OnPlayerLogin(Player* player) override",1)[1].split(
            "void OnPlayerLogout(Player* player) override",1)[0]
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);",login)
        self.assertIn("f14ExperimentalRndbotPlayerVisibility",login)
        for token in ("isBotAccount(player)","RACE_DRAENEI","RACE_BLOODELF",
                      "GetSession()->IsHeadless()",
                      "sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())"):
            self.assertIn(token,login)
        self.assertEqual(login.count("sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())"),3)
        logout=player.split("void OnPlayerLogout(Player* player) override",1)[1].split(
            "void OnPlayerGetTrainerSpellState(",1)[0]
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);",logout)

    def test_readonly_verified_epoch_fails_visible(self):
        header=read("src/IndividualProgression.h")
        core=read("src/IndividualProgression.cpp")
        for x in ("HasF14VerifiedRandomBotIdentity(", "HasF14VerifiedRandomBotGuid(",
                  "GetF14VerifiedRandomBotLoginEpoch(",
                  "_f14VerifiedRandomBotLoginEpoch",
                  "_f14NextRandomBotLoginEpoch"):
            self.assertIn(x,header)
        segment=core.split("void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(",1)[1].split(
            "bool IndividualProgression::ShouldSkipF14VerifiedRandomBotGroupDowngrade(",1)[0]
        self.assertIn("_f14VerifiedRandomBotLoginEpoch.erase(guid);",segment)
        self.assertIn("std::numeric_limits<std::uint64_t>::max()",segment)
        self.assertIn("std::unique_lock<std::shared_mutex>",segment)
        self.assertIn("std::shared_lock<std::shared_mutex>",segment)
        self.assertIn("if (!verified)",segment)
        self.assertIn("if (!guid || !guid.IsPlayer())",segment)
        self.assertNotIn("sRandomPlayerbotMgr",segment)

    def test_world_and_who_callbacks_are_allow_by_default_and_read_only(self):
        player=read("src/IndividualProgressionPlayer.cpp")
        body=player.split("class IndividualProgressionF14ObserverScript",1)[1].split(
            "void AddSC_mod_individual_progression_player()",1)[0]
        self.assertIn("OnWorldObjectCanBeSeenByPlayer(",body)
        self.assertIn("OnWhoListEntryCanBeSeenByPlayer(",body)
        self.assertIn("HasF14VerifiedRandomBotIdentity(candidate)",body)
        self.assertIn("HasF14VerifiedRandomBotGuid(targetGuid)",body)
        self.assertIn("GetEffectiveProgression(viewer) >= PROGRESSION_PRE_TBC",body)
        self.assertIn("!sIndividualProgression->isNormalAccount(viewer)",body)
        self.assertIn("!observer || !target || !target->IsPlayer()",body)
        self.assertIn("(targetRace != RACE_DRAENEI && targetRace != RACE_BLOODELF)",body)
        self.assertIn("!candidate->GetSession() || !candidate->GetSession()->IsHeadless()",body)
        for forbidden in ("sRandomPlayerbotMgr", "ObjectAccessor::FindPlayer",
                          "ForceUpdateProgressionState(", "UpdateObjectVisibility(",
                          "RewardQuest(", "ModifyMoney("):
            self.assertNotIn(forbidden,body)
        self.assertGreaterEqual(body.count("return true;"),5)
        reg=player.split("void AddSC_mod_individual_progression_player()",1)[1]
        self.assertIn("#if defined(COSMIC_F14_OBSERVER_HOOK_VERSION)",reg)
        self.assertIn("new IndividualProgressionF14ObserverScript();",reg)
        self.assertNotIn("new IndividualProgressionF14AttackEventScript()",reg)

if __name__ == "__main__":
    unittest.main()
