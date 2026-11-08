"""Source-only ownership/parity guard for IP120 -> Core6 F14 actor/WHO model.

This cannot authenticate a live RandomBot, effective era, Worldserver or DB.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
HEADER=(ROOT/"src/IndividualProgressionF14ActorWhoPublicationContract.h").read_text()
IP=(ROOT/"src/IndividualProgression.cpp").read_text()
IPPLAYER=(ROOT/"src/IndividualProgressionPlayer.cpp").read_text()

class UnifiedF14ActorWhoSource(unittest.TestCase):
    def test_model_unwired_no_positive_authorization(self):
        for s in ("AuthorizesRuntimeActorOrWhoHiding() { return false; }",
                  "AuthorizesNpcHidingOrRightsDenial() { return false; }",
                  "AuthorizesStage8OrPaidEntitlement() { return false; }"):
            self.assertIn(s,HEADER)
        for token in ("SaveToDB(","ForceUpdateProgressionState(","SetPhaseMask(",
                      "GetPreparedStatement(","RegisterWorldObjectScript("):
            self.assertNotIn(token,HEADER)

    def test_all_surfaces_share_one_positive_producer_source(self):
        self.assertIn("CommonSourceAndViewerMayAsk(",HEADER)
        self.assertIn("MayAskActorSurface(",HEADER)
        self.assertIn("MayAskWhoSurface(",HEADER)
        self.assertEqual(HEADER.count("return CommonSourceAndViewerMayAsk(p, o) &&"),2)
        for x in ("positiveCurrentLoginProducerProof", "activeAndNotRetired",
                  "genuineHeadlessRandomBot", "notAltbotAndNotHuman",
                  "sourceIdentityStableForThisQuery", "loginEpoch != 0",
                  "accountId != 0", "currentEffectiveStageVerified",
                  "eraChangePending"):
            self.assertIn(x,HEADER)

    def test_who_is_guid_race_only_not_live_target_dereference(self):
        who=HEADER.split("constexpr bool MayAskWhoSurface(",1)[1]
        self.assertIn("w.fullPlayerGuid == p.fullPlayerGuid && w.race == p.race",who)
        self.assertNotIn("ToPlayer(",who)
        self.assertNotIn("IsRandomBot(",who)
        self.assertNotIn("GetSession(",who)
        self.assertIn("a.loginEpoch == p.loginEpoch",HEADER)

    def test_preserve_existing_ip120_positive_identity_owner_no_thread_unsafe_lookup(self):
        self.assertIn("RememberF14VerifiedRandomBotGroupIdentity(",IP)
        self.assertIn("HasF14VerifiedRandomBotGuid(",IP)
        self.assertIn("HasF14VerifiedRandomBotIdentity(",IP)
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(",IPPLAYER)
        self.assertIn("OnWhoListEntryCanBeSeenByPlayer(",IPPLAYER)
        self.assertIn("GetEffectiveProgression(viewer) >= PROGRESSION_PRE_TBC",IPPLAYER)
        self.assertNotIn("IndividualProgressionF14ActorWhoPublicationContract.h",IPPLAYER)
        self.assertNotIn("IndividualProgressionF14ActorWhoPublicationContract.h",IP)

if __name__=="__main__":
    unittest.main()
