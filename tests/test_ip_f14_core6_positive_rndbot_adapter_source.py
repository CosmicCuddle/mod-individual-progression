#!/usr/bin/env python3
"""F14 IP Core6 separate positive adapter test, original false-only unchanged.

Exact old-PASS/new-PASS on inherited IP136 negative-only regression is required.
No host, Core runtime, real packet, creature restoration or client ACK.
"""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
PARENT = "7ca897e2397216405f910f46e6a12e5b4e0b987b"
POSITIVE = "COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_VIEWER_POLICY_EXPERIMENT"
PUBLISHER = "COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT"


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def frozen(path):
    return subprocess.check_output(
        ["git", "show", f"{PARENT}:{path}"], cwd=ROOT
    ).decode("utf-8")


class F14OptedCore6PositiveAdapter(unittest.TestCase):
    def test_original_negative_class_and_player_cpp_byte_recoverable(self):
        old = frozen("src/IndividualProgressionPlayer.cpp")
        now = read("src/IndividualProgressionPlayer.cpp")
        self.assertNotIn(PUBLISHER, now)  # inherited IP136 strict source invariant
        self.assertNotIn("TryReadF14VerifiedActorPublication(", now)
        old_class = old.split("class IndividualProgressionF14Core6NegativeActorWhoAdapter final", 1)[1].split(
            "void AddSC_mod_individual_progression_player()", 1)[0]
        new_class = now.split("class IndividualProgressionF14Core6NegativeActorWhoAdapter final", 1)[1].split(
            "void AddSC_mod_individual_progression_player()", 1)[0]
        self.assertEqual(old_class, new_class)
        self.assertEqual(new_class.count("return false;"), 2)

        # Exclude ONLY a separate optional adapter include and registration
        # selection; every previous Worldserver source line stays byte-identical.
        start = now.index("// A separate C++ class, never the original false-only adapter")
        end = now.index("// Experimental Core6 ABI adapter ONLY.", start)
        restored = now[:start] + now[end:]
        register = (
            f"#if defined({POSITIVE}) && {POSITIVE} >= 1\n"
            "    new IndividualProgressionF14Core6PositiveRndbotAdapter();\n"
            "#else\n"
            "    new IndividualProgressionF14Core6NegativeActorWhoAdapter();\n"
            "#endif"
        )
        self.assertEqual(restored.count(register), 1)
        restored = restored.replace(
            register, "    new IndividualProgressionF14Core6NegativeActorWhoAdapter()")
        # The old line has its semicolon, keep full exact source.
        restored = restored.replace(
            "    new IndividualProgressionF14Core6NegativeActorWhoAdapter()\n",
            "    new IndividualProgressionF14Core6NegativeActorWhoAdapter();\n")
        self.assertEqual(restored, old)

    def test_positive_class_is_new_and_requires_all_three_explicit_macros(self):
        s = read("src/IndividualProgressionF14Core6PositiveRndbotAdapter.h")
        self.assertIn(f"#if !defined({POSITIVE})", s)
        self.assertIn(f"#if !defined({PUBLISHER})", s)
        self.assertIn("COSMIC_IP_F14_CORE6_ACTOR_WHO_NEGATIVE_ADAPTER_EXPERIMENT", s)
        self.assertIn("class IndividualProgressionF14Core6PositiveRndbotAdapter final", s)
        self.assertIn("WorldObjectScript", s)
        self.assertIn("OnF14ShouldSuppressPlayerForViewer(", s)
        self.assertIn("OnF14ShouldExcludeWhoEntryForViewer(", s)
        self.assertIn("CosmicIPF14Core6PositiveRndbot::HideActor(", s)
        self.assertIn("CosmicIPF14Core6PositiveRndbot::ExcludeWho(", s)
        self.assertNotIn("SetPhaseMask(", s)
        self.assertNotIn("SaveToDB(", s)

    def test_current_positive_login_proof_and_observer_checks(self):
        s = read("src/IndividualProgressionF14Core6PositiveRndbotAdapter.h")
        for needle in (
            "TryReadF14VerifiedActorPublication(",
            "published.fullGuid == targetGuid",
            "published.accountId != 0",
            "published.race == claimedRace",
            "published.loginEpoch != 0",
            "liveAccountId == published.accountId",
            "actorSession && actorSession->IsHeadless() && accountId != 0",
            "!actor->IsGameMaster()",
            "!viewer->IsInWorld() || viewer->IsGameMaster()",
            "sIndividualProgression->isNormalAccount(ordinaryViewer)",
            "GetEffectiveProgression(ordinaryViewer)",
            "effectiveStage <= IndividualProgressionGroupPolicy::LastSupportedStage",
        ):
            self.assertIn(needle, s)

    def test_ordinary_viewer_is_not_gm_account_even_with_gm_mode_off(self):
        path = "src/IndividualProgressionF14Core6PositiveRndbotAdapter.h"
        original = subprocess.check_output(
            ["git", "show", "a8d92ce4c867e53e6337765783956ce4f65e7022:" + path],
            cwd=ROOT,
        ).decode("utf-8")
        updated = read(path)
        # Real old-RED/new-GREEN, without modifying the owner-original pure
        # policy or assuming toggled GM mode establishes account privilege.
        self.assertNotIn("AccountMgr::IsPlayerAccount", original)
        self.assertNotIn("sIndividualProgression->isGMAccount(ordinaryViewer)", original)
        self.assertIn('#include "AccountMgr.h"', updated)
        start = updated.index("static bool IsConservativePositiveViewer(")
        end = updated.index("static CosmicIPF14Core6PositiveRndbot::Publication", start)
        guard = updated[start:end]
        self.assertIn(
            "!AccountMgr::IsPlayerAccount(viewer->GetSession()->GetSecurity())", guard)
        self.assertIn(
            "sIndividualProgression->isGMAccount(ordinaryViewer)", guard)
        self.assertIn("!viewer->IsInWorld() || viewer->IsGameMaster()", guard)
        self.assertIn("!sIndividualProgression->isNormalAccount(ordinaryViewer)", guard)
        self.assertLess(
            guard.index("AccountMgr::IsPlayerAccount("),
            guard.index("GetEffectiveProgression(ordinaryViewer)"))
        self.assertLess(
            guard.index("isGMAccount(ordinaryViewer)"),
            guard.index("GetEffectiveProgression(ordinaryViewer)"))
        self.assertIn("facts.confirmedNonGM = true;", guard)

    def test_who_is_cached_guid_race_only_and_never_target_player(self):
        s = read("src/IndividualProgressionF14Core6PositiveRndbotAdapter.h")
        start = s.index("bool OnF14ShouldExcludeWhoEntryForViewer(")
        end = s.index("\n    }", start)
        body = s[start:end]
        self.assertIn("ReadVerifiedPublication(", body)
        self.assertIn("static_cast<uint8>(cachedRace), 0", body)
        for forbidden in (
            "ObjectAccessor::", "FindPlayer(", "targetGuid->",
            "sRandomPlayerbotMgr", "WorldDatabase", "CharacterDatabase",
        ):
            self.assertNotIn(forbidden, body)

    def test_off_default_and_original_refresh_gate_unchanged(self):
        original = frozen("src/IndividualProgression.cpp")
        now = read("src/IndividualProgression.cpp")
        self.assertEqual(now, original)
        conf = read("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", conf)
        self.assertIn('LOG_ERROR("module", "F14 player observer pilot requires Core observer, cached WHO and immediate refresh hooks; forcing OFF")', now)
        self.assertIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", now)

    def test_pure_policy_uses_only_positive_rndbot_conditions(self):
        h = read("src/IndividualProgressionF14Core6PositiveRndbotViewerPolicy.h")
        for key in (
            "confirmedNormalHumanAccount", "confirmedNonGM", "confirmedNotSelf",
            "effectiveStageKnown", "verifiedSelectedRandomBot",
            "fullGuidMatches", "currentAccountProven", "raceMatches",
            "nonzeroCurrentLoginEpoch", "actorHeadless",
            "FirstTbcStage = 8", "LastKnownStage = 18",
        ):
            self.assertIn(key, h)
        self.assertNotIn("WorldSession", h)
        self.assertNotIn("SendDirectMessage", h)


if __name__ == "__main__":
    unittest.main()
