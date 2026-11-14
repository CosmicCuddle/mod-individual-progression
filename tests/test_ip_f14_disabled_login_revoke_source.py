"""F14 logout / disabled-IP and early re-login revocation source tests.

Source-only regression: no client hide, database, config or in-game test.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
PLAYER=(ROOT/"src/IndividualProgressionPlayer.cpp").read_text(encoding="utf-8")
IP=(ROOT/"src/IndividualProgression.cpp").read_text(encoding="utf-8")
HEADER=(ROOT/"src/IndividualProgression.h").read_text(encoding="utf-8")

def login_order_is_safe(source):
    begin=source.split("void OnPlayerLogin(Player* player) override",1)[1].split(
        "void OnPlayerLogout(Player* player) override",1)[0]
    pre=begin.split("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE",1)[0]
    expected=("if (!player)\n            return;",
              "ForgetF14VerifiedRandomBotGroupIdentity(player);",
              "if (!sIndividualProgression->enabled || !player->IsInWorld())")
    return (all(pre.count(s)==1 for s in expected)
            and pre.index(expected[0]) < pre.index(expected[1]) < pre.index(expected[2])
            and "RememberF14VerifiedRandomBotGroupIdentity(" not in pre)

class F14StaleLoginIdentity(unittest.TestCase):
    def test_existing_verified_rndbot_proof_and_floor_unchanged(self):
        self.assertTrue(login_order_is_safe(PLAYER))
        login=PLAYER.split("void OnPlayerLogin(Player* player) override",1)[1].split(
            "void OnPlayerLogout(Player* player) override",1)[0]
        for term in ("sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())",
                     "sIndividualProgression->isBotAccount(player)",
                     "player->GetSession()->IsHeadless()",
                     "RACE_BLOODELF","RACE_DRAENEI",
                     "if (!sIndividualProgression->isNormalAccount(player))"):
            self.assertIn(term,login)
        self.assertIn("RememberF14VerifiedRandomBotGroupIdentity(",login)

    def test_old_disabled_early_return_pattern_is_regression(self):
        altered=PLAYER.replace(
            "if (!player)\n            return;\n\n        // F14 identity invalidation",
            "if (!sIndividualProgression->enabled)\n            return;\n\n        // F14 identity invalidation",1)
        self.assertFalse(login_order_is_safe(altered))

    def test_old_forget_after_enabled_guard_is_regression(self):
        altered=PLAYER.replace(
            "if (!sIndividualProgression->enabled || !player->IsInWorld())",
            "if (!sIndividualProgression->enabled)\n            return;\n"
            "        if (!player->IsInWorld())",1)
        self.assertFalse(login_order_is_safe(altered))

    def test_no_early_return_before_forget_for_non_null_player(self):
        login=PLAYER.split("void OnPlayerLogin(Player* player) override",1)[1].split(
            "void OnPlayerLogout(Player* player) override",1)[0]
        prefix=login.split("ForgetF14VerifiedRandomBotGroupIdentity(player);",1)[0]
        self.assertIn("if (!player)",prefix)
        self.assertNotIn("!sIndividualProgression->enabled",prefix)
        self.assertNotIn("!player->IsInWorld()",prefix)
        self.assertEqual(login.count("ForgetF14VerifiedRandomBotGroupIdentity(player);"),1)
        logout=PLAYER.split("void OnPlayerLogout(Player* player) override",1)[1].split(
            "void OnPlayerGetTrainerSpellState(",1)[0]
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);",logout)

    def test_opt_in_snapshot_reader_disallows_runtime_disabled_ip(self):
        reader=IP.split("bool IndividualProgression::TryReadF14VerifiedActorPublication(",1)[1].split(
            "void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(",1)[0]
        self.assertIn("out = F14VerifiedActorPublication{};",reader)
        self.assertIn("if (!enabled || !fullGuid || !fullGuid.IsPlayer())",reader)
        self.assertLess(reader.index("if (!enabled || !fullGuid || !fullGuid.IsPlayer())"),
                        reader.index("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock)"))
        self.assertIn("return false;\n#endif",reader)
        self.assertNotIn("sRandomPlayerbotMgr",reader)
        self.assertIn("_f14VerifiedActorPublications.erase(guid);",IP)
        self.assertIn("_f14VerifiedRandomBotLoginEpoch.erase(guid);",IP)
        self.assertIn("_f14GroupCacheLock",HEADER)

    def test_no_new_hide_write_or_stage_modification(self):
        login=PLAYER.split("void OnPlayerLogin(Player* player) override",1)[1].split(
            "#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE",1)[0]
        for term in ("SetPhaseMask(", "DestroyForPlayer(", "ShouldF14SuppressPlayerForViewer(",
                     "ForceUpdateProgressionState(", "SaveToDB(", "DELETE FROM",
                     "RewardQuest("):
            self.assertNotIn(term,login)

if __name__ == "__main__":
    unittest.main()
