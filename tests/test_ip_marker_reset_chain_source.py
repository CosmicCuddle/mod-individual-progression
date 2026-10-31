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
    def test_playerbots_checkout_is_exact_clean_pin(self):
        root = sources().roots["pb"]
        self.assertEqual(x.git_head(root), x.PINNED_PLAYERBOTS_SHA,
                         f"{root} must be a git checkout of mod-playerbots@{x.PINNED_PLAYERBOTS_SHA}")
        self.assertTrue(x.git_is_clean(root), f"{root} has local modifications or untracked files")

    def test_core_checkout_is_exact_clean_pin(self):
        root = sources().roots["core"]
        self.assertEqual(x.git_head(root), x.PINNED_CORE_SHA,
                         f"{root} must be a git checkout of azerothcore-wotlk@{x.PINNED_CORE_SHA}")
        self.assertTrue(x.git_is_clean(root), f"{root} has local modifications or untracked files")


class ConfigNamespaceAudit(unittest.TestCase):
    """CS02-IP finding 1 / CS01-IP item 2: AiPlayerbot.* -> Playerbots.* rename (review-only comparison).

    The legacy checkout (PINNED_PLAYERBOTS_LEGACY_ROOT @ 037c0141...) is read ONLY for key names and
    defaults. It is not the harness input and not evidence of the owner's installed revision.
    """

    @classmethod
    def setUpClass(cls):
        root = os.environ.get("PINNED_PLAYERBOTS_LEGACY_ROOT")
        if not root:
            if os.environ.get("REQUIRE_LEGACY_CONFIG_AUDIT") == "1":
                raise AssertionError("REQUIRE_LEGACY_CONFIG_AUDIT=1 but PINNED_PLAYERBOTS_LEGACY_ROOT is unset")
            raise unittest.SkipTest("legacy PB checkout not provided; namespace audit not run")
        cls.root = Path(root)
        cls.legacy = {k: (cls.root / v).read_text(encoding="utf-8", errors="replace")
                      for k, v in x.LEGACY_PB_FILES.items()}

    def test_legacy_checkout_is_exact_clean_pin(self):
        self.assertEqual(x.git_head(self.root), x.LEGACY_PLAYERBOTS_SHA)
        self.assertTrue(x.git_is_clean(self.root))
        self.assertNotEqual(self.root.resolve(), sources().roots["pb"].resolve(),
                            "legacy audit must use a distinct checkout from the primary pin")

    def test_key_namespace_renamed_with_same_defaults(self):
        legacy = x.legacy_config_facts(self.legacy["config"], self.legacy["conf_dist"])
        primary = x.legacy_config_facts(sources().pb["config"], sources().pb["conf_dist"])
        self.assertEqual(legacy, {"RandomBotMaxLevel": ("AiPlayerbot", "80", "80"),
                                  "DowngradeMaxLevelBot": ("AiPlayerbot", "true", "0")})
        self.assertEqual(primary, {"RandomBotMaxLevel": ("Playerbots", "80", "80"),
                                   "DowngradeMaxLevelBot": ("Playerbots", "true", "0")})

    def test_legacy_pin_also_clamps_to_world_max_level(self):
        self.assertIn("if (randomBotMaxLevel > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL)) "
                      "randomBotMaxLevel = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);",
                      x.normalize_ws(x.mask_comments(self.legacy["config"])))

    def test_stale_legacy_conf_under_primary_pin_silently_enables_downgrade(self):
        """A legacy-style 'AiPlayerbot.DowngradeMaxLevelBot = 0' line is not read at e88cc409,
        so the effective value is the code default. Derived from source; owner config UNKNOWN."""
        primary = x.legacy_config_facts(sources().pb["config"], sources().pb["conf_dist"])
        stale_conf = "AiPlayerbot.DowngradeMaxLevelBot = 0\nAiPlayerbot.RandomBotMaxLevel = 60\n"
        ns, code_default, _ = primary["DowngradeMaxLevelBot"]
        present = re.findall(r"(?m)^\s*" + ns + r"\.DowngradeMaxLevelBot\s*=\s*(\S+)", stale_conf)
        effective = present[0] if present else code_default
        self.assertEqual(effective, "true")
        model = x.randomize_decision_model(sources())
        # Bot at 60, worldserver MaxPlayerLevel 60 (clamp) -> full reset; MaxPlayerLevel 80 -> incremental.
        self.assertEqual(model.outcome(level=60, cls=1, random_bot_max_level=60, downgrade_max_level_bot=True),
                         "full_reset")
        self.assertEqual(model.outcome(level=60, cls=1, random_bot_max_level=80, downgrade_max_level_bot=True),
                         "incremental")


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
    ("pb_initself_rare_made_incremental", "pb", "playerbot_mgr",
     in_function("PlayerbotHolder::HandlePlayerbotCommand", sub_once(
         "ITEM_QUALITY_RARE);\n                factory.Randomize(false);",
         "ITEM_QUALITY_RARE);\n                factory.Randomize(true);")),
     "check_pb_gm_initself_full_reset_routes"),
    ("pb_initself_uncommon_extra_gate", "pb", "playerbot_mgr",
     in_function("PlayerbotHolder::HandlePlayerbotCommand", sub_once(
         "if (master->CanBeGameMaster())\n            {\n                // OnBotLogin(master);\n"
         "                PlayerbotFactory factory(master, master->GetLevel(), ITEM_QUALITY_UNCOMMON);",
         "if (master->CanBeGameMaster() && !HasSavedIpStage(master))\n            {\n                // OnBotLogin(master);\n"
         "                PlayerbotFactory factory(master, master->GetLevel(), ITEM_QUALITY_UNCOMMON);")),
     "check_pb_gm_initself_full_reset_routes"),
    ("pb_level_mgr_reset_bot_incremental", "pb", "level_mgr",
     in_function("RandomBotLevelMgr::ResetBot", sub_once("newFactory.Randomize(false);", "newFactory.Randomize(true);")),
     "check_pb_level_mgr_full_reset_routes_default_off"),
    ("pb_level_mgr_login_hook_ungated", "pb", "level_mgr",
     sub_once("        if (!sPlayerbotAIConfig.ResetBotLevelEnabled)\n            return;\n"
              "        RandomBotLevelMgr::instance().OnBotLogin(player);",
              "        RandomBotLevelMgr::instance().OnBotLogin(player);"),
     "check_pb_level_mgr_full_reset_routes_default_off"),
    ("pb_level_mgr_new_unconditional_caller", "pb", "level_mgr",
     in_function("RandomBotLevelMgr::Update", lambda t: t.replace("{", "{\n    SkipBotLevel(nullptr, 0);", 1)),
     "check_pb_level_mgr_full_reset_routes_default_off"),
    ("pb_level_brackets_code_default_on", "pb", "config",
     sub_once('GetOption<bool>("Playerbots.LevelBrackets.Enabled", false)',
              'GetOption<bool>("Playerbots.LevelBrackets.Enabled", true)'),
     "check_pb_level_mgr_full_reset_routes_default_off"),
    ("ip_vanilla_human_xp_cap_moved", "ip", "ip_player",
     sub_once("PROGRESSION_PRE_TBC) && player->GetLevel() >= 60)", "PROGRESSION_PRE_TBC) && player->GetLevel() >= 70)"),
     "check_ip_level_caps_are_xp_gates_not_world_cap"),
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


REMOVE_CALL = "        bot->RemoveRewardedQuest(entry);\n"

# CS02-IP review finding 4: guards that wrap the removal WITHOUT a new 'continue'.
GUARDED_REMOVAL_FIXTURES: List[Tuple[str, str]] = [
    ("braced_tracking_flag_guard",
     "        if (!quest->HasFlag(QUEST_FLAGS_TRACKING))\n        {\n    " + REMOVE_CALL + "        }\n"),
    ("braced_ip_marker_range_guard",
     "        if (entry < IP_STAGE_MARKER_FIRST || entry > IP_STAGE_MARKER_LAST)\n        {\n    "
     + REMOVE_CALL + "        }\n"),
    ("braceless_tracking_flag_guard",
     "        if (!quest->HasFlag(QUEST_FLAGS_TRACKING))\n    " + REMOVE_CALL),
]


class NegativeMutationFixtures(unittest.TestCase):
    """The harness must notice each deliberate break of the recorded chain."""

    def test_guarded_removal_rejected_for_the_intended_reason(self):
        for name, replacement in GUARDED_REMOVAL_FIXTURES:
            with self.subTest(fixture=name):
                mutated = sources().with_edit(
                    "pb", "factory", in_function("PlayerbotFactory::ResetQuests", sub_once(REMOVE_CALL, replacement)))
                with self.assertRaises(x.ContractViolation) as ctx:
                    x.check_pb_reset_quests_removes_every_non_none_quest(mutated)
                self.assertEqual(str(ctx.exception), x.MSG_REMOVE_NOT_UNCONDITIONAL)

    def test_top_level_statement_splitter(self):
        block = ("a = 1; if (b) c(); if (d) { e(); } else { f(); } g(\";}\"); "
                 "for (int i = 0; i < 2; ++i) { h(); } k();")
        self.assertEqual(x.top_level_statements(block), [
            "a = 1;", "if (b) c();", "if (d) { e(); } else { f(); }", 'g(";}");',
            "for (int i = 0; i < 2; ++i) { h(); }", "k();"])

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

    def _sql_findings(self, sql: str):
        tree = dict(sources().ip_sql_tree)
        tree["data/sql/world/base/zz_fixture.sql"] = sql
        return x.run_all(replace(sources(), ip_sql_tree=tree))["check_no_other_ip_sql_redefines_markers"]

    def test_column_less_insert_on_marker_is_detected(self):
        err = self._sql_findings("INSERT INTO `quest_template_addon` VALUES (66004, 0, 0, 1);\n")
        self.assertIsNotNone(err)
        self.assertIn("column-less", err)

    def test_column_less_insert_on_unrelated_id_is_not_flagged(self):
        self.assertIsNone(self._sql_findings("REPLACE INTO quest_template_addon VALUES (9999, 0, 0, 1);\n"))

    def test_insert_select_on_marker_table_fails_closed(self):
        err = self._sql_findings(
            "INSERT INTO `quest_template_addon` SELECT * FROM `quest_template_addon_backup` WHERE `ID` > 1;\n")
        self.assertIsNotNone(err)
        self.assertIn("UNPARSEABLE", err)

    def test_conditional_preprocessor_in_extracted_body_fails_closed(self):
        for name, group, key, fn, old in (
                ("if0_around_removal", "pb", "factory", "PlayerbotFactory::ResetQuests", REMOVE_CALL),
                ("ifdef_in_get_quest_status", "core", "player_quest", "Player::GetQuestStatus",
                 "    return QUEST_STATUS_NONE;\n")):
            with self.subTest(fixture=name):
                wrapped = "#if 0\n" + old + "#endif\n"
                mutated = sources().with_edit(group, key, in_function(fn, sub_once(old, wrapped)))
                with self.assertRaises(x.ExtractionError) as ctx:
                    x.find_function(getattr(mutated, group)[key], fn)
                self.assertIn("conditional preprocessor '#if'", str(ctx.exception))
                failing = [k for k, v in x.run_all(mutated).items() if v]
                self.assertTrue(failing, f"{name}: no contract failed")

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

class TreeScanFixtures(unittest.TestCase):
    """Fixtures for contracts that scan whole src/ trees rather than one file."""

    def test_legacy_option_key_anywhere_in_pb_src_is_detected(self):
        tree = dict(sources().pb_src_tree)
        tree["src/Script/LegacyConfigShim.cpp"] = (
            'bool d = sConfigMgr->GetOption<bool>("AiPlayerbot.DowngradeMaxLevelBot", true);\n')
        with self.assertRaises(x.ContractViolation):
            x.check_pb_no_legacy_config_namespace(replace(sources(), pb_src_tree=tree))

    def test_legacy_key_in_comment_is_not_a_finding(self):
        tree = dict(sources().pb_src_tree)
        tree["src/Script/Note.cpp"] = "// previously AiPlayerbot.DowngradeMaxLevelBot\n"
        x.check_pb_no_legacy_config_namespace(replace(sources(), pb_src_tree=tree))

    def test_partial_pb_src_checkout_fails_closed(self):
        with self.assertRaises(x.ContractViolation):
            x.check_pb_no_legacy_config_namespace(replace(sources(), pb_src_tree={}))

    def test_ip_touching_world_max_level_is_detected(self):
        tree = dict(sources().ip_src_tree)
        tree["src/IndividualProgressionCap.cpp"] = "uint32 c = sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL);\n"
        with self.assertRaises(x.ContractViolation):
            x.check_ip_level_caps_are_xp_gates_not_world_cap(replace(sources(), ip_src_tree=tree))


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

    def test_tracking_flag_guard_is_caught_for_the_right_reason(self):
        """CS02 finding 4: the guard must COMPILE (verbatim Quest::HasFlag/QuestFlags) and
        fail on behaviour (markers carry Flags=1024 TRACKING and survive), not on a build error."""
        run = self._build_and_run("tracking-guard", (x.RESET_SKIP_TRACKING_FLAG_MUTATION,))
        self.assertEqual(run.returncode, 1, run.stdout)
        self.assertRegex(run.stdout, r"not ok \d+ - stage 8: every rewarded stage marker removed from m_RewardedQuests")
        self.assertRegex(run.stdout, r"(?m)^ok \d+ - ordinary rewarded quest also deleted")


if __name__ == "__main__":
    unittest.main()
