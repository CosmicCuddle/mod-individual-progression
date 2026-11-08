"""F14 current authenticated RandomBot actor publication: source safety checks.

Exact login producer and one mutex-backed revocable snapshot, not active F14
client/WHO/creature hiding, born-clean stage8, DB or in-game evidence.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
C=(ROOT/"src/IndividualProgression.cpp").read_text(encoding="utf-8")
H=(ROOT/"src/IndividualProgression.h").read_text(encoding="utf-8")
P=(ROOT/"src/IndividualProgressionPlayer.cpp").read_text(encoding="utf-8")
IP128=(ROOT/"src/IndividualProgressionF14ActorWhoPublicationContract.h").read_text(encoding="utf-8")

class CurrentActorPublication(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.macro="COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT"
        cls.writer=C.split("void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(",1)[1].split(
            "void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(",1)[0]
        cls.reader=C.split("bool IndividualProgression::TryReadF14VerifiedActorPublication(",1)[1].split(
            "void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(",1)[0]

    def test_opt_in_false_absent_define_and_empty_snapshot_off(self):
        m=self.macro
        self.assertNotIn("#define "+m,H+C+P)
        self.assertIn("#if defined("+m+") && "+m+" >= 1",H)
        self.assertGreaterEqual(C.count("#if defined("+m+") && "+m+" >= 1"),3)
        self.assertIn("out = F14VerifiedActorPublication{};",self.reader)
        self.assertIn("(void)fullGuid;\n    return false;\n#endif",self.reader)
        self.assertNotIn(m,P)
        self.assertNotIn("TryReadF14VerifiedActorPublication(",P)

    def test_only_source_verified_login_may_publish_current_account_and_race(self):
        w=self.writer
        for term in ("ObjectGuid const fullGuid = player->GetGUID();",
                     "WorldSession const* currentSession = player->GetSession();",
                     "currentSession->GetAccountId()", "verified && fullGuid.IsPlayer()",
                     "player->IsInWorld()", "currentSession->IsHeadless()",
                     "!player->IsGameMaster()", "isBotAccount(player)", "accountId != 0",
                     "race == RACE_BLOODELF", "race == RACE_DRAENEI",
                     "if (canPublishActor)", "F14VerifiedActorPublication{fullGuid, accountId, race, loginEpoch}"):
            self.assertIn(term,w)
        self.assertIn("sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())",P)
        self.assertIn("void OnPlayerLogout(Player* player) override",P)

    def test_revoke_old_publication_under_same_lock_before_new_write(self):
        w=self.writer
        self.assertIn("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock)",w)
        before=w.index("_f14VerifiedActorPublications.erase(guid);")
        after=w.index("_f14VerifiedActorPublications.emplace(",before)
        self.assertLess(w.index("_f14VerifiedRandomBotLoginEpoch.erase(guid);"),before)
        self.assertLess(before,w.index("if (!verified)"))
        self.assertLess(before,w.index("std::numeric_limits<std::uint64_t>::max()"))
        self.assertLess(before,after)
        self.assertIn("ExpireF14LifecycleActor(expiredLoginEpoch);",w)

    def test_read_under_single_lock_matches_full_guid_epoch_account_race(self):
        r=self.reader
        for term in ("out = F14VerifiedActorPublication{};", "!fullGuid.IsPlayer()",
                     "std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock)",
                     "_f14VerifiedRandomBotGroupCache.find(lowGuid)",
                     "_f14VerifiedRandomBotLoginEpoch.find(lowGuid)",
                     "_f14VerifiedActorPublications.find(lowGuid)",
                     "identity->second.fullGuid != fullGuid",
                     "identity->second.loginEpoch != epoch->second",
                     "!identity->second.accountId", "identity->second.race != RACE_BLOODELF",
                     "identity->second.race != RACE_DRAENEI",
                     "out = identity->second;", "return true;"):
            self.assertIn(term,r)
        lock=r.index("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock)")
        self.assertLess(lock,r.index("_f14VerifiedActorPublications.find(lowGuid)"))
        self.assertLess(r.index("identity->second.loginEpoch != epoch->second"),r.index("out = identity->second;"))
        self.assertNotIn("sRandomPlayerbotMgr",r)
        self.assertNotIn("Player* ",r)
        self.assertNotIn("CharacterDatabase",r)

    def test_existing_ip128_adapter_never_promotes_a_snapshot_into_hiding(self):
        adapter=P.split("class IndividualProgressionF14Core6NegativeActorWhoAdapter",1)[1].split(
            "void AddSC_mod_individual_progression_player()",1)[0]
        self.assertEqual(adapter.count("return false;"),2)
        self.assertNotIn("TryReadF14VerifiedActorPublication(",adapter)
        self.assertIn("AuthorizesRuntimeActorOrWhoHiding() { return false; }",IP128)
        self.assertIn("AuthorizesNpcHidingOrRightsDenial() { return false; }",IP128)
        self.assertIn("AuthorizesStage8OrPaidEntitlement() { return false; }",IP128)

if __name__=="__main__":
    unittest.main()
