"""F14 opt-in actor publication: atomic config reload + no resurrection.
Source-only; no installed IP config, database or client is exercised.
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
CPP=(ROOT/"src/IndividualProgression.cpp").read_text(encoding="utf-8")
HDR=(ROOT/"src/IndividualProgression.h").read_text(encoding="utf-8")
PLAYER=(ROOT/"src/IndividualProgressionPlayer.cpp").read_text(encoding="utf-8")
FLAG="COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT"

class F14ReloadActorPublication(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.begin=CPP.split("void IndividualProgression::BeginF14ActorPublicationConfigReload()",1)[1].split(
            "void IndividualProgression::FinishF14ActorPublicationConfigReload(",1)[0]
        cls.finish=CPP.split("void IndividualProgression::FinishF14ActorPublicationConfigReload(",1)[1].split(
            "// New WHO/actor identity snapshot",1)[0]
        cls.reader=CPP.split("bool IndividualProgression::TryReadF14VerifiedActorPublication(",1)[1].split(
            "void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(",1)[0]
        cls.writer=CPP.split("void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(",1)[1].split(
            "#if defined("+FLAG+") && "+FLAG+" >= 1\nvoid IndividualProgression::Begin",1)[0]
        cls.load=CPP.split("static void LoadConfig()",1)[1].split("static void LoadXpValues()",1)[0]

    def test_no_new_path_in_normal_build(self):
        self.assertNotIn("#define "+FLAG,HDR+CPP+PLAYER)
        self.assertIn("#include <atomic>",HDR)
        self.assertIn("std::atomic_bool _f14ActorPublicationEnabled{false};",HDR)
        for token in ("BeginF14ActorPublicationConfigReload();","FinishF14ActorPublicationConfigReload(bool ipEnabled);"):
            self.assertIn(token,HDR)
        self.assertEqual(self.load.count("#if defined("+FLAG+") && "+FLAG+" >= 1"),2)

    def test_reload_begin_revokes_under_one_existing_mutex(self):
        self.assertIn("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);",self.begin)
        self.assertIn("_f14ActorPublicationEnabled.store(false, std::memory_order_release);",self.begin)
        self.assertIn("_f14VerifiedActorPublications.clear();",self.begin)
        self.assertLess(self.begin.index("store(false, std::memory_order_release)"),
                        self.begin.index("_f14VerifiedActorPublications.clear()"))
        self.assertNotIn("_f14VerifiedRandomBotGroupCache.clear()",self.begin)
        self.assertNotIn("_f14VerifiedRandomBotLoginEpoch.clear()",self.begin)
        self.assertNotIn("ForceUpdateProgressionState(",self.begin)

    def test_atomic_enable_only_after_complete_config(self):
        self.assertIn("_f14ActorPublicationEnabled.store(ipEnabled, std::memory_order_release);",self.finish)
        self.assertNotIn("_f14VerifiedActorPublications.emplace(",self.finish)
        self.assertIn("sIndividualProgression->BeginF14ActorPublicationConfigReload();",self.load)
        self.assertIn("sIndividualProgression->FinishF14ActorPublicationConfigReload(sIndividualProgression->enabled);",self.load)
        self.assertLess(self.load.index("BeginF14ActorPublicationConfigReload();"),
                        self.load.index('GetOption<bool>("IndividualProgression.Enable"'))
        self.assertGreater(self.load.index("FinishF14ActorPublicationConfigReload(sIndividualProgression->enabled);"),
                           self.load.index('GetOption<std::string>("IndividualProgression.PaidExpansion.VendorName"'))

    def test_reloader_cannot_resurrect_original_verified_group_identity(self):
        self.assertIn("_f14VerifiedActorPublications.clear();",self.begin)
        self.assertNotIn("_f14VerifiedActorPublications.emplace(",self.begin+self.finish)
        self.assertIn("_f14VerifiedActorPublications.erase(guid);",self.writer)
        self.assertIn("_f14VerifiedRandomBotLoginEpoch.erase(guid);",self.writer)
        self.assertIn("if (canPublishActor && _f14ActorPublicationEnabled.load(std::memory_order_acquire))",self.writer)
        self.assertLess(self.writer.index("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock)"),
                        self.writer.index("if (canPublishActor && _f14ActorPublicationEnabled.load("))

    def test_reader_never_reads_non_atomic_legacy_enabled_on_who_thread(self):
        self.assertIn("bool const enabled = _f14ActorPublicationEnabled.load(std::memory_order_acquire);",self.reader)
        self.assertIn("if (!enabled || !fullGuid || !fullGuid.IsPlayer())",self.reader)
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);",self.reader)
        self.assertIn("if (!_f14ActorPublicationEnabled.load(std::memory_order_acquire))",self.reader)
        self.assertLess(self.reader.index("bool const enabled = _f14ActorPublicationEnabled.load("),
                        self.reader.index("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock)"))
        self.assertLess(self.reader.index("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock)"),
                        self.reader.index("if (!_f14ActorPublicationEnabled.load("))
        self.assertIn("out = F14VerifiedActorPublication{};",self.reader)
        self.assertNotIn("sRandomPlayerbotMgr",self.reader)

    def test_unsafe_mutations_are_detected(self):
        def safe(s):
            return ("BeginF14ActorPublicationConfigReload();" in s and
                    "FinishF14ActorPublicationConfigReload(sIndividualProgression->enabled);" in s)
        self.assertTrue(safe(self.load))
        self.assertFalse(safe(self.load.replace("BeginF14ActorPublicationConfigReload();","",1)))
        self.assertFalse(safe(self.load.replace("FinishF14ActorPublicationConfigReload(sIndividualProgression->enabled);","",1)))
        original="if (canPublishActor && _f14ActorPublicationEnabled.load(std::memory_order_acquire))"
        self.assertIn(original,self.writer)
        self.assertNotIn(original,self.writer.replace(original,"if (canPublishActor)"))

    def test_actor_and_who_do_not_hide_or_grant_progression(self):
        hook=PLAYER.split("class IndividualProgressionF14Core6NegativeActorWhoAdapter",1)[1].split(
            "void AddSC_mod_individual_progression_player()",1)[0]
        self.assertEqual(hook.count("return false;"),2)
        self.assertNotIn("TryReadF14VerifiedActorPublication(",hook)
        for dangerous in ("SaveToDB(","SetPhaseMask(","ForceUpdateProgressionState(",
                          "DELETE FROM","RewardQuest(","DestroyForPlayer("):
            self.assertNotIn(dangerous,self.begin+self.finish+self.reader)

if __name__=="__main__":
    unittest.main()
