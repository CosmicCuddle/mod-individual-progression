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
COMMON_ANCESTOR = "dd96f2b77e3ebfa2a4726bf785525ba133f1ab61"

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
)
# The overlapping GM commands were reconciled by retaining Cosmic's exact
# source blob; there is deliberately NO net diff in that protected path.
CUSTOM_SCOPED = "src/cs_individualProgression.cpp"
GM_TELEPORT_HELPER = "src/IndividualProgressionGmTeleportPolicy.h"
CHROMAGGUS_PROTECTED = (
    "src/vanillaScripts/boss_chromaggus.cpp",
    "data/sql/world/base/dungeon_blackwing_lair.sql",
)
LOADER_ADDITION = "src/IndividualProgression_loader.cpp"


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
    def test_complete_upstream_27_path_inventory_has_an_explicit_disposition(self):
        # Immutable upstream compared to the original common ancestor, not
        # the existing Cosmic master or the custom PR branch. This catches an
        # upstream path silently omitted from a merge resolution.
        common = COMMON_ANCESTOR
        changed = set(subprocess.check_output(
            ["git", "diff", "--name-only", common, UPSTREAM],
            cwd=ROOT, text=True,
        ).splitlines())
        expected = (set(UPSTREAM_ONLY) | set(SHARED)
                    | {CUSTOM_SCOPED, LOADER_ADDITION}
                    | set(CHROMAGGUS_PROTECTED))
        self.assertEqual(len(expected), 27)
        self.assertEqual(changed, expected)
        # Shared paths are consciously reconciled rather than copied verbatim.
        # GM command source is currently the protected Cosmic custom version;
        # old GM / new upstream GM-tele differences remain a release-review item.
        # Chromaggus removal is deliberately held for owner gameplay policy.
        self.assertTrue(set(SHARED).isdisjoint(set(UPSTREAM_ONLY)))
        self.assertTrue(set(CHROMAGGUS_PROTECTED).isdisjoint(set(UPSTREAM_ONLY)))
        self.assertTrue(set(CHROMAGGUS_PROTECTED).isdisjoint(set(SHARED)))

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
        self.assertEqual(changed, set(UPSTREAM_ONLY) | set(SHARED) | {LOADER_ADDITION, CUSTOM_SCOPED, GM_TELEPORT_HELPER})

    def test_chromaggus_three_files_preserved(self):
        for path in CHROMAGGUS_PROTECTED:
            with self.subTest(path=path):
                self.assertEqual(current(path), historical(BASE, path))
        loader = current(LOADER_ADDITION)
        # Keep Cosmic's Chromaggus registration while importing exactly the
        # two required Borean Tundra registration lines from Grim upstream.
        original = historical(BASE, LOADER_ADDITION)
        declaration = "void AddSC_ipp_zone_nagrand();" + chr(10)
        call = "    AddSC_ipp_zone_nagrand();" + chr(10)
        self.assertEqual(original.count(declaration), 1)
        self.assertEqual(original.count(call), 1)
        expected = original.replace(
            declaration,
            declaration + "void AddSC_ipp_zone_borean_tundra();" + chr(10),
        ).replace(
            call,
            call + "    AddSC_ipp_zone_borean_tundra();" + chr(10),
        )
        self.assertEqual(loader, expected)
        self.assertEqual(loader.count("AddSC_boss_chromaggus_60_2_A();"), 2)
        self.assertEqual(loader.count("AddSC_ipp_zone_borean_tundra();"), 2)


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

    def test_custom_gm_commands_preserved_except_reviewed_teleport_adaptation(self):
        before = historical(BASE, "src/cs_individualProgression.cpp")
        after = current(CUSTOM_SCOPED)
        inc = '#include "IndividualProgressionGmTeleportPolicy.h"' + chr(10)
        self.assertEqual(after.count(inc), 1)
        old_tele = command_source(before, "HandleTeleIndividualProgressionCommand", "HandleAttuneIndividualProgressionCommand")
        new_tele = command_source(after, "HandleTeleIndividualProgressionCommand", "HandleAttuneIndividualProgressionCommand")
        # Outside this one command and one pure include, protect the full
        # original Cosmic #46 command source byte-for-byte.
        self.assertEqual(after.replace(inc, "").replace(new_tele, old_tele), before)
        self.assertIn("handler->HasLowerSecurity(target)", new_tele)
        self.assertIn("IndividualProgressionGmTeleportPolicy::CanTeleport(", new_tele)
        self.assertIn("Group* group = target->GetGroup();", new_tele)
        self.assertIn("group->SetRaidDifficulty(RAID_DIFFICULTY_10MAN_HEROIC)", new_tele)
        self.assertIn("target->SetRaidDifficulty(RAID_DIFFICULTY_10MAN_HEROIC)", new_tele)
        self.assertLess(new_tele.index("handler->HasLowerSecurity(target)"),
                        new_tele.index("target->TeleportTo("))
        self.assertIn("if (!target || !target->IsInWorld())", new_tele)
        self.assertIn("if (!player)", new_tele)
        self.assertIn("CanTeleport(", current(GM_TELEPORT_HELPER))

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

    def test_profession_cleanup_is_explicit_opt_in_only(self):
        config = options(current("conf/individualProgression.conf.dist"))
        header = current("src/IndividualProgression.h")
        core = current("src/IndividualProgression.cpp")
        spells = current("src/IndividualProgressionSpells.cpp")
        player_script = current("src/IndividualProgressionPlayer.cpp")
        self.assertEqual(config.get("IndividualProgression.ProfessionTierCap"), "0")
        self.assertIn("bool ProfessionTierCap = false;", header)
        self.assertIn(
            'GetOption<bool>("IndividualProgression.ProfessionTierCap", false)', core
        )
        self.assertIn(
            "if (!enabled || !ProfessionTierCap || !player || !player->IsInWorld()", spells
        )
        self.assertIn("void IndividualProgression::CapProfessionSkills(Player* player)", spells)
        self.assertIn("void IndividualProgression::CapProfessionAchievements(Player* player)", spells)
        self.assertIn("    if (!cap)\n        return;", spells)
        self.assertIn("sIndividualProgression->CapProfessionSkills(player);", player_script)
        self.assertIn("sIndividualProgression->CapProfessionAchievements(player);", player_script)

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
