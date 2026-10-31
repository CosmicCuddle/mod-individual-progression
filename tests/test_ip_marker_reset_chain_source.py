#!/usr/bin/env python3
"""Regression harness: Playerbots full randomisation vs IP stage markers 66001..66018.

TEST-ONLY. Reads exact pinned public sources; writes only to a temp dir.

Evidence levels (never conflate them):
  A  SOURCE CONTRACT   - ip_marker_reset_extract.check_* on pinned text.
  B  COMPILED BODIES   - verbatim pinned function bodies compiled with stand-in
                         types and executed (tests/marker_reset_harness).
  -  NOT COVERED       - Worldserver runtime, a real character DB, the owner's
                         installed Core/Playerbots/IP revisions or configs.

Environment:
  PINNED_PLAYERBOTS_ROOT  mod-playerbots checkout at e88cc409...
  PINNED_CORE_ROOT        Grimfeather/azerothcore-wotlk checkout at 8daffdab...
  PINNED_IP_ROOT          IP checkout under test (default: this repository)
  CXX                     C++ compiler for layer B (default: g++, then clang++)
  REQUIRE_COMPILED_HARNESS=1  fail (instead of skip) when no compiler exists

Run:  python3 -m unittest discover -s tests -p 'test_ip_marker_reset_chain_source.py' -v
"""
from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from typing import Callable, List, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ip_marker_reset_extract as x  # noqa: E402

HARNESS_DIR = Path(__file__).resolve().parent / "marker_reset_harness"
EXPECTED_COMPILED_EXPECTATIONS = 86

_SOURCES = None


def sources() -> x.Sources:
    global _SOURCES
    if _SOURCES is None:
        _SOURCES = x.load_sources()
    return _SOURCES


# --------------------------------------------------------------------------
# Pinned revision gate
# --------------------------------------------------------------------------

class PinnedRevisions(unittest.TestCase):
    def test_playerbots_checkout_is_exact_pin(self):
        root = sources().roots["pb"]
        self.assertEqual(x.git_head(root), x.PINNED_PLAYERBOTS_SHA,
                         f"{root} must be a git checkout of mod-playerbots@{x.PINNED_PLAYERBOTS_SHA}")

    def test_core_checkout_is_exact_pin(self):
        root = sources().roots["core"]
        self.assertEqual(x.git_head(root), x.PINNED_CORE_SHA,
                         f"{root} must be a git checkout of azerothcore-wotlk@{x.PINNED_CORE_SHA}")


# --------------------------------------------------------------------------
# Layer A: one test per source contract
# --------------------------------------------------------------------------

class SourceContracts(unittest.TestCase):
    """Each check_* must pass on the unmodified pinned sources."""


def _make_contract_test(check: Callable[[x.Sources], None]):
    def test(self):
        check(sources())
    test.__doc__ = (check.__doc__ or check.__name__).strip().splitlines()[0]
    return test


for _check in x.ALL_CHECKS:
    setattr(SourceContracts, "test_" + _check.__name__[len("check_"):], _make_contract_test(_check))


class DerivedFacts(unittest.TestCase):
    """Human-readable facts the coordination thread relies on, derived from source."""

    def test_trigger_matrix_from_pinned_randomize_text(self):
        model = x.randomize_decision_model(sources())
        dk = x.CLASS_DEATH_KNIGHT
        # Shipped playerbots.conf.dist (RandomBotMaxLevel=80, DowngradeMaxLevelBot=0):
        self.assertEqual(model.outcome(level=60, cls=1, random_bot_max_level=80, downgrade_max_level_bot=False),
                         "incremental")
        # MaxPlayerLevel=60 clamps RandomBotMaxLevel to 60; key ABSENT -> code default true:
        self.assertEqual(model.outcome(level=60, cls=1, random_bot_max_level=60, downgrade_max_level_bot=True),
                         "full_reset")
        self.assertEqual(model.outcome(level=2, cls=1, random_bot_max_level=80, downgrade_max_level_bot=False),
                         "full_reset")
        self.assertEqual(model.outcome(level=55, cls=dk, random_bot_max_level=80, downgrade_max_level_bot=False),
                         "full_reset")

    def test_config_default_differs_between_conf_dist_and_code(self):
        facts = x.playerbots_config_facts(sources())
        self.assertEqual(facts.conf_dist["DowngradeMaxLevelBot"], "0")
        self.assertEqual(facts.code_default["DowngradeMaxLevelBot"], "true")
        self.assertTrue(facts.max_level_clamped_to_world_cap)

    def test_pvp_markers_are_distinct_from_stage_markers(self):
        pvp = x.ip_marker_templates(sources(), x.PVP_MARKER_RANGE)
        self.assertTrue(pvp, "expected PvP marker rows 661xx in player_progression.sql")
        self.assertFalse(set(pvp) & set(x.EXPECTED_STAGE_MARKERS))


# --------------------------------------------------------------------------
# Negative mutation fixtures: each must flip the named contract
# --------------------------------------------------------------------------

def sub_once(old: str, new: str) -> Callable[[str], str]:
    def apply(text: str) -> str:
        n = text.count(old)
        if n != 1:
            raise AssertionError(f"fixture anchor occurs {n} times: {old!r}")
        return text.replace(old, new)
    return apply


def in_function(name: str, transform: Callable[[str], str]) -> Callable[[str], str]:
    """Apply ``transform`` only inside the single definition of ``name``."""
    def apply(text: str) -> str:
        fn = x.find_function(text, name)
        start = text.index(fn.text)
        return text[:start] + transform(fn.text) + text[start + len(fn.text):]
    return apply


def sql_row_edit(row_prefix: str, old: str, new: str) -> Callable[[str], str]:
    """Replace the first ``old`` after the unique ``row_prefix`` occurrence."""
    def apply(text: str) -> str:
        if text.count(row_prefix) != 1:
            raise AssertionError(f"row prefix occurs {text.count(row_prefix)} times: {row_prefix!r}")
        i = text.index(row_prefix)
        j = text.index(old, i)
        return text[:j] + new + text[j + len(old):]
    return apply


# (fixture name, group, file key, transform, contract expected to fail)
MUTATIONS: List[Tuple[str, str, str, Callable[[str], str], str]] = [
    ("pb_reset_skips_ip_marker_range", "pb", "factory",
     in_function("PlayerbotFactory::ResetQuests", sub_once(
         "uint32 entry = quest->GetQuestId();",
         "uint32 entry = quest->GetQuestId();\n        if (entry >= 66001 && entry <= 66018) continue;")),
     "check_pb_reset_quests_removes_every_non_none_quest"),
    ("pb_reset_called_on_incremental_randomize", "pb", "factory",
     in_function("PlayerbotFactory::Randomize", lambda t: sub_once(
         "    ClearInventory();\n", "    ResetQuests();\n    ClearInventory();\n")(
             sub_once("        ResetQuests();\n", "")(t))),
     "check_pb_full_randomize_calls_reset_only_when_not_incremental"),
    ("pb_randomize_first_call_commented_out", "pb", "mgr",
     in_function("RandomPlayerbotMgr::RandomizeFirst", sub_once(
         "factory.Randomize(false);", "// factory.Randomize(false);")),
     "check_pb_full_reset_entry_points"),
    ("pb_clear_handler_commented_out", "pb", "mgr",
     sub_once('handlers["clear"] = &RandomPlayerbotMgr::Clear;', '// handlers["clear"] = &RandomPlayerbotMgr::Clear;'),
     "check_pb_full_reset_entry_points"),
    ("pb_initmin_handler_enabled", "pb", "mgr",
     sub_once('// handlers["initmin"]', 'handlers["initmin"]'),
     "check_pb_full_reset_entry_points"),
    ("pb_clear_everything_without_reset", "pb", "factory",
     in_function("PlayerbotFactory::ClearEverything", sub_once("    ResetQuests();\n", "")),
     "check_pb_clear_everything_route"),
    ("pb_downgrade_negation_flipped", "pb", "mgr",
     in_function("RandomPlayerbotMgr::Randomize", sub_once(
         "|| !sPlayerbotAIConfig.DowngradeMaxLevelBot", "|| sPlayerbotAIConfig.DowngradeMaxLevelBot")),
     "check_pb_trigger_matrix"),
    ("pb_conf_dist_downgrade_enabled", "pb", "conf_dist",
     sub_once("Playerbots.DowngradeMaxLevelBot = 0", "Playerbots.DowngradeMaxLevelBot = 1"),
     "check_pb_config_exposure_defaults"),
    ("pb_max_level_clamp_removed", "pb", "config",
     sub_once("        RandomBotMaxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);\n", ""),
     "check_pb_config_exposure_defaults"),
    ("core_get_status_ignores_repeatable", "core", "player_quest",
     in_function("Player::GetQuestStatus", sub_once(
         "if (!qInfo->IsRepeatable() && IsQuestRewarded(quest_id))", "if (IsQuestRewarded(quest_id))")),
     "check_core_get_quest_status_rewarded_semantics"),
    ("core_remove_rewarded_no_db_delete", "core", "player_quest",
     in_function("Player::RemoveRewardedQuest", sub_once("        m_RewardedQuestsSave[questId] = false;\n", "")),
     "check_core_reward_and_remove_bookkeeping"),
    ("core_save_loop_no_delete_statement", "core", "player_storage",
     in_function("Player::_SaveQuestStatus", sub_once(
         "GetPreparedStatement(CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST)",
         "GetPreparedStatement(CHAR_INS_CHAR_QUESTSTATUS_REWARDED)")),
     "check_core_save_persists_rewarded_delete"),
    ("core_monthly_repeatable_normalisation_removed", "core", "object_mgr",
     lambda t: (lambda a, b, c: a + c)(*t.rpartition("qinfo->SpecialFlags |= QUEST_SPECIAL_FLAGS_REPEATABLE;")),
     "check_core_loader_normalisation"),
    ("ip_sql_marker_66008_repeatable", "ip", "progression_sql",
     sub_once("(66008, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 256)",
              "(66008, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 257)"),
     "check_ip_sql_markers_are_rewarded_status_quests"),
    ("ip_sql_marker_66003_daily_flag", "ip", "progression_sql",
     sql_row_edit("(66003, 2, ", ", 1024,", ", 5120,"),
     "check_ip_sql_markers_are_rewarded_status_quests"),
    ("ip_sql_marker_66005_seasonal_sort", "ip", "progression_sql",
     sub_once("(66005, 2, 60, 60, 0,", "(66005, 2, 60, 60, -22,"),
     "check_ip_sql_markers_are_rewarded_status_quests"),
    ("ip_reads_shifted_marker_ids", "ip", "ip_cpp",
     in_function("IndividualProgression::GetPlayerProgressionFromQuests", sub_once("66000 + i", "67000 + i")),
     "check_ip_reads_exactly_the_sql_markers"),
]


class NegativeMutationFixtures(unittest.TestCase):
    """The harness must notice each deliberate break of the recorded chain."""

    def test_baseline_has_no_failures(self):
        failures = {k: v for k, v in x.run_all(sources()).items() if v}
        self.assertEqual(failures, {})

    def test_every_mutation_is_detected_by_its_contract(self):
        for name, group, key, transform, expected in MUTATIONS:
            with self.subTest(fixture=name):
                mutated = sources().with_edit(group, key, transform)
                results = x.run_all(mutated)
                self.assertIsNotNone(results[expected],
                                     f"{name}: {expected} still passes on mutated source")

    def test_foreign_sql_redefining_a_marker_is_detected(self):
        tree = dict(sources().ip_sql_tree)
        tree["data/sql/world/base/zz_negative_fixture.sql"] = (
            "UPDATE `quest_template_addon` SET `SpecialFlags` = 1 WHERE `ID` = 66004;\n")
        results = x.run_all(replace(sources(), ip_sql_tree=tree))
        self.assertIsNotNone(results["check_no_other_ip_sql_redefines_markers"])

    def test_foreign_sql_on_unrelated_ids_is_not_flagged(self):
        tree = dict(sources().ip_sql_tree)
        tree["data/sql/world/base/zz_control_fixture.sql"] = (
            "UPDATE `quest_template_addon` SET `SpecialFlags` = 1 WHERE `ID` = 9999;\n"
            "DELETE FROM `quest_template` WHERE `ID` BETWEEN 66101 AND 66114;\n")
        results = x.run_all(replace(sources(), ip_sql_tree=tree))
        self.assertIsNone(results["check_no_other_ip_sql_redefines_markers"])


# --------------------------------------------------------------------------
# Extractor self-tests on synthetic input
# --------------------------------------------------------------------------

class ExtractorSelfTests(unittest.TestCase):
    def test_comments_masked_but_strings_kept(self):
        src = 'int a; // x()\nconst char* s = "// not a comment"; /* y() */ char c = \'"\';\n'
        masked = x.mask_comments(src)
        self.assertNotIn("x()", masked)
        self.assertNotIn("y()", masked)
        self.assertIn('"// not a comment"', masked)
        self.assertEqual(len(masked), len(src))

    def test_find_function_requires_single_definition(self):
        src = "void A::f()\n{\n    g(); // f();\n}\nvoid B() { A::f(); }\n"
        fn = x.find_function(src, "A::f")
        self.assertEqual(x.normalize_ws(fn.body), "g();")
        with self.assertRaises(x.ExtractionError):
            x.find_function(src + "void A::f() {}\n", "A::f")

    def test_sql_parser_handles_quotes_and_comments(self):
        sql = ("-- c;\nINSERT INTO `t` (`ID`, `Name`, `V`) VALUES (1, 'it''s; ok', -2),\n"
               "(2, \"a, \\\"b\\\"\", NULL); /* ; */ DELETE FROM t WHERE ID = 3;")
        stmts = list(x.iter_sql_statements(sql))
        self.assertEqual(len(stmts), 2)
        ins = x.parse_insert(stmts[0])
        self.assertEqual(ins.rows[0], {"ID": 1, "Name": "it's; ok", "V": -2})
        self.assertEqual(ins.rows[1], {"ID": 2, "Name": 'a, "b"', "V": None})

    def test_condition_translator_rejects_unmodelled_symbols(self):
        with self.assertRaises(x.ContractViolation):
            x.cpp_condition_to_python("bot->GetLevel() < 3 || system(\"x\")")


# --------------------------------------------------------------------------
# Layer B: compiled extracted bodies
# --------------------------------------------------------------------------

def _compiler() -> str | None:
    for cand in (os.environ.get("CXX"), "g++", "clang++"):
        if cand and shutil.which(cand):
            return cand
    return None


class CompiledExtractedBodies(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cxx = _compiler()
        if cls.cxx is None:
            if os.environ.get("REQUIRE_COMPILED_HARNESS") == "1":
                raise AssertionError("REQUIRE_COMPILED_HARNESS=1 but no C++ compiler found")
            raise unittest.SkipTest("no C++ compiler; layer B not run")
        cls.tmp = Path(tempfile.mkdtemp(prefix="ip-marker-reset-"))

    @classmethod
    def tearDownClass(cls):
        if getattr(cls, "tmp", None):
            shutil.rmtree(cls.tmp, ignore_errors=True)

    def _build_and_run(self, label: str, mutations=()) -> subprocess.CompletedProcess:
        out = self.tmp / label
        x.generate_harness_sources(sources(), out, mutations)
        exe = out / "harness"
        build = subprocess.run(
            [self.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(out), "-I", str(HARNESS_DIR),
             str(HARNESS_DIR / "harness_main.cpp"), "-o", str(exe)],
            capture_output=True, text=True)
        self.assertEqual(build.returncode, 0, f"compile failed ({label}):\n{build.stderr}")
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
        sys.stderr.write(f"\n--- compiled harness [{label}] via {self.cxx} (exit {run.returncode}) ---\n")
        sys.stderr.write("\n".join(l for l in run.stdout.splitlines() if l.startswith(("#", "not ok", "1.."))) + "\n")
        return run

    def test_pinned_bodies_delete_markers_and_queue_db_delete(self):
        run = self._build_and_run("pinned")
        self.assertEqual(run.returncode, 0, run.stdout)
        self.assertIn(f"1..{EXPECTED_COMPILED_EXPECTATIONS}", run.stdout)
        self.assertEqual(len(re.findall(r"(?m)^ok \d+ ", run.stdout)), EXPECTED_COMPILED_EXPECTATIONS)
        self.assertNotIn("not ok", run.stdout)

    def test_generated_bodies_are_verbatim_pinned_text(self):
        out = self.tmp / "verbatim"
        x.generate_harness_sources(sources(), out)
        bodies = (out / "pinned_bodies.inc").read_text()
        reset = x.find_function(sources().pb["factory"], "PlayerbotFactory::ResetQuests")
        status = x.find_function(sources().core["player_quest"], "Player::GetQuestStatus")
        self.assertIn(reset.text, bodies)
        self.assertIn(status.text, bodies)
        self.assertNotIn("NEGATIVE FIXTURE", bodies)

    def test_mutated_reset_quests_is_caught(self):
        run = self._build_and_run("mutated", (x.RESET_SKIP_IP_MARKERS_MUTATION,))
        self.assertEqual(run.returncode, 1, run.stdout)
        self.assertIn("not ok", run.stdout)
        self.assertRegex(run.stdout, r"not ok \d+ - stage 8: IP reads stage 0 after pinned ResetQuests")


if __name__ == "__main__":
    unittest.main()
