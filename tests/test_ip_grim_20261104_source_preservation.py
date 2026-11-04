#!/usr/bin/env python3
"""Grim upstream + Cosmic IP source integration safety contracts.

These are reviewable, *offline source-only* tests. They cannot demonstrate
C++ compile, a running Worldserver, correct database rows, or actual gameplay.
The expected historical refs are intentional; do not silently repin them.
"""

import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "26d5b7a1de30ad92a595aaf251fc14754cf54f32"
UPSTREAM = "42c9a32892740af55a40e076e6381cc879181396"

UPSTREAM_ONLY = (
    "data/sql/world/base/av.sql",
    "data/sql/world/base/av_gameobjects.sql",
    "data/sql/world/base/cs_individualProgression.sql",
    "data/sql/world/base/dungeon_dire_maul.sql",
    "data/sql/world/base/dungeon_magisters_terrace.sql",
    "data/sql/world/base/dungeon_onyxia.sql",
    "data/sql/world/base/naxx40.sql",
    "data/sql/world/base/zone_borean_tundra.sql",
    "data/sql/world/base/zone_howling_fjord.sql",
    "data/sql/world/base/zone_ironforge.sql",
    "data/sql/world/base/zone_isle_of_queldanas.sql",
    "data/sql/world/base/zz_ipp_aware_npcs.sql",
    "optional/sql/world/zz_optional_custom_races.sql",
    "optional/sql/world/zz_optional_vanilla_transports.sql",
    "src/naxx40Scripts/custom_gameobjects_40.cpp",
    "src/vanillaScripts/instance_onyxias_lair.cpp",
    "src/vanillaScripts/quest_the_masquerade.cpp",
    "src/wotlkScripts/zone_borean_tundra.cpp",
)
SHARED = (
    "conf/individualProgression.conf.dist",
    "src/IndividualProgression.cpp",
    "src/IndividualProgression.h",
    "src/IndividualProgressionPlayer.cpp",
    "src/IndividualProgressionSpells.cpp",
    "src/cs_individualProgression.cpp",
)
CHROMAGGUS_PROTECTED = (
    "src/vanillaScripts/boss_chromaggus.cpp",
    "src/IndividualProgression_loader.cpp",
    "data/sql/world/base/dungeon_blackwing_lair.sql",
)


def historical(ref, path):
    return subprocess.check_output(
        ["git", "show", f"{ref}:{path}"], cwd=ROOT
    ).decode("utf-8")


def current(path):
    return (ROOT / path).read_text()


def options(text):
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r"\s*(IndividualProgression\.[\w.]+)\s*=\s*(.*?)\s*", line)
        if match:
            key, value = match.groups()
            if key in result:
                raise AssertionError("duplicate config key: " + key)
            result[key] = value
    return result


def command_source(text, name, next_name):
    start = "    static bool " + name + "("
    end = "    static bool " + next_name + "("
    if text.count(start) != 1 or text.count(end) != 1:
        raise AssertionError("command markers changed: " + name)
    return text.split(start, 1)[1].split(end, 1)[0]


class ExactProvenance(unittest.TestCase):
    def test_custom_ancestor_is_retained(self):
        subprocess.run(
            ["git", "merge-base", "--is-ancestor", BASE, "HEAD"],
            cwd=ROOT, check=True,
        )

    def test_upstream_only_files_match_byte_for_byte(self):
        for path in UPSTREAM_ONLY:
            with self.subTest(path=path):
                self.assertEqual(
                    (ROOT / path).read_bytes(),
                    subprocess.check_output(
                        ["git", "show", f"{UPSTREAM}:{path}"], cwd=ROOT
                    ),
                )

    def test_no_extra_source_sql_or_conf_paths(self):
        changed = set(subprocess.check_output(
            ["git", "diff", "--name-only", BASE, "HEAD", "--",
             "src", "conf", "data/sql", "optional/sql"],
            cwd=ROOT, text=True,
        ).splitlines())
        self.assertEqual(changed, set(UPSTREAM_ONLY) | set(SHARED))

    def test_chromaggus_three_files_preserved(self):
        for path in CHROMAGGUS_PROTECTED:
            with self.subTest(path=path):
                self.assertEqual(current(path), historical(BASE, path))
        self.assertIn(
            "AddSC_boss_chromaggus_60_2_A();",
            current("src/IndividualProgression_loader.cpp"),
        )


class PolicyPreservation(unittest.TestCase):
    def test_existing_custom_settings_unchanged_and_autoattune_off(self):
        before = options(historical(BASE, "conf/individualProgression.conf.dist"))
        after = options(current("conf/individualProgression.conf.dist"))
        for key, value in before.items():
            with self.subTest(key=key):
                self.assertEqual(after.get(key), value)
        self.assertEqual(after.get("IndividualProgression.AutoAttune"), "0")
        self.assertEqual(after.get("IndividualProgression.PaidExpansion.Enable"), "0")
        self.assertEqual(
            after.get("IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor"), "0"
        )

    def test_autoattune_is_bounded_by_off_default(self):
        header = current("src/IndividualProgression.h")
        source = current("src/IndividualProgression.cpp")
        self.assertIn("bool AutoAttune = false;", header)
        self.assertIn("void IndividualProgression::GrantAttunements(", source)
        self.assertIn("if (!enabled || !AutoAttune || !player", source)
        self.assertIn(
            "sIndividualProgression->GrantAttunements(player, newState);", source
        )
        self.assertNotIn(
            "IndividualProgression.AutoAttune = 1",
            current("conf/individualProgression.conf.dist"),
        )

    def test_custom_gm_command_bodies_exactly_preserved(self):
        before = historical(BASE, "src/cs_individualProgression.cpp")
        after = current("src/cs_individualProgression.cpp")
        pairs = (
            ("HandleGetIndividualProgressionCommand", "HandleSetIndividualProgressionCommand"),
            ("HandleSetIndividualProgressionCommand", "HandleSetBotIndividualProgressionCommand"),
            ("HandleSetBotIndividualProgressionCommand", "HandleSetRepIndividualProgressionCommand"),
            ("HandleSetRepIndividualProgressionCommand", "HandlePetIndividualProgressionCommand"),
            ("HandlePetIndividualProgressionCommand", "HandleTeleIndividualProgressionCommand"),
            ("HandleTeleIndividualProgressionCommand", "HandleAttuneIndividualProgressionCommand"),
            ("HandleAttuneIndividualProgressionCommand", "HandlePVPIndividualProgressionCommand"),
        )
        for name, nxt in pairs:
            with self.subTest(command=name):
                self.assertEqual(
                    command_source(after, name, nxt),
                    command_source(before, name, nxt),
                )

    def test_excluded_human_login_not_silently_rewritten(self):
        source = current("src/IndividualProgressionPlayer.cpp")
        self.assertIn(
            "if (!sIndividualProgression->isNormalAccount(player)) // bot or exluded account",
            source,
        )
        self.assertNotIn(
            "ForceUpdateProgressionState(player, static_cast<ProgressionState>(18))",
            source,
        )
        guard = source.split(
            "if (!sIndividualProgression->isNormalAccount(player))", 1
        )[1].split("// F14 partial safeguard", 1)[0]
        self.assertIn("if (sIndividualProgression->isBotAccount(player))", guard)
        self.assertIn("sIndividualProgression->UpdateRNDbotSpells(player)", guard)
        self.assertIn("ITEM_THIEVES_TOOLS = 5060", guard)
        self.assertLess(
            guard.index("if (sIndividualProgression->isBotAccount(player))"),
            guard.index("ITEM_THIEVES_TOOLS = 5060"),
        )

    def test_custom_hunter_and_paid_guards_still_present(self):
        spells = current("src/IndividualProgressionSpells.cpp")
        cs = current("src/cs_individualProgression.cpp")
        ip = current("src/IndividualProgression.cpp")
        self.assertIn('#include "CharmInfo.h"', spells)
        self.assertIn("NeedsPaidGrantAudit(", cs)
        self.assertIn("ShouldSkipF14VerifiedRandomBotGroupDowngrade(", ip)
        self.assertIn("PurchaseExpansion(Player* player", current("src/IndividualProgression.h"))


if __name__ == "__main__":
    unittest.main()
