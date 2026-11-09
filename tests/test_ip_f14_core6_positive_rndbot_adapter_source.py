#!/usr/bin/env python3
"""Core6 F14 positive RNDbot adapter exact-parent contract, test-only host.
Old Core6 adapter returned FALSE; this child adds an explicitly triple-gated
positive caller. No real Core producer, packet, client ACK or deployment.
"""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
PARENT = "7ca897e2397216405f910f46e6a12e5b4e0b987b"
MACRO = "COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_VIEWER_POLICY_EXPERIMENT"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def old(path):
    return subprocess.check_output(
        ["git", "show", f"{PARENT}:{path}"], cwd=ROOT
    ).decode("utf-8")


class F14PositiveBridge(unittest.TestCase):
    def test_old_negative_core6_adapter_proven_and_exactly_recoverable(self):
        path = "src/IndividualProgressionPlayer.cpp"
        before = old(path)
        after = read(path)
        self.assertNotIn(MACRO, before)
        self.assertIn("return false;", before)
        self.assertIn("TryReadF14VerifiedActorPublication(", after)
        self.assertIn("CosmicIPF14Core6PositiveRndbot::HideActor(", after)
        self.assertIn("CosmicIPF14Core6PositiveRndbot::ExcludeWho(", after)
        # Removing only the scoped include and the additional body of the
        # original adapter must reconstruct parent IP137 byte-for-byte.
        start = after.index("// The independent Core6 adapter remains default-FALSE.")
        end = after.index("// Optional Playerbots dependency:", start)
        after = after[:start] + after[end:]
        old_start = before.index("    // New Core6 actor client CREATE/OUT query:")
        old_end = before.index("\n};\n#endif", old_start)
        new_start = after.index(f"#if defined({MACRO}) && {MACRO} >= 1",
                                after.index("class IndividualProgressionF14Core6NegativeActorWhoAdapter"))
        new_end = after.index("\n};\n#endif", new_start)
        after = after[:new_start] + before[old_start:old_end] + after[new_end:]
        self.assertEqual(after, before)

    def test_manual_positive_build_requires_both_existing_independent_proofs(self):
        s = read("src/IndividualProgressionPlayer.cpp")
        self.assertIn('#error "F14 positive Core6 policy requires the current-login verified actor publication experiment"', s)
        self.assertIn('#error "F14 positive Core6 policy requires the exact existing Core6 actor+WHO adapter"', s)
        self.assertIn("COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT", s)
        self.assertIn("COSMIC_IP_F14_CORE6_ACTOR_WHO_NEGATIVE_ADAPTER_EXPERIMENT", s)
        self.assertIn("TryReadF14VerifiedActorPublication(", s)
        self.assertIn("published.fullGuid == targetGuid", s)
        self.assertIn("published.accountId != 0", s)
        self.assertIn("published.race == claimedRace", s)
        self.assertIn("published.loginEpoch != 0", s)
        self.assertIn("liveAccountId == published.accountId", s)
        self.assertIn("actorSession && actorSession->IsHeadless() && accountId != 0", s)
        self.assertIn("!actor->IsGameMaster()", s)
        self.assertIn("!viewer->IsInWorld() || viewer->IsGameMaster()", s)
        self.assertIn("sIndividualProgression->isNormalAccount(normalViewer)", s)
        self.assertIn("GetEffectiveProgression(normalViewer)", s)

    def test_actual_who_does_not_dereference_cached_target(self):
        s = read("src/IndividualProgressionPlayer.cpp")
        start = s.index("bool OnF14ShouldExcludeWhoEntryForViewer(",
                        s.index("class IndividualProgressionF14Core6NegativeActorWhoAdapter"))
        end = s.index("\n    }", start)
        body = s[start:end]
        self.assertIn("TryReadF14VerifiedActorPublication(", s)
        self.assertIn("ReadVerifiedPublication(", body)
        self.assertIn("static_cast<uint8>(cachedRace), 0", body)
        self.assertNotIn("ObjectAccessor::", body)
        self.assertNotIn("FindPlayer(", body)
        self.assertNotIn("targetGuid->", body)
        self.assertNotIn("sRandomPlayerbotMgr", body)

    def test_manual_positive_not_actually_default_on(self):
        s = read("src/IndividualProgressionPlayer.cpp")
        self.assertIn("!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility", s)
        conf = read("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility", conf)
        # The original IP137 dependency gate MUST remain: real Core refresh
        # is not in Core6 and both TBC race floor and old pilot force OFF.
        reload = read("src/IndividualProgression.cpp")
        self.assertIn('LOG_ERROR("module", "F14 player observer pilot requires Core observer, cached WHO and immediate refresh hooks; forcing OFF")', reload)
        self.assertIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", reload)
        self.assertIn('IndividualProgression.F14.ForceVerifiedRandomBotTbcFloorOnLogin', reload)

    def test_human_altbot_and_unknown_do_not_get_guess_based_proof(self):
        s = read("src/IndividualProgressionPlayer.cpp")
        body = s[s.index("static CosmicIPF14Core6PositiveRndbot::Publication ReadVerifiedPublication("):
                 s.index("\npublic:\n#endif", s.index("static CosmicIPF14Core6PositiveRndbot::Publication ReadVerifiedPublication("))]
        self.assertIn("TryReadF14VerifiedActorPublication(", body)
        self.assertNotIn("IsRandomBot(", body)
        self.assertNotIn("isBotAccount(", body)
        self.assertNotIn("CharacterDatabase", body)
        self.assertNotIn("WorldDatabase", body)
        self.assertNotIn("SetPhaseMask(", body)

    def test_test_only_pure_header_has_no_core_or_network_dependency(self):
        h = read("src/IndividualProgressionF14Core6PositiveRndbotViewerPolicy.h")
        for required in (
            "confirmedNormalHumanAccount", "confirmedNonGM",
            "confirmedNotSelf", "effectiveStageKnown",
            "verifiedSelectedRandomBot", "fullGuidMatches",
            "currentAccountProven", "raceMatches",
            "nonzeroCurrentLoginEpoch", "actorHeadless",
            "FirstTbcStage = 8", "LastKnownStage = 18",
        ):
            self.assertIn(required, h)
        self.assertNotIn("WorldSession", h)
        self.assertNotIn("SendDirectMessage", h)
        self.assertNotIn("SaveToDB", h)


if __name__ == "__main__":
    unittest.main()
