#!/usr/bin/env python3
"""Combined #16 (Hunter + expansion grouping + C3 offline-leader guard) plus the
#13 default-OFF partial F14 saved-stage login safeguard: source and model tests.

SYNTHETIC / SOURCE EVIDENCE ONLY. Nothing here launches a Worldserver, touches a
database or proves gameplay. The native compile and link is a separate CI job.

Parents (immutable, verified through git objects):
  #16 integration/hunter-grouping-offline-leader-guard-20261031  c44c3320...
  #13 feature/f14-preserve-saved-rndbot-login-guard-20261031     a18397bb...
"""
from __future__ import annotations

import re
import subprocess
import unittest
from itertools import product
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT_16 = "c44c3320406670cdc8f25c963ebba13e9cf24b71"
SOURCE_13 = "a18397bbb0c7054052030a83ac30b699ac382d38"
F14_KEY = "IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin"

# Child PRs may add ONLY the documented F19 login floor (one extra condition,
# one include and a corrected comment). Revert exactly those lines so every
# assertion below stays as strict as on #19 itself; anything else still fails.
F19_SUBSTITUTIONS = (
    ('#include "IndividualProgressionF14LoginPolicy.h"\n', ""),
    ("                // F19: never keep a stage below the original level write\n"
     "                // (<=60 -> 0, 61-70 -> 8, 71+ -> 13) or above a set limit.\n", ""),
    ("savedStage <= IndividualProgressionGroupPolicy::LastSupportedStage &&\n"
     "                    IndividualProgressionF14LoginPolicy::PreservesAtLeastLevelStage(\n"
     "                        savedStage, player->GetLevel(), sIndividualProgression->progressionLimit))",
     "savedStage <= IndividualProgressionGroupPolicy::LastSupportedStage)"),
    ("                    // may not yet exist during Core's PlayerScript login. It is\n"
     "                    // itself backed by the mutable currentBots set, which is\n"
     "                    // only safe on the source-verified WorldScript login thread;\n"
     "                    // do not call it from other hooks or threads.\n",
     "                    // may not yet exist during Core's PlayerScript login.\n"
     "                    // Do not call the mutable currentBots-backed predicate\n"
     "                    // outside the source-verified WorldScript login thread.\n"),
)


def without_f19_login_floor(src: str) -> str:
    # Strip only the distinct, bounded F14 stage-floor pilot when this old
    # historical source test runs on a later composed IP child. This test
    # still verifies that original #13/#19 login is unchanged beneath it.
    start = "            // F14 NEW owner-priority SOURCE PILOT, default OFF."
    end = "            // END F14 new verified RandomBot stage-floor block.\n"
    if start in src:
        before, rest = src.split(start, 1)
        if end not in rest:
            raise AssertionError("F14 stage-floor cleanup marker missing")
        src = before + rest.split(end, 1)[1].lstrip("\n")
        src = src.replace(
            "if (!preserveVerifiedRandomBotStage && !verifiedRandomBotTbcFloorHandled)",
            "if (!preserveVerifiedRandomBotStage)",
            1,
        )
    for new_text, old_text in F19_SUBSTITUTIONS:
        src = src.replace(new_text, old_text)
    return src


PLAYER = without_f19_login_floor((ROOT / "src/IndividualProgressionPlayer.cpp").read_text())
CORE = (ROOT / "src/IndividualProgression.cpp").read_text()
# The legacy #13 port test compares the historical configuration block,
# so remove only the new F14 pilot read/absent-PB-handler from that comparison.
# All new F14 source guards are verified in test_ip_f14_tbc_floor_source.py.
CORE_F14_HISTORICAL = CORE.replace(
    '        sIndividualProgression->f14EnforceVerifiedRandomBotTbcFloorOnLogin = sConfigMgr->GetOption<bool>(\n'
    '            "IndividualProgression.F14.EnforceVerifiedRandomBotTbcFloorOnLogin", false);\n', ""
).replace(
    '        if (sIndividualProgression->f14EnforceVerifiedRandomBotTbcFloorOnLogin)\n'
    '        {\n'
    '            LOG_ERROR("module", "Individual Progression F14 TBC RandomBot floor requested without Playerbots header: forcing this incomplete feature OFF");\n'
    '            sIndividualProgression->f14EnforceVerifiedRandomBotTbcFloorOnLogin = false;\n'
    '        }\n', ""
)
HEADER = (ROOT / "src/IndividualProgression.h").read_text()
CONF = (ROOT / "conf/individualProgression.conf.dist").read_text()


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, check=True).stdout


def git_show(rev: str, path: str) -> str:
    return git("show", f"{rev}:{path}")


def balanced_block(src: str, anchor: str) -> str:
    """Text from ``anchor`` through the matching closing brace of its first ``{``."""
    i = src.index(anchor)
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        depth += src[k] == "{"
        depth -= src[k] == "}"
        if depth == 0:
            return src[i:k + 1]
    raise AssertionError(f"unbalanced block after {anchor!r}")


LOGIN_ANCHOR = "            // F14 partial safeguard, OFF by default."
LOGIN_END = "        else // normal account"


def without_paid_expansion_group_reads(src: str) -> str:
    """Reverse ONLY the documented paid-expansion group substitutions made by a
    child PR (IP-11D: effective stage in enforcement-ON era checks). Anything
    else that differs from the parents still fails the strict comparisons."""
    for comment in (
        "        // Paid expansion (default OFF): eras compare by effective stage, so an\n"
        "        // unpaid stage-13 character groups as TBC. Identical to raw when OFF.\n",
        "            // Paid expansion (default OFF): compare eras by effective stage.\n"
        "            uint8 const joiningEra = sIndividualProgression->GetEffectiveProgression(player);\n"
        "            uint8 const leaderEra = sIndividualProgression->GetEffectiveProgression(leader);\n",
    ):
        src = src.replace(comment, "")
    src = src.replace("joiningEra", "joiningState").replace("leaderEra", "leaderState")
    src = src.replace("GetAccountEffectiveProgression(", "GetAccountProgression(")
    return src.replace("GetEffectiveProgression(", "GetPlayerProgressionFromQuests(")


def login_region(src: str) -> str:
    start = src.index(LOGIN_ANCHOR)
    return src[start:src.index(LOGIN_END, start)]


# ---------------------------------------------------------------------------
# Model of the ported guard (mirrors the source conditions asserted below).
# ---------------------------------------------------------------------------
RACE_BLOODELF, RACE_DRAENEI, RACE_HUMAN = 10, 11, 1


def preserves_saved_stage(enabled, playerbots_header, race, headless, is_random_bot, saved_stage):
    return (enabled and playerbots_header and race in (RACE_BLOODELF, RACE_DRAENEI) and headless
            and 8 <= saved_stage <= 18 and is_random_bot)


def login_write_without_guard(level):
    """The ORIGINAL non-normal-account login write that runs whenever the guard does not preserve."""
    return 0 if level <= 60 else (8 if level <= 70 else 13)


class PinnedParents(unittest.TestCase):
    def test_parents_are_reachable_git_objects(self):
        for rev in (PARENT_16, SOURCE_13):
            self.assertEqual(git("cat-file", "-t", rev).strip(), "commit", rev)

    def test_branch_is_a_child_of_exact_16(self):
        subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", PARENT_16, "HEAD"], check=True)


class PreservedParentSource(unittest.TestCase):
    """Everything outside the claimed port must be byte-identical to #16."""

    def test_untouched_executable_files_identical_to_16(self):
        for path in ("src/IndividualProgressionSpells.cpp",
                     "src/IndividualProgressionGroupPolicy.h", "src/IndividualProgression_loader.cpp"):
            with self.subTest(path=path):
                self.assertEqual(git("diff", "--name-only", PARENT_16, "--", path).strip(), "", path)

    def test_gm_source_is_exactly_the_previously_reviewed_fix(self):
        # This child preserves #14's GM fix, with CS01's separately reviewed self-only get and paid-mode set audit. All other executable
        # protections remain identical to frozen #19.
        self.assertEqual(git("rev-parse", "HEAD:src/cs_individualProgression.cpp").strip(),
                         "278aaa48dc5fb7770bd6013e659e4872752d4b03")

    def test_hunter_blob_is_the_reviewed_one(self):
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionSpells.cpp").strip(),
                         "6ee93e1174b5aef6c5d61c9ea31526506c9d08da")

    def test_no_sql_or_data_change(self):
        self.assertEqual(git("diff", "--name-only", PARENT_16, "--", "data", "optional").strip(), "")

    def test_c3_group_accept_callback_byte_identical_to_16(self):
        anchor = "bool OnPlayerCanGroupAccept"
        self.assertEqual(balanced_block(without_paid_expansion_group_reads(PLAYER), anchor),
                         balanced_block(git_show(PARENT_16, "src/IndividualProgressionPlayer.cpp"), anchor))

    def test_c3_leader_guard_precedes_both_quest_reads(self):
        block = re.sub(r"//[^\n]*", "", balanced_block(without_paid_expansion_group_reads(PLAYER), "bool OnPlayerCanGroupAccept"))  # code only
        guard = block.index("if (!leader || !leader->IsInWorld())")
        self.assertIn("return !sIndividualProgression->enforceGroupRules;", block[guard:guard + 120])
        reads = [m.start() for m in re.finditer(r"GetPlayerProgressionFromQuests\(", block)]
        self.assertEqual(len(reads), 3, "joining, leader and per-member reads")
        self.assertTrue(all(r > guard for r in reads), "a quest-stage read happens before the C3 leader guard")
        self.assertNotIn("ForceUpdateProgressionState", block[:guard])

    def test_only_two_hunks_changed_in_player_cpp(self):
        import difflib
        parent = git_show(PARENT_16, "src/IndividualProgressionPlayer.cpp").splitlines()
        current = without_paid_expansion_group_reads(PLAYER).splitlines()
        hunks = [l for l in difflib.unified_diff(parent, current, n=0, lineterm="") if l.startswith("@@")]
        self.assertEqual(len(hunks), 2, hunks)


class PortedFromSource13(unittest.TestCase):
    """The port must be semantically identical to #13, not a rewrite."""

    def test_login_region_verbatim_from_13(self):
        self.assertEqual(login_region(PLAYER), login_region(git_show(SOURCE_13, "src/IndividualProgressionPlayer.cpp")))

    def test_optional_playerbots_include_block_verbatim_from_13(self):
        def head(src):
            return src[:src.index("static_assert(PROGRESSION_PRE_TBC")]
        self.assertEqual(head(PLAYER), head(git_show(SOURCE_13, "src/IndividualProgressionPlayer.cpp")))

    def test_config_read_and_header_member_from_13(self):
        src13 = git_show(SOURCE_13, "src/IndividualProgression.cpp")
        block = re.search(r'        sIndividualProgression->f14PreserveVerifiedRandomBotStageOnLogin = .*?#endif\n', src13, re.S).group(0)
        self.assertEqual(CORE_F14_HISTORICAL.count(block), 1)
        self.assertIn("enforceGroupRules, f14PreserveVerifiedRandomBotStageOnLogin, f14EnforceVerifiedRandomBotTbcFloorOnLogin, EnableSetRepCommand", HEADER)

    def test_conf_block_from_13(self):
        conf13 = git_show(SOURCE_13, "conf/individualProgression.conf.dist")
        block = conf13[conf13.index("# " + F14_KEY):conf13.index(F14_KEY + " = 0") + len(F14_KEY + " = 0")]
        self.assertEqual(CONF.count(block), 1)


class DefaultOffAndFailClosed(unittest.TestCase):
    def test_default_off_in_conf_and_code(self):
        self.assertIn(F14_KEY + " = 0", CONF)
        self.assertIn(f'GetOption<bool>("{F14_KEY}", false)', CORE)

    def test_missing_playerbots_header_forces_flag_off_with_error(self):
        self.assertIn('#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")', CORE)
        self.assertIn("sIndividualProgression->f14PreserveVerifiedRandomBotStageOnLogin = false;", CORE)
        self.assertIn('LOG_ERROR("module", "IndividualProgression F14 saved-RandomBot login safeguard requested,', CORE)

    def test_required_build_fails_without_playerbots(self):
        self.assertIn("defined(COSMIC_IP_F14_REQUIRE_PLAYERBOTS) && !COSMIC_IP_F14_PLAYERBOTS_AVAILABLE", PLAYER)
        self.assertIn('#error "F14 login guard: required Playerbots RandomPlayerbotMgr API was not found"', PLAYER)

    def test_guard_compiled_only_with_playerbots(self):
        region = login_region(PLAYER)
        self.assertLess(region.index("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE"), region.index("sRandomPlayerbotMgr.IsRandomBot"))


class GuardConditions(unittest.TestCase):
    def test_exact_preservation_predicate(self):
        flat = " ".join(login_region(PLAYER).split())
        for piece in (
            "bool preserveVerifiedRandomBotStage = false;",
            "if (sIndividualProgression->f14PreserveVerifiedRandomBotStageOnLogin && "
            "(player->getRace() == RACE_DRAENEI || player->getRace() == RACE_BLOODELF) && "
            "player->GetSession() && player->GetSession()->IsHeadless())",
            "uint8 const savedStage = sIndividualProgression->GetPlayerProgressionFromQuests(player);",
            "if (savedStage >= IndividualProgressionGroupPolicy::FirstTbcStage && "
            "savedStage <= IndividualProgressionGroupPolicy::LastSupportedStage)",
            "preserveVerifiedRandomBotStage = sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter());",
        ):
            self.assertIn(piece, flat)

    def test_original_level_writes_unchanged_when_not_preserving(self):
        flat = " ".join(login_region(PLAYER).split())
        self.assertIn(
            "if (!preserveVerifiedRandomBotStage) { if (player->GetLevel() <= 60) "
            "sIndividualProgression->ForceUpdateProgressionState(player, static_cast<ProgressionState>(0)); "
            "else if ((player->GetLevel() > 60) && (player->GetLevel() <= 70)) "
            "sIndividualProgression->ForceUpdateProgressionState(player, static_cast<ProgressionState>(8)); "
            "else sIndividualProgression->ForceUpdateProgressionState(player, static_cast<ProgressionState>(13)); }",
            flat)

    def test_guard_never_writes_a_stage(self):
        region = login_region(PLAYER)
        guarded = region[:region.index("if (!preserveVerifiedRandomBotStage)")]
        self.assertNotIn("ForceUpdateProgressionState", guarded)
        self.assertNotIn("UpdateProgressionState", guarded)
        self.assertNotIn("RewardQuest", guarded)

    def test_stage_bounds_are_8_to_18(self):
        policy = (ROOT / "src/IndividualProgressionGroupPolicy.h").read_text()
        self.assertRegex(policy, r"FirstTbcStage\s*=\s*8\b")
        self.assertRegex(policy, r"LastSupportedStage\s*=\s*18\b")


class SyntheticMatrix(unittest.TestCase):
    """SYNTHETIC truth table for the ported predicate; not gameplay evidence."""

    def test_only_the_narrow_case_preserves(self):
        preserved = []
        for enabled, header, race, headless, rndbot, stage in product(
                (False, True), (False, True), (RACE_HUMAN, RACE_BLOODELF, RACE_DRAENEI),
                (False, True), (False, True), range(0, 19)):
            if preserves_saved_stage(enabled, header, race, headless, rndbot, stage):
                preserved.append((race, stage))
        # Exactly: flag ON, header present, headless, positive RandomBot, race 10/11, saved stage 8..18.
        self.assertEqual(len(preserved), 2 * 11)
        self.assertTrue(all(race in (RACE_BLOODELF, RACE_DRAENEI) and 8 <= stage <= 18 for race, stage in preserved))

    def test_off_baseline_identical_to_parent_behaviour(self):
        for race, headless, rndbot, stage, level in product(
                (RACE_HUMAN, RACE_BLOODELF, RACE_DRAENEI), (False, True), (False, True), range(0, 19), (1, 60, 61, 70, 71, 80)):
            if not preserves_saved_stage(False, True, race, headless, rndbot, stage):
                self.assertIn(login_write_without_guard(level), (0, 8, 13))
            else:
                self.fail("flag OFF must never preserve")

    def test_named_negative_cases(self):
        cases = {
            "flag_off_bloodelf_rndbot_stage12": (False, True, RACE_BLOODELF, True, True, 12),
            "human_race_rndbot_stage12": (True, True, RACE_HUMAN, True, True, 12),
            "non_headless_bloodelf_stage12": (True, True, RACE_BLOODELF, False, True, 12),
            "not_random_bot_altbot_or_unknown": (True, True, RACE_DRAENEI, True, False, 12),
            "unsaved_stage0": (True, True, RACE_BLOODELF, True, True, 0),
            "vanilla_stage7": (True, True, RACE_DRAENEI, True, True, 7),
            "playerbots_header_missing": (True, False, RACE_BLOODELF, True, True, 12),
        }
        for name, args in cases.items():
            with self.subTest(case=name):
                self.assertFalse(preserves_saved_stage(*args))

    def test_named_positive_cases(self):
        for stage in (8, 12, 18):
            for race in (RACE_BLOODELF, RACE_DRAENEI):
                with self.subTest(race=race, stage=stage):
                    self.assertTrue(preserves_saved_stage(True, True, race, True, True, stage))


class WorkflowDeviation(unittest.TestCase):
    """The ONE edit to #16's own workflow: scope only its frozen delta step to #16's head."""

    WF = ".github/workflows/ip-combined-offline-leader-source-20261031.yml"

    def test_only_frozen_step_scoped(self):
        src = (ROOT / self.WF).read_text()
        step = src.split("      - name: Strict six-file delta vs original combined Hunter/grouping parent\n", 1)[1]
        step = step.split("        shell: bash", 1)[0]
        self.assertIn("github.head_ref == 'integration/hunter-grouping-offline-leader-guard-20261031'", step)
        self.assertIn("github.ref == 'refs/heads/integration/hunter-grouping-offline-leader-guard-20261031'", step)
        header = src.split("  frozen-combined-source-and-policy:", 1)[1].split("    steps:", 1)[0]
        self.assertNotIn("    if:", header, "the whole regression job must not be skipped")
        for regression in ("test_ip_combined_offline_leader_guard.py", "python3 tests/check_hunter_autocast_source.py",
                           "python3 tests/check_group_expansion_source.py", "tests/group_expansion_policy.cpp"):
            self.assertIn(regression, src)

    def test_workflow_diff_vs_16_adds_only_the_if(self):
        diff = git("diff", "-U0", PARENT_16, "--", self.WF)
        added = [l[1:] for l in diff.splitlines() if l.startswith("+") and not l.startswith("+++")]
        removed = [l for l in diff.splitlines() if l.startswith("-") and not l.startswith("---")]
        self.assertEqual(removed, [])
        self.assertEqual(len(added), 5)
        self.assertTrue(added[0].strip().startswith("#") and added[1].strip().startswith("#"))
        self.assertEqual(added[2].strip(), "if: >-")


class NewWorkflowContract(unittest.TestCase):
    """This branch's own workflow: pinned native link with the PB-required define, never launched."""

    WF = ".github/workflows/ip-combined-c3-f14-partial-login-20261031.yml"

    def setUp(self):
        self.src = (ROOT / self.WF).read_text()

    def test_pins_and_define(self):
        for pin in ("8daffdabe9202c228e45d833c9a1d399597a15cd", "e88cc4099e69f35126b28942a412ca702d20453b",
                    "80e9de1c39a6dc995188a80c31a5e785d0f2a6f9", PARENT_16, SOURCE_13):
            self.assertIn(pin, self.src)
        self.assertIn('-DCMAKE_CXX_FLAGS="-DCOSMIC_IP_F14_REQUIRE_PLAYERBOTS"', self.src)
        self.assertIn("--target worldserver", self.src)
        self.assertNotIn("RandomBotStagePolicy", self.src, "#11's header is not part of #16")

    def test_own_freeze_scoped_and_no_launch_or_upload(self):
        step = self.src.split('      - name: "Strict nine-file delta vs combined PR 16 parent"\n', 1)[1].split("        shell: bash", 1)[0]
        self.assertIn("github.head_ref == 'integration/hunter-group-c3-f14-partial-login-20261031'", step)
        for job in ("  combined-source-and-negative-regression:", "  full-native-off-by-default:"):
            header = self.src.split(job, 1)[1].split("    steps:", 1)[0]
            self.assertNotIn("    if:", header)
        for forbidden in ("upload-artifact", "./worldserver", "apps/worldserver &", "secrets."):
            self.assertNotIn(forbidden, self.src)


if __name__ == "__main__":
    unittest.main()
