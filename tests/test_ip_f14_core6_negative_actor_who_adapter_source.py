"""IP127 -> owner Core6 exact actor-/who-API negative-only adapter, source audit.

This source test is NOT a real Core6 full Worldserver build, DB, or game test.
"""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
IP=(ROOT/"src/IndividualProgressionPlayer.cpp").read_text(encoding="utf-8")
POLICY=(ROOT/"src/IndividualProgressionF14ActorWhoPublicationContract.h").read_text(encoding="utf-8")
CONF=(ROOT/"conf/individualProgression.conf.dist").read_text(encoding="utf-8")

class Core6NegativeAdapterSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.macro="COSMIC_IP_F14_CORE6_ACTOR_WHO_NEGATIVE_ADAPTER_EXPERIMENT"
        cls.pilot=IP.split("// Experimental Core6 ABI adapter ONLY",1)[1].split(
            "void AddSC_mod_individual_progression_player()",1)[0]

    def test_normal_build_compiles_adapter_out(self):
        self.assertEqual(IP.count("#if defined("+self.macro+") && "+self.macro+" >= 1"),2)
        self.assertNotIn("#define "+self.macro,IP)
        self.assertNotIn("#define "+self.macro,POLICY)
        self.assertNotIn(self.macro,CONF)
        reg=IP.split("void AddSC_mod_individual_progression_player()",1)[1]
        self.assertIn("new IndividualProgressionF14Core6NegativeActorWhoAdapter();",reg)
        self.assertIn("#if defined("+self.macro+") && "+self.macro+" >= 1",reg)

    def test_exact_new_core5_who_and_core6_actor_abi_hooks(self):
        for phrase in (
            "class IndividualProgressionF14Core6NegativeActorWhoAdapter final : public WorldObjectScript",
            "WORLDOBJECTHOOK_ON_F14_VIEWER_PLAYER_SUPPRESSION_QUERY",
            "WORLDOBJECTHOOK_ON_F14_WHO_GUID_EXCLUSION_QUERY",
            "bool OnF14ShouldSuppressPlayerForViewer(",
            "Player const* /*viewer*/, Player const* /*actor*/) override",
            "bool OnF14ShouldExcludeWhoEntryForViewer(",
            "Player const* /*viewer*/, ObjectGuid const& /*targetGuid*/,",
            "uint32 /*cachedRace*/) override",
        ):
            self.assertIn(phrase,self.pilot)

    def test_two_callbacks_strictly_never_hide(self):
        self.assertEqual(self.pilot.count("return false;"),2)
        self.assertNotIn("return true;",self.pilot)
        for bad in ("sRandomPlayerbotMgr","IsRandomBot(","ForceUpdateProgressionState(",
                    "GetPreparedStatement(","Execute(","SetPhaseMask(",
                    "SetLootRecipient(","GetEffectiveProgression(",
                    "HasF14VerifiedRandomBotGuid(","HasF14VerifiedRandomBotIdentity("):
            self.assertNotIn(bad,self.pilot)

    def test_legacy_allow_hook_stays_separate_and_intact(self):
        old=IP.split("class IndividualProgressionF14ObserverScript",1)[1].split(
            "// Experimental Core6 ABI adapter ONLY",1)[0]
        self.assertIn("OnWhoListEntryCanBeSeenByPlayer(",old)
        self.assertIn("OnWorldObjectCanBeSeenByPlayer(",old)
        self.assertGreaterEqual(old.count("return true;"),5)
        self.assertNotIn("OnF14ShouldExcludeWhoEntryForViewer(",old)

    def test_cannot_change_source_model_into_live_authority(self):
        self.assertIn("AuthorizesRuntimeActorOrWhoHiding() { return false; }",POLICY)
        self.assertIn("AuthorizesNpcHidingOrRightsDenial() { return false; }",POLICY)
        self.assertIn("AuthorizesStage8OrPaidEntitlement() { return false; }",POLICY)

if __name__ == "__main__":
    unittest.main()
