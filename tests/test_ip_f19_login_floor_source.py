"""Source contracts for the F19 login floor on the default-OFF F14 login guard.

Pure rules are tested by tests/f14_login_preserve_policy.cpp; these checks
cover how they are wired into IndividualProgressionPlayer.cpp. Source checks,
not gameplay tests.

Run: python3 -B -m unittest discover -s tests -p 'test_ip_f19_login_floor_source.py'
"""
import re
import subprocess
import unittest
from pathlib import Path

from f14_stage_floor_source_normalize import (
    FROZEN_PARENT, strip_login_pilot, strip_policy_pilot,
)

ROOT = Path(__file__).resolve().parents[1]
PARENT_20 = "5fe0f9105d55d249aa55a25510d3fa23e3d84148"
BASE_IP_30 = "817ec290b5a82306bec76cdfcf32fc86c58c5da2"  # reviewed paid + warning child, exact SHA
REVIEWED_IP_32 = "6c180c0a78cdadcf6c79e8e627f2cc2e91e0fdae"  # frozen parent of paid-TBC #33
PLAYER = strip_login_pilot((ROOT / "src/IndividualProgressionPlayer.cpp").read_text())
POLICY = strip_policy_pilot((ROOT / "src/IndividualProgressionF14LoginPolicy.h").read_text())
LOGIN_ANCHOR = "            // F14 partial safeguard, OFF by default."
LOGIN_END = "        else // normal account"


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, check=True).stdout


def login_region(src: str) -> str:
    start = src.index(LOGIN_ANCHOR)
    return src[start:src.index(LOGIN_END, start)]


def flat(text: str) -> str:
    return " ".join(re.sub(r"//[^\n]*", "", text).split())


# Integration strict-scope checks use BASE_IP_30 instead of the standalone
# PR #24 baseline, because the paid-expansion source already changed other
# player callbacks. All F19 policy/wiring tests remain identical to #24.
class Provenance(unittest.TestCase):
    def test_parent_is_pr20_head(self):
        subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", PARENT_20, "HEAD"], check=True)

    def test_f19_original_source_scope_and_reviewed_child_ownership(self):
        # Freeze the F19 provenance check at its reviewed parent, not at
        # moving HEAD: #33 intentionally edits the separate paid-expansion
        # implementation. Never broaden this assertion to arbitrary sources.
        subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", REVIEWED_IP_32, "HEAD"], check=True)
        reviewed = sorted(git("diff", "--name-only", BASE_IP_30, REVIEWED_IP_32, "--", "src").split())
        self.assertEqual(reviewed, ["src/IndividualProgressionF14LoginPolicy.h", "src/IndividualProgressionPlayer.cpp"])
        # Freeze the post-F19 paid-era lineage at #34, then separately
        # verify the exact F14 pilot subtracts back to #34 byte-for-byte.
        changed_since_review = sorted(git("diff", "--name-only", REVIEWED_IP_32, FROZEN_PARENT, "--", "src").split())
        self.assertEqual(changed_since_review, [
            "src/IndividualProgressionExpansion.cpp",
            "src/IndividualProgressionExpansionPolicy.h",
        ])
        # The F19 login writer and pure policy must be byte-identical to the
        # frozen reviewed #32; paid-TBC changes cannot silently alter either.
        self.assertEqual(git("diff", "--name-only", REVIEWED_IP_32, FROZEN_PARENT, "--",
                             "src/IndividualProgressionF14LoginPolicy.h",
                             "src/IndividualProgressionPlayer.cpp").strip(), "")
        self.assertEqual(PLAYER, git("show", FROZEN_PARENT + ":src/IndividualProgressionPlayer.cpp"))
        self.assertEqual(POLICY, git("show", FROZEN_PARENT + ":src/IndividualProgressionF14LoginPolicy.h"))
        self.assertEqual(git("diff", "--name-only", BASE_IP_30, "HEAD", "--",
                             "data", "optional", "sql").strip(), "")


    def test_f14_conf_addition_preserves_all_previous_settings(self):
        # Grok Low-1: compare the entire distributed config against the
        # reviewed #34 parent after removing ONLY the new default-OFF block.
        # A flip to EnforceGroupRules or any other pre-existing default must
        # fail this check, even though the F14 pilot itself is test-only.
        path = "conf/individualProgression.conf.dist"
        current = (ROOT / path).read_text()
        begin = "\n# IndividualProgression.F14.EnforceVerifiedRandomBotTbcFloorOnLogin\n"
        end = "IndividualProgression.F14.EnforceVerifiedRandomBotTbcFloorOnLogin = 0\n"
        self.assertEqual(current.count(begin), 1)
        self.assertEqual(current.count(end), 1)
        before, remainder = current.split(begin, 1)
        new_comments, after = remainder.split(end, 1)
        # This new block is comment-only until its single expected OFF key.
        self.assertNotIn("\nIndividualProgression.", "\n" + new_comments)
        self.assertEqual(before + after, git("show", FROZEN_PARENT + ":" + path))


class Wiring(unittest.TestCase):
    def test_floor_condition_inside_existing_guard(self):
        region = flat(login_region(PLAYER))
        self.assertIn(
            "if (savedStage >= IndividualProgressionGroupPolicy::FirstTbcStage && "
            "savedStage <= IndividualProgressionGroupPolicy::LastSupportedStage && "
            "IndividualProgressionF14LoginPolicy::PreservesAtLeastLevelStage( "
            "savedStage, player->GetLevel(), sIndividualProgression->progressionLimit))", region)
        # Still evaluated only after the flag, race and headless checks.
        self.assertLess(region.index("f14PreserveVerifiedRandomBotStageOnLogin"), region.index("PreservesAtLeastLevelStage"))
        self.assertLess(region.index("IsHeadless()"), region.index("PreservesAtLeastLevelStage"))
        # The Playerbots identity check stays the last, innermost step.
        self.assertLess(region.index("PreservesAtLeastLevelStage"), region.index("sRandomPlayerbotMgr.IsRandomBot("))

    def test_original_level_write_unchanged(self):
        self.assertIn(
            "if (!preserveVerifiedRandomBotStage) { if (player->GetLevel() <= 60) "
            "sIndividualProgression->ForceUpdateProgressionState(player, static_cast<ProgressionState>(0)); "
            "else if ((player->GetLevel() > 60) && (player->GetLevel() <= 70)) "
            "sIndividualProgression->ForceUpdateProgressionState(player, static_cast<ProgressionState>(8)); "
            "else sIndividualProgression->ForceUpdateProgressionState(player, static_cast<ProgressionState>(13)); }",
            flat(login_region(PLAYER)))

    def test_policy_mirrors_original_level_write(self):
        self.assertIn("if (level <= 60)\n            return 0;", POLICY)
        self.assertIn("if (level <= 70)\n            return 8;", POLICY)
        self.assertIn("return 13;", POLICY)
        self.assertIn("if (savedStage < LevelStage(level))\n            return false;", POLICY)
        self.assertIn("if (progressionLimit > 0 && savedStage > progressionLimit)\n            return false;", POLICY)
        self.assertEqual(re.findall(r'#include [<"]([^>"]+)[>"]', POLICY), ["cstdint"])

    def test_include_added_and_comment_corrected(self):
        self.assertIn('#include "IndividualProgressionGroupPolicy.h"\n#include "IndividualProgressionF14LoginPolicy.h"\n', PLAYER)
        self.assertIn("itself backed by the mutable currentBots set", PLAYER)
        self.assertNotIn("Do not call the mutable currentBots-backed predicate", PLAYER)

    def test_diff_vs_parent_is_only_the_documented_lines(self):
        diff = git("diff", "-U0", BASE_IP_30, FROZEN_PARENT, "--", "src/IndividualProgressionPlayer.cpp")
        removed = [l[1:].strip() for l in diff.splitlines() if l.startswith("-") and not l.startswith("---")]
        self.assertEqual(removed, [
            "savedStage <= IndividualProgressionGroupPolicy::LastSupportedStage)",
            "// may not yet exist during Core's PlayerScript login.",
            "// Do not call the mutable currentBots-backed predicate",
            "// outside the source-verified WorldScript login thread.",
        ])
        added = [l[1:].strip() for l in diff.splitlines() if l.startswith("+") and not l.startswith("+++")]
        self.assertEqual(len(added), 10, added)
        self.assertNotIn("ForceUpdateProgressionState", "".join(added))


class WorkflowDeviation(unittest.TestCase):
    WF = ".github/workflows/ip19-gm-null-safety-integration-20261101.yml"

    def test_gm_workflow_only_scopes_its_frozen_step(self):
        diff = git("diff", "-U0", PARENT_20, "--", self.WF)
        added = [l for l in diff.splitlines() if l.startswith("+") and not l.startswith("+++")]
        removed = [l for l in diff.splitlines() if l.startswith("-") and not l.startswith("---")]
        self.assertEqual(removed, [])
        self.assertEqual(len(added), 5, added)
        self.assertIn("github.head_ref == 'integration/ip19-gm-null-safety-20261101'", "".join(added))


if __name__ == "__main__":
    unittest.main()
