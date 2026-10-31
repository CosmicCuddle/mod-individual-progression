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

    def test_reset_function_changes_outside_the_loop_rejected_for_the_intended_reason(self):
        """CS02-IP rev-2 Low: early return / post-loop restore outside the template loop."""
        early = sub_once("    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)",
                         "    if (HasIpStage(bot))\n        return;\n    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)")
        restore = lambda t: t[:t.rindex("}")] + "    RestoreIpStageMarkers(bot);\n}"
        for name, transform in (("early_return", early), ("post_loop_restore", restore)):
            with self.subTest(fixture=name):
                mutated = sources().with_edit("pb", "factory", in_function("PlayerbotFactory::ResetQuests", transform))
                with self.assertRaises(x.ContractViolation) as ctx:
                    x.check_pb_reset_quests_removes_every_non_none_quest(mutated)
                self.assertEqual(str(ctx.exception), x.MSG_RESET_FUNCTION_CHANGED)

    def test_initself_contract_tolerates_variable_rename_but_counts_any_factory_on_master(self):
        rename = in_function("PlayerbotHolder::HandlePlayerbotCommand", sub_once(
            "PlayerbotFactory factory(master, master->GetLevel(), ITEM_QUALITY_RARE);\n                factory.Randomize(false);",
            "PlayerbotFactory rareFactory(master, master->GetLevel(), ITEM_QUALITY_RARE);\n                rareFactory.Randomize(false);"))
        x.check_pb_gm_initself_full_reset_routes(sources().with_edit("pb", "playerbot_mgr", rename))
        extra = in_function("PlayerbotHolder::HandlePlayerbotCommand", sub_once(
            'if (!strcmp(cmd, "initself"))', 'PlayerbotFactory sneaky(master, 1);\n    if (!strcmp(cmd, "initself"))'))
        with self.assertRaises(x.ContractViolation):
            x.check_pb_gm_initself_full_reset_routes(sources().with_edit("pb", "playerbot_mgr", extra))

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
        try:
            x.check_no_other_ip_sql_redefines_markers(replace(sources(), ip_sql_tree=tree))
        except x.ContractViolation as exc:
            return str(exc)
        return None

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

    # CS02-IP rev-2 finding (Medium): former false negatives. Each must be reported, for the stated reason.
    SQL_GATE_NEGATIVES = [
        ("update_without_where", "UPDATE `quest_template_addon` SET `SpecialFlags` = 1;", "no WHERE"),
        ("range_predicate", "UPDATE quest_template_addon SET SpecialFlags = 1 WHERE ID > 66000;",
         "WHERE matches stage-marker row(s) 66001..66018 (18)"),
        ("in_subquery", "UPDATE quest_template SET Flags = 4096 WHERE ID IN (SELECT ID FROM quest_backup);",
         "not statically decidable"),
        ("lower_case_id_insert", "INSERT INTO quest_template_addon (id, SpecialFlags) VALUES (66004, 1);",
         "INSERT/REPLACE of stage-marker ID(s) [66004]"),
        ("delete_by_sort_id", "DELETE FROM quest_template WHERE QuestSortID = 0;",
         "DELETE FROM quest_template: WHERE matches stage-marker row(s)"),
        ("truncate", "TRUNCATE TABLE quest_template_addon;", "TRUNCATE on a marker table"),
        ("where_only_inside_string", "UPDATE quest_template SET Flags = 4096, LogTitle = 'x WHERE ID = 9999';",
         "no WHERE"),
        ("multi_table_chain_write",
         "UPDATE quest_template q JOIN quest_template p ON p.ID = q.ID SET q.Flags = p.Flags WHERE q.ID = 9999;",
         "multi-table UPDATE writing chain column(s)"),
        ("insert_without_id_column", "INSERT INTO quest_template_addon (SpecialFlags) VALUES (1);",
         "without exactly one ID column"),
    ]
    # Must stay clean: literal non-stage IDs, PvP markers 66101..66114, and broad writes of columns
    # the chain does not read (this exact statement exists in vanilla_quest_reputations.sql and its
    # WHERE matches all 18 marker rows).
    SQL_GATE_CONTROLS = [
        ("literal_other_id", "UPDATE quest_template_addon SET SpecialFlags = 1 WHERE ID = 9999;"),
        ("pvp_range", "UPDATE quest_template_addon SET SpecialFlags = 1 WHERE ID BETWEEN 66101 AND 66114;"),
        ("pvp_in_list", "UPDATE `quest_template` SET `Flags` = 0 WHERE `ID` IN (66101, 66102);"),
        ("broad_irrelevant_column", "UPDATE quest_template SET RewardFactionOverride1 = 0 WHERE RewardFactionId1=0;"),
        ("marker_table_only_read", "UPDATE creature_template SET x = 1 WHERE entry IN (SELECT ID FROM quest_template);"),
        ("not_between_stage", "DELETE FROM quest_template WHERE ID = 9999 AND NOT ID BETWEEN 66001 AND 66018;"),
    ]

    # Revision 4, CS02-IP N1: cross-statement dependencies (same file). Each reason is asserted.
    SQL_DEPENDENCY_NEGATIVES = [
        ("quest_template_two_stage",
         "UPDATE quest_template SET RewardFactionOverride1=123 WHERE ID BETWEEN 66001 AND 66018;\n"
         "UPDATE quest_template SET Flags=5120 WHERE RewardFactionOverride1=123;",
         "WHERE depends on column 'rewardfactionoverride1'"),
        ("quest_template_addon_two_stage",
         "UPDATE quest_template_addon SET MaxLevel=77 WHERE ID BETWEEN 66001 AND 66018;\n"
         "UPDATE quest_template_addon SET SpecialFlags=0 WHERE MaxLevel=77;",
         "WHERE depends on column 'maxlevel'"),
        ("three_step_chain",
         "UPDATE quest_template SET RewardFactionOverride2=5 WHERE ID BETWEEN 66001 AND 66018;\n"
         "UPDATE quest_template SET RewardFactionOverride3=6 WHERE RewardFactionOverride2=5;\n"
         "UPDATE quest_template SET Flags=5120 WHERE RewardFactionOverride3=6;",
         "WHERE depends on column 'rewardfactionoverride3'"),
        ("dependent_delete",
         "UPDATE quest_template SET RewardFactionOverride1=123 WHERE ID BETWEEN 66001 AND 66018;\n"
         "DELETE FROM quest_template WHERE RewardFactionOverride1=123;",
         "DELETE quest_template (statement #2): WHERE depends on column 'rewardfactionoverride1'"),
    ]
    # Revision 4, CS02-IP N2 / N4a.
    SQL_REV4_NEGATIVES = [
        ("executable_comment", "/*!50000 UPDATE quest_template_addon SET SpecialFlags=1 */;",
         "MySQL executable comment naming a marker table"),
        ("create_trigger",
         "CREATE TRIGGER t BEFORE UPDATE ON quest_template_addon FOR EACH ROW SET NEW.SpecialFlags = 1;",
         "stored/dynamic SQL, fail closed"),
        ("prepare_dynamic_sql", "PREPARE s FROM 'UPDATE quest_template_addon SET SpecialFlags = 1';",
         "PREPARE naming a marker table"),
        ("set_id_to_stage_literal", "UPDATE quest_template SET ID = 66001 WHERE ID = 9999;",
         "SET ID = 66001 assigns a stage-marker ID"),
        ("set_id_from_expression", "UPDATE quest_template SET ID = ID + 1 WHERE ID = 9999;",
         "SET ID from an expression"),
    ]
    SQL_REV4_CONTROLS = [
        ("broad_irrelevant_then_literal_other_id",
         "UPDATE quest_template SET RewardFactionOverride1 = 0 WHERE RewardFactionId1=0;\n"
         "UPDATE quest_template SET Flags = 4096 WHERE ID = 9999;"),
        # RewardHonor is used because the REAL baseline already makes RewardFactionOverride1..5 and
        # AllowableRaces mutable on marker rows (see test_baseline_mutable_columns_are_documented).
        ("mutator_limited_to_other_id",
         "UPDATE quest_template SET RewardHonor=123 WHERE ID = 9999;\n"
         "UPDATE quest_template SET Flags=5120 WHERE RewardHonor=123;"),
        ("mutator_limited_to_pvp_ids",
         "UPDATE quest_template SET RewardHonor=123 WHERE ID BETWEEN 66101 AND 66114;\n"
         "UPDATE quest_template SET Flags=5120 WHERE RewardHonor=123;"),
        ("set_id_to_pvp_literal", "UPDATE quest_template SET ID = 66102 WHERE ID = 9999;"),
        ("ordinary_comment", "/* UPDATE quest_template_addon SET SpecialFlags=1 */ SELECT 1;"),
        ("keywords_only_inside_quest_text",
         "UPDATE quest_template SET QuestDescription = 'Create a potion; prepare well' WHERE ID = 9999;"),
    ]

    def test_sql_dependency_negatives_report_the_intended_reason(self):
        for name, sql, reason in self.SQL_DEPENDENCY_NEGATIVES:
            with self.subTest(fixture=name):
                rows = x.ip_marker_rows(sources())
                self.assertEqual(x.marker_sql_statement_findings(sql + "\n", rows), [],
                                 f"{name}: must be invisible to the per-statement gate (that is the N1 gap)")
                standalone = " | ".join(x.marker_sql_findings(sql + "\n", rows))
                self.assertIn(reason, standalone)
                self.assertIn("which <input> statement #", standalone)  # attributed to the fixture itself
                err = self._sql_findings(sql + "\n")
                self.assertIsNotNone(err, f"{name}: not reported")
                self.assertIn(reason, err)
                self.assertIn("cross-statement dependency, fail closed", err)

    def test_sql_dependency_split_across_two_files_is_detected(self):
        tree = dict(sources().ip_sql_tree)
        tree["data/sql/world/base/zz_fixture_a.sql"] = (
            "UPDATE quest_template SET RewardHonor=123 WHERE ID BETWEEN 66001 AND 66018;\n")
        tree["optional/sql/world/zz_fixture_b.sql"] = (
            "UPDATE quest_template SET Flags=5120 WHERE RewardHonor=123;\n")
        rows = x.ip_marker_rows(sources())
        for rel in ("data/sql/world/base/zz_fixture_a.sql", "optional/sql/world/zz_fixture_b.sql"):
            self.assertEqual(x.marker_sql_findings(tree[rel], rows), [], f"{rel} alone must look clean")
        with self.assertRaises(x.ContractViolation) as ctx:
            x.check_no_other_ip_sql_redefines_markers(replace(sources(), ip_sql_tree=tree))
        msg = str(ctx.exception)
        self.assertIn("optional/sql/world/zz_fixture_b.sql: UPDATE quest_template (statement #1): "
                      "WHERE depends on column 'rewardhonor', which data/sql/world/base/zz_fixture_a.sql", msg)

    def test_baseline_mutable_columns_are_documented(self):
        """The real tree's UPDATEs that may touch stage-marker rows write only non-chain columns.
        A future chain write filtering on any of these columns fails closed BY DESIGN."""
        rows = x.ip_marker_rows(sources())
        mutable = set()
        for rel, text in sources().ip_sql_tree.items():
            if rel == x.IP_FILES["progression_sql"]:
                continue
            for w in x._file_marker_writes(rel, text):
                if w.verb == "UPDATE" and x._touches_original_rows(w, rows):
                    mutable |= {(t, c) for t in w.tables for c in w.set_cols}
        self.assertEqual(mutable, {("quest_template", c) for c in (
            "allowableraces", "rewardfactionoverride1", "rewardfactionoverride2", "rewardfactionoverride3",
            "rewardfactionoverride4", "rewardfactionoverride5")})
        self.assertFalse({c for _, c in mutable} & set().union(*x.CHAIN_COLUMNS.values()))

    # Revision 5: CS02-IP F1/F2 and self-review S1/S2/S3. Each reason is asserted.
    SQL_REV5_NEGATIVES = [
        ("load_data", "LOAD DATA INFILE 'x.csv' REPLACE INTO TABLE quest_template;",
         "unsupported statement form 'LOAD' naming a marker table"),
        ("load_xml", "LOAD XML INFILE 'x.xml' REPLACE INTO TABLE quest_template_addon;",
         "unsupported statement form 'LOAD' naming a marker table"),
        ("cte_update_with_subselect",
         "WITH c AS (SELECT 66001 AS i) UPDATE quest_template SET Flags=0 WHERE ID IN (SELECT i FROM c);",
         "unsupported statement form 'WITH' naming a marker table"),
        ("cte_update_plain", "WITH s AS (SELECT 1) UPDATE quest_template SET Flags=0;",
         "unsupported statement form 'WITH' naming a marker table"),
        ("odku_assigns_stage_id",
         "INSERT INTO quest_template (ID,Flags) VALUES (90000,0) ON DUPLICATE KEY UPDATE ID=66001;",
         "ON DUPLICATE KEY UPDATE assigns ID = 66001 (a stage-marker ID)"),
        ("odku_assigns_id_expression",
         "INSERT INTO quest_template (ID,Flags) VALUES (90000,0) ON DUPLICATE KEY UPDATE ID=ID+1;",
         "ON DUPLICATE KEY UPDATE assigns ID from an expression"),
        ("odku_writes_chain_column",
         "INSERT INTO quest_template_addon (ID,SpecialFlags) VALUES (90000,0) "
         "ON DUPLICATE KEY UPDATE SpecialFlags=VALUES(SpecialFlags);",
         "ON DUPLICATE KEY UPDATE writes chain column(s) ['specialflags']"),
        ("ansi_quoted_update", 'UPDATE "quest_template" SET Flags = 0;',
         'double-quoted marker-table identifier "quest_template" in UPDATE'),
        ("ansi_quoted_insert", 'INSERT INTO "quest_template_addon" (ID, SpecialFlags) VALUES (9999, 0);',
         'double-quoted marker-table identifier "quest_template_addon" in INSERT'),
    ]
    SQL_REV5_CONTROLS = [
        ("set_variable_from_select",  # this form exists in zz_optional_custom_races.sql
         "SET @BK := (SELECT `AllowableRaces` FROM `quest_template` WHERE `ID` = 9681);"),
        ("plain_select", "SELECT ID FROM quest_template WHERE ID = 66001;"),
        ("handler_read", "HANDLER quest_template OPEN;"),
        ("explain_select", "EXPLAIN SELECT * FROM quest_template;"),
        ("marker_name_only_in_string", "UPDATE creature_template SET subname = 'quest_template notes' WHERE entry = 1;"),
        ("marker_name_in_set_value_string", "UPDATE quest_template SET LogTitle = 'quest_template' WHERE ID = 9999;"),
        ("odku_non_chain_values_fn",
         "INSERT INTO quest_template_addon (ID, MaxLevel) VALUES (9999, 60) ON DUPLICATE KEY UPDATE MaxLevel = VALUES(MaxLevel);"),
        ("plain_unrelated_insert", "INSERT INTO quest_template_addon (ID, SpecialFlags) VALUES (9999, 0);"),
        ("pvp_insert", "INSERT INTO quest_template_addon (ID, SpecialFlags) VALUES (66101, 256);"),
    ]

    # Revision 6: CS02-IP G1 (double-quoted marker names anywhere) and G2 (EXPLAIN ANALYZE / write verb).
    G1_REASON = "(ANSI_QUOTES; effective SQL mode unverified) (fail closed)"
    G2_REASON = "with ANALYZE or a write verb on a marker table (engine behaviour unverified) (fail closed)"
    SQL_REV6_NEGATIVES = [
        ("g1_join", 'UPDATE creature_template c JOIN "quest_template" q ON q.ID=c.entry SET q.Flags=0;',
         'double-quoted marker-table identifier "quest_template" in UPDATE ' + G1_REASON),
        ("g1_delete_alias", 'DELETE q FROM "quest_template" q WHERE q.ID=66001;',
         'double-quoted marker-table identifier "quest_template" in DELETE ' + G1_REASON),
        ("g1_load", 'LOAD DATA INFILE \'x\' REPLACE INTO TABLE "quest_template";',
         'double-quoted marker-table identifier "quest_template" in LOAD ' + G1_REASON),
        ("g1_truncate", 'TRUNCATE TABLE "quest_template_addon";',
         'double-quoted marker-table identifier "quest_template_addon" in TRUNCATE ' + G1_REASON),
        ("g1_rename", 'RENAME TABLE "quest_template" TO old;',
         'double-quoted marker-table identifier "quest_template" in RENAME ' + G1_REASON),
        ("g1_schema_qualified", 'UPDATE "world"."quest_template" SET Flags=0;',
         'double-quoted marker-table identifier "quest_template" in UPDATE ' + G1_REASON),
        ("g2_explain_analyze_update", "EXPLAIN ANALYZE UPDATE quest_template SET Flags=0 WHERE ID=66001;",
         "EXPLAIN " + G2_REASON),
        ("g2_explain_analyze_join",
         "EXPLAIN ANALYZE UPDATE quest_template q JOIN quest_template_addon a ON a.ID=q.ID SET a.SpecialFlags=0;",
         "EXPLAIN " + G2_REASON),
        ("g2_explain_update", "EXPLAIN UPDATE quest_template SET Flags=0;", "EXPLAIN " + G2_REASON),
        ("g2_format_analyze_delete", "EXPLAIN FORMAT=TREE ANALYZE DELETE FROM quest_template WHERE ID=66001;",
         "EXPLAIN " + G2_REASON),
    ]
    SQL_REV6_CONTROLS = [
        ("explain_select", "EXPLAIN SELECT * FROM quest_template;"),
        ("explain_write_other_table", "EXPLAIN UPDATE creature_template SET x=1;"),
        ("describe_marker_table", "DESCRIBE quest_template;"),
        ("single_quoted_marker_name", "UPDATE creature_template SET subname = 'quest_template' WHERE entry = 1;"),
        # 74 real baseline UPDATEs carry double-quoted quest text like this; they must stay clean.
        ("double_quoted_quest_text",
         'UPDATE `quest_template` SET `LogDescription` = "Collect 4 Frostmaul Shards." WHERE `ID` = 9999;'),
        ("read_only_double_quoted_name", 'SELECT ID FROM "quest_template";'),
    ]

    # Revision 7: CS02-IP H1 (MariaDB SET STATEMENT ... FOR), H2 (EXPLAIN modifiers), H3 (locked FPs).
    H1_REASON = "MariaDB SET STATEMENT executes wrapped SQL (engine unverified; fail closed)"
    SQL_REV7_NEGATIVES = [
        ("h1_set_statement_delete",
         "SET STATEMENT max_statement_time=60 FOR DELETE FROM quest_template WHERE ID=66001;", H1_REASON),
        ("h1_set_statement_update", "SET STATEMENT max_statement_time=60 FOR UPDATE quest_template SET Flags=0;",
         H1_REASON),
        ("h1_set_statement_insert",
         "SET STATEMENT max_statement_time=60 FOR INSERT INTO quest_template (ID) VALUES (66001);", H1_REASON),
        # Conservative SOURCE detection only: MariaDB documents parser-affecting variables may not apply here.
        ("h1_set_statement_ansi_quoted",
         "SET STATEMENT sql_mode='ANSI_QUOTES' FOR UPDATE \"quest_template\" SET Flags=0;", H1_REASON),
        ("h2_explain_extended_update", "EXPLAIN EXTENDED UPDATE quest_template SET Flags=0;",
         "EXPLAIN " + G2_REASON),
        ("h2_explain_partitions_delete", "EXPLAIN PARTITIONS DELETE FROM quest_template WHERE ID=66001;",
         "EXPLAIN " + G2_REASON),
        ("h2_describe_extended_update", "DESCRIBE EXTENDED UPDATE quest_template SET Flags=0;",
         "DESCRIBE " + G2_REASON),
        ("h2_explain_with_update", "EXPLAIN WITH c AS (SELECT 1) UPDATE quest_template SET Flags=0;",
         "EXPLAIN " + G2_REASON),
        ("h2_explain_format_into_update", "EXPLAIN FORMAT=JSON INTO @v UPDATE quest_template SET Flags=0;",
         "EXPLAIN " + G2_REASON),
    ]
    # H3 + new H2 side effect: intentional, documented conservative false positives. Locked so that
    # nobody "fixes" them by loosening the gate.
    SQL_REV7_LOCKED_FALSE_POSITIVES = [
        ("explain_analyze_select", "EXPLAIN ANALYZE SELECT * FROM quest_template;", "EXPLAIN " + G2_REASON),
        ("leading_paren_select_quoted", '(SELECT 1 FROM "quest_template");',
         'double-quoted marker-table identifier "quest_template"'),
        ("leading_paren_select", "(SELECT 1 FROM quest_template);", "unsupported statement form '('"),
        ("explain_select_for_update", "EXPLAIN SELECT * FROM quest_template FOR UPDATE;", "EXPLAIN " + G2_REASON),
    ]
    SQL_REV7_CONTROLS = [
        ("real_set_variable_read_9681",
         "SET @BK_RACES_9681 := (SELECT `AllowableRaces` FROM `quest_template` WHERE `ID` = 9681);"),
        ("real_set_variable_read_9684",
         "SET @BK_RACES_9684 := (SELECT `AllowableRaces` FROM `quest_template` WHERE `ID` = 9684);"),
        ("set_variable_single_quoted_name", "SET @note = 'quest_template';"),
        ("set_variable_nested_for_update", "SET @x := (SELECT ID FROM quest_template WHERE ID = 1 FOR UPDATE);"),
        ("explain_keyword_only_in_string", "EXPLAIN SELECT * FROM quest_template WHERE LogTitle = 'UPDATE';"),
    ]

    def test_sql_rev7_negatives_report_the_intended_reason(self):
        for name, sql, reason in self.SQL_REV7_NEGATIVES:
            with self.subTest(fixture=name):
                err = self._sql_findings(sql + "\n")
                self.assertIsNotNone(err, f"{name}: not reported")
                self.assertIn(reason, err)

    def test_sql_rev7_documented_false_positives_stay_fail_closed(self):
        for name, sql, reason in self.SQL_REV7_LOCKED_FALSE_POSITIVES:
            with self.subTest(fixture=name):
                err = self._sql_findings(sql + "\n")
                self.assertIsNotNone(err, f"{name}: documented conservative false positive was loosened")
                self.assertIn(reason, err)

    def test_sql_rev7_controls_stay_clean(self):
        for name, sql in self.SQL_REV7_CONTROLS:
            with self.subTest(fixture=name):
                self.assertIsNone(self._sql_findings(sql + "\n"))

    def test_sql_rev6_negatives_report_the_intended_reason(self):
        for name, sql, reason in self.SQL_REV6_NEGATIVES:
            with self.subTest(fixture=name):
                err = self._sql_findings(sql + "\n")
                self.assertIsNotNone(err, f"{name}: not reported")
                self.assertIn(reason, err)

    def test_sql_rev6_controls_stay_clean(self):
        for name, sql in self.SQL_REV6_CONTROLS:
            with self.subTest(fixture=name):
                self.assertIsNone(self._sql_findings(sql + "\n"))

    def test_baseline_double_quoted_values_are_real_and_never_marker_names(self):
        """Bounds G1: double-quoted VALUES exist in the real tree, but none equals a marker-table name."""
        with_dq, dq_marker = 0, 0
        for rel, text in sources().ip_sql_tree.items():
            for stmt in x.iter_sql_statements(text):
                if not x._MARKER_TABLE_TEXT_RE.search(stmt):
                    continue
                toks = x.sql_tokens(stmt) or []
                dq = [t[1] for t in toks if t[0] == "str" and t[1].startswith('"')]
                with_dq += bool(dq)
                dq_marker += any(v[1:-1].strip().lower() in x.MARKER_TABLES for v in dq)
        self.assertGreater(with_dq, 50)
        self.assertEqual(dq_marker, 0)

    def test_sql_rev5_negatives_report_the_intended_reason(self):
        for name, sql, reason in self.SQL_REV5_NEGATIVES:
            with self.subTest(fixture=name):
                err = self._sql_findings(sql + "\n")
                self.assertIsNotNone(err, f"{name}: not reported")
                self.assertIn(reason, err)

    def test_sql_rev5_controls_stay_clean(self):
        for name, sql in self.SQL_REV5_CONTROLS:
            with self.subTest(fixture=name):
                self.assertIsNone(self._sql_findings(sql + "\n"))

    def test_odku_values_function_no_longer_raises_extraction_error(self):
        rows = x.ip_marker_rows(sources())
        sql = ("INSERT INTO quest_template_addon (ID, MaxLevel) VALUES (9999, 60) "
               "ON DUPLICATE KEY UPDATE MaxLevel = VALUES(MaxLevel);\n")
        self.assertEqual(x.marker_sql_findings(sql, rows), [])  # rev 4 raised "row has 1 values for 2 columns"

    def test_sql_rev4_negatives_report_the_intended_reason(self):
        for name, sql, reason in self.SQL_REV4_NEGATIVES:
            with self.subTest(fixture=name):
                err = self._sql_findings(sql + "\n")
                self.assertIsNotNone(err, f"{name}: not reported")
                self.assertIn(reason, err)

    def test_sql_rev4_controls_stay_clean(self):
        for name, sql in self.SQL_REV4_CONTROLS:
            with self.subTest(fixture=name):
                self.assertIsNone(self._sql_findings(sql + "\n"))

    def test_sql_gate_negatives_report_the_intended_reason(self):
        for name, sql, reason in self.SQL_GATE_NEGATIVES:
            with self.subTest(fixture=name):
                err = self._sql_findings(sql + "\n")
                self.assertIsNotNone(err, f"{name}: not reported")
                self.assertIn(reason, err)

    def test_sql_gate_controls_stay_clean(self):
        for name, sql in self.SQL_GATE_CONTROLS:
            with self.subTest(fixture=name):
                self.assertIsNone(self._sql_findings(sql + "\n"))

    def test_sql_gate_baseline_contains_the_broad_irrelevant_statement(self):
        """Proves the allowlist control is a REAL baseline case, not a synthetic one."""
        text = sources().ip_sql_tree["data/sql/world/base/vanilla_quest_reputations.sql"]
        self.assertIn("UPDATE quest_template SET RewardFactionOverride1 = 0 WHERE RewardFactionId1=0", text)
        rows = x.ip_marker_rows(sources())["quest_template"]
        self.assertTrue(all(r["rewardfactionid1"] == 0 for r in rows))

    def test_sql_tokens_are_quote_aware(self):
        toks = x.sql_tokens("UPDATE t SET a = 'x WHERE y' WHERE ID = 1;")
        self.assertEqual([t for t in toks if t[0] == "ident" and t[1].upper() == "WHERE"], [("ident", "WHERE")])

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


class OptionalRouteContracts(unittest.TestCase):
    """Rev-3 optional items: tree-wide full-reset call inventory and the AddClass login route."""

    def _tree_edit(self, rel: str, transform):
        tree = dict(sources().pb_src_tree)
        tree[rel] = transform(tree.get(rel, ""))
        return replace(sources(), pb_src_tree=tree)

    def test_new_full_reset_caller_in_another_file_is_detected(self):
        mutated = self._tree_edit("src/Ai/NewFeature.cpp", lambda _: (
            "void NewFeature::Run(Player* b)\n{\n    PlayerbotFactory f(b, 60);\n    f.Randomize(false);\n}\n"))
        with self.assertRaises(x.ContractViolation) as ctx:
            x.check_pb_tree_wide_full_reset_call_inventory(mutated)
        self.assertIn("('src/Ai/NewFeature.cpp', 'NewFeature::Run', 'Randomize(false)')", str(ctx.exception))

    def test_removed_full_reset_caller_is_detected(self):
        rel = "src/Bot/RandomPlayerbotMgr.cpp"
        mutated = self._tree_edit(rel, in_function("RandomPlayerbotMgr::Clear", sub_once(
            "factory.ClearEverything();", "factory.Refresh();")))
        with self.assertRaises(x.ContractViolation) as ctx:
            x.check_pb_tree_wide_full_reset_call_inventory(mutated)
        self.assertIn("RandomPlayerbotMgr::Clear", str(ctx.exception))

    def test_comment_only_caller_is_not_counted(self):
        mutated = self._tree_edit("src/Ai/Note.cpp", lambda _: "// f.Randomize(false); RandomizeFirst(bot);\n")
        x.check_pb_tree_wide_full_reset_call_inventory(mutated)

    def test_pointer_style_full_reset_callers_are_detected(self):
        """CS02-IP N3: '->' forms must be inventoried like '.' forms."""
        mutated = self._tree_edit("src/Ai/PointerFeature.cpp", lambda _: (
            "void PointerFeature::Run(Player* b)\n{\n    PlayerbotFactory* f = new PlayerbotFactory(b, 60);\n"
            "    f->Randomize(false);\n    sRandomPlayerbotMgr->Randomize(b);\n}\n"))
        with self.assertRaises(x.ContractViolation) as ctx:
            x.check_pb_tree_wide_full_reset_call_inventory(mutated)
        msg = str(ctx.exception)
        self.assertIn("('src/Ai/PointerFeature.cpp', 'PointerFeature::Run', 'Randomize(false)')", msg)
        self.assertIn("('src/Ai/PointerFeature.cpp', 'PointerFeature::Run', 'sRandomPlayerbotMgr.Randomize(')", msg)

    def test_new_rewarded_removal_caller_is_detected(self):
        mutated = self._tree_edit("src/Ai/ForgetAction.cpp", lambda _: (
            "bool ForgetAction::Execute(Event event)\n{\n    bot->RemoveRewardedQuest(66008);\n    return true;\n}\n"))
        with self.assertRaises(x.ContractViolation) as ctx:
            x.check_pb_rewarded_removal_caller_inventory(mutated)
        self.assertIn("('src/Ai/ForgetAction.cpp', 'ForgetAction::Execute', 'RemoveRewardedQuest(')", str(ctx.exception))

    def test_drop_quest_control_flow_changes_rejected_for_the_intended_reason(self):
        rel = "src/Ai/Base/Actions/DropQuestAction.cpp"
        fn = "DropQuestAction::Execute"
        fixtures = (
            ("removal_call_dropped", sub_once("    bot->RemoveRewardedQuest(entry);\n", "")),
            ("entry_zeroed_when_no_slot_matches", sub_once(
                "        uint32 logQuest = bot->GetQuestSlotQuestId(slot);",
                "        entry = 0;\n        uint32 logQuest = bot->GetQuestSlotQuestId(slot);")),
            ("stage_marker_exemption_added", sub_once(
                "    bot->RemoveRewardedQuest(entry);",
                "    if (entry < 66001 || entry > 66018)\n        bot->RemoveRewardedQuest(entry);")),
        )
        for name, transform in fixtures:
            with self.subTest(fixture=name):
                mutated = self._tree_edit(rel, in_function(fn, transform))
                with self.assertRaises(x.ContractViolation) as ctx:
                    x.check_pb_drop_quest_removes_rewarded_marker_without_log_slot(mutated)
                self.assertTrue(str(ctx.exception).startswith(x.MSG_DROP_QUEST_CHANGED))

    def test_drop_quest_reaches_stage_and_pvp_sets_separately(self):
        sets = x.drop_quest_affected_marker_sets(sources())
        self.assertEqual(sets["stage"], tuple(range(66001, 66019)))
        self.assertEqual(sets["pvp_rank"], tuple(range(66101, 66115)))
        self.assertFalse(set(sets["stage"]) & set(sets["pvp_rank"]))

    def test_addclass_login_matrix_from_pinned_condition(self):
        rows = [  # account type, master online, master level, bot level, expected full randomise
            (2, True, 60, 55, True),    # AddClass, gap 5
            (2, True, 50, 60, True),    # AddClass, master LOWER by 10 (abs)
            (2, True, 60, 57, False),   # gap 3: not > 3
            (2, False, 60, 10, False),  # no master
            (1, True, 60, 10, False),   # RandomBot account type 1: never this route
            (0, True, 60, 10, False),   # normal account
        ]
        for acct, master, mlvl, blvl, expected in rows:
            with self.subTest(account_type=acct, master=master, gap=mlvl - blvl):
                self.assertEqual(x.addclass_login_full_reset(acct, master, mlvl, blvl, sources()), expected)

    def test_addclass_condition_change_is_detected(self):
        for name, old, new in (("gap_threshold", "> 3)", "> 30)"),
                               ("type1_included", "IsAccountType(accountId, 2)", "IsAccountType(accountId, 1)")):
            with self.subTest(fixture=name):
                mutated = sources().with_edit("pb", "playerbot_mgr",
                                              in_function("PlayerbotHolder::OnBotLogin", sub_once(old, new)))
                with self.assertRaises(x.ContractViolation):
                    x.check_pb_addclass_login_full_reset_route(mutated)


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
