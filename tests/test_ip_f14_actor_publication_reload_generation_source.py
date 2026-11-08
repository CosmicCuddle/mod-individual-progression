"""F14 source regression: an in-flight bot login must not survive IP config reload.
Exact REVIEW-only structural test of the real module, not a native/gameplay test.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/IndividualProgression.cpp").read_text(encoding="utf-8")
HDR = (ROOT / "src/IndividualProgression.h").read_text(encoding="utf-8")
PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text(encoding="utf-8")
FLAG = "COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT"


class AtomicActorReloadGenerationSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.writer = CPP.split(
            "void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(", 1
        )[1].split(
            "#if defined(" + FLAG + ") && " + FLAG +
            " >= 1\nvoid IndividualProgression::Begin", 1
        )[0]
        cls.capture = CPP.split(
            "IndividualProgression::CaptureF14ActorPublicationLoginLease() const", 1
        )[1].split("// New WHO/actor identity snapshot", 1)[0]
        cls.login = PLAYER.split("void OnPlayerLogin(Player* player) override", 1)[1].split(
            "void OnPlayerLogout(Player* player) override", 1)[0]
        cls.begin = CPP.split(
            "void IndividualProgression::BeginF14ActorPublicationConfigReload()", 1
        )[1].split("void IndividualProgression::FinishF14ActorPublicationConfigReload(", 1)[0]
        cls.finish = CPP.split(
            "void IndividualProgression::FinishF14ActorPublicationConfigReload(", 1
        )[1].split("// New WHO/actor identity snapshot", 1)[0]
        cls.reader = CPP.split(
            "bool IndividualProgression::TryReadF14VerifiedActorPublication(", 1
        )[1].split("void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(", 1)[0]

    def test_compiled_out_by_default(self):
        self.assertIn("std::atomic<std::uint64_t> _f14ActorPublicationReloadGeneration{0};", HDR)
        self.assertIn("#include <atomic>", HDR)
        self.assertNotIn("#define " + FLAG, CPP + HDR + PLAYER)
        self.assertIn("#if defined(" + FLAG + ") && " + FLAG + " >= 1", CPP)
        self.assertIn("_f14ActorPublicationEnabled{false};", HDR)

    def test_caller_captures_lease_before_source_bot_verification(self):
        c = self.capture
        login = self.login
        self.assertIn(
            "_f14ActorPublicationReloadGeneration.load(std::memory_order_acquire);", c)
        self.assertIn(
            "_f14ActorPublicationEnabled.load(std::memory_order_acquire);", c)
        self.assertIn(
            "f14SourceLease = sIndividualProgression->CaptureF14ActorPublicationLoginLease();",
            login)
        self.assertIn(
            "player, sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter()),\n"
            "                f14SourceLease);", login)
        self.assertLess(login.index("CaptureF14ActorPublicationLoginLease();"),
                        login.index("ForgetF14VerifiedRandomBotGroupIdentity(player);"))
        self.assertLess(login.index("CaptureF14ActorPublicationLoginLease();"),
                        login.index("sRandomPlayerbotMgr.IsRandomBot("))
        self.assertIn(
            "F14ActorPublicationLoginLease sourceLease", self.writer.split("{", 1)[0])
        self.assertNotIn("CaptureF14ActorPublicationLoginLease();", self.writer)

    def test_writer_rechecks_generation_under_existing_lock(self):
        w = self.writer
        self.assertIn("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);", w)
        self.assertIn(
            "if (canPublishActor && _f14ActorPublicationEnabled.load(std::memory_order_acquire))", w)
        self.assertIn("if (sourceLease.publicationWasEnabled &&", w)
        self.assertIn("sourceLease.generation ==", w)
        self.assertIn(
            "_f14ActorPublicationReloadGeneration.load(std::memory_order_acquire))", w)
        self.assertLess(w.index("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock)"),
                        w.index("if (sourceLease.publicationWasEnabled &&"))
        self.assertLess(w.index("if (sourceLease.publicationWasEnabled &&"),
                        w.index("_f14VerifiedActorPublications.emplace("))
        self.assertIn("_f14VerifiedActorPublications.erase(guid);", w)

    def test_reload_increments_once_under_lock_and_never_wraps(self):
        b = self.begin
        self.assertIn("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);", b)
        self.assertIn("_f14ActorPublicationEnabled.store(false, std::memory_order_release);", b)
        self.assertIn(
            "if (previousGeneration != std::numeric_limits<std::uint64_t>::max())", b)
        self.assertIn(
            "_f14ActorPublicationReloadGeneration.store(\n            previousGeneration + 1, std::memory_order_release);",
            b)
        self.assertLess(b.index("_f14ActorPublicationEnabled.store(false"),
                        b.index("_f14ActorPublicationReloadGeneration.store("))
        self.assertLess(b.index("_f14ActorPublicationReloadGeneration.store("),
                        b.index("_f14VerifiedActorPublications.clear();"))
        self.assertNotIn("_f14VerifiedRandomBotGroupCache.clear()", b)
        self.assertNotIn("_f14VerifiedRandomBotLoginEpoch.clear()", b)

    def test_exhaustion_keeps_publisher_off_and_does_not_rearm_stale_identity(self):
        f = self.finish
        self.assertIn(
            "_f14ActorPublicationReloadGeneration.load(std::memory_order_acquire) ==", f)
        self.assertIn("std::numeric_limits<std::uint64_t>::max()", f)
        self.assertLess(f.index("std::numeric_limits<std::uint64_t>::max()"),
                        f.index("_f14ActorPublicationEnabled.store(ipEnabled"))
        self.assertIn("_f14ActorPublicationEnabled.store(ipEnabled, std::memory_order_release);", f)
        self.assertNotIn("_f14VerifiedActorPublications.emplace(", f)
        self.assertNotIn("_f14VerifiedActorPublications.emplace(", self.begin)

    def test_inherited_reader_and_core_abi_remain_false_only(self):
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);", self.reader)
        self.assertIn("if (!_f14ActorPublicationEnabled.load(std::memory_order_acquire))", self.reader)
        self.assertIn("out = F14VerifiedActorPublication{};", self.reader)
        hook = PLAYER.split(
            "class IndividualProgressionF14Core6NegativeActorWhoAdapter", 1
        )[1].split("void AddSC_mod_individual_progression_player()", 1)[0]
        self.assertEqual(hook.count("return false;"), 2)
        for term in ("ForceUpdateProgressionState(", "RewardQuest(", "SaveToDB(",
                     "DELETE FROM", "SetPhaseMask(", "DestroyForPlayer("):
            self.assertNotIn(term, self.begin + self.finish + self.reader)

    def test_mutations_removing_lease_gate_are_detected(self):
        w = self.writer
        self.assertNotIn(
            "sourceLease.generation ==",
            w.replace("sourceLease.generation ==", "true ||", 1))
        self.assertNotIn(
            "if (sourceLease.publicationWasEnabled &&",
            w.replace("if (sourceLease.publicationWasEnabled &&", "if (true &&", 1))
        b = self.begin
        self.assertNotIn(
            "previousGeneration + 1",
            b.replace("previousGeneration + 1", "previousGeneration", 1))


if __name__ == "__main__":
    unittest.main()
