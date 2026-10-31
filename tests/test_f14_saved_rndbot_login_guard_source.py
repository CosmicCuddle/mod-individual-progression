#!/usr/bin/env python3
"""F14 conservative login-only saved-stage preservation source and model tests.

Not an assertion of real identity accuracy or safe visibility. Executable C++
will be separately compiled in pinned disposable Worldserver; no game started.
"""
import re
import unittest
from pathlib import Path
from itertools import product

ROOT = Path(__file__).resolve().parents[1]
LOGIN = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
CORE = (ROOT / "src/IndividualProgression.cpp").read_text()
HEADER = (ROOT / "src/IndividualProgression.h").read_text()
CONF = (ROOT / "conf/individualProgression.conf.dist").read_text()


def should_preserve(enabled, race, headless, active_rndbot, saved_stage):
    return (enabled and race in (10, 11) and headless and active_rndbot and
            8 <= saved_stage <= 18)


class SourceGuards(unittest.TestCase):
    def test_default_disabled_in_config_and_code(self):
        key = "IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin"
        self.assertIn(key + " = 0", CONF)
        self.assertIn('GetOption<bool>("' + key + '", false)', CORE)
        self.assertIn("f14PreserveVerifiedRandomBotStageOnLogin", HEADER)

    def test_original_level_based_forced_stage_remains_beneath_guard(self):
        self.assertIn("if (!preserveVerifiedRandomBotStage)", LOGIN)
        block = LOGIN.split("if (!preserveVerifiedRandomBotStage)", 1)[1]
        old = [
            "if (player->GetLevel() <= 60)",
            "ForceUpdateProgressionState(player, static_cast<ProgressionState>(0))",
            "else if ((player->GetLevel() > 60) && (player->GetLevel() <= 70))",
            "ForceUpdateProgressionState(player, static_cast<ProgressionState>(8))",
            "ForceUpdateProgressionState(player, static_cast<ProgressionState>(13))"
        ]
        positions = [block.index(s) for s in old]
        self.assertEqual(sorted(positions), positions)
        self.assertEqual(LOGIN.count("ForceUpdateProgressionState(player, static_cast<ProgressionState>(0))"), 1)

    def test_positional_prewrite_guard(self):
        begin = LOGIN.index("bool preserveVerifiedRandomBotStage = false;")
        end = LOGIN.index("if (!preserveVerifiedRandomBotStage)", begin)
        sec = LOGIN[begin:end]
        self.assertIn("sIndividualProgression->f14PreserveVerifiedRandomBotStageOnLogin", sec)
        self.assertIn("RACE_DRAENEI", sec)
        self.assertIn("RACE_BLOODELF", sec)
        self.assertIn("player->GetSession()->IsHeadless()", sec)
        self.assertIn("GetPlayerProgressionFromQuests(player)", sec)
        self.assertIn("savedStage >= IndividualProgressionGroupPolicy::FirstTbcStage", sec)
        self.assertIn("savedStage <= IndividualProgressionGroupPolicy::LastSupportedStage", sec)
        self.assertIn("sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())", sec)
        self.assertNotIn("ForceUpdateProgressionState", sec)
        self.assertNotIn("AccountMgr::GetName", sec)
        self.assertNotIn("LoginDatabase", sec)

    def test_existing_normal_account_branch_left_intact(self):
        self.assertIn("else // normal account", LOGIN)
        self.assertIn("sIndividualProgression->checkIPProgression(player);", LOGIN)
        self.assertIn("sIndividualProgression->CheckAdjustments(player);", LOGIN)

    def test_playerbots_dependency_optional_and_scope_only_in_login(self):
        self.assertIn('#if __has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")', LOGIN)
        self.assertIn("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE", LOGIN)
        self.assertIn("#endif", LOGIN)
        self.assertEqual(LOGIN.count("sRandomPlayerbotMgr.IsRandomBot("), 1)
        self.assertNotIn("RandomBot", (ROOT / "src/cs_individualProgression.cpp").read_text())

    def test_no_stage8_grant_or_visibility_change(self):
        guarded = LOGIN.split("bool preserveVerifiedRandomBotStage = false;", 1)[1].split("else // normal account", 1)[0]
        self.assertNotIn("sIndividualProgression->UpdateProgressionState(player, static_cast<ProgressionState>(8));", guarded)
        self.assertNotIn("SetPhaseMask", guarded)
        self.assertNotIn("SetVisible", guarded)
        self.assertNotIn("SetInvisibility", guarded)

    def test_header_and_conf_keep_group_config_unmodified(self):
        self.assertIn("IndividualProgression.EnforceGroupRules = 0", CONF)
        self.assertIn("IndividualProgression.EnforceGroupRules", CORE)


class SyntheticSemantics(unittest.TestCase):
    def test_matrix_exact_conditions(self):
        for enabled, race, headless, active, stage in product(
                (False, True), (1, 4, 10, 11), (False, True),
                (False, True), (0, 1, 7, 8, 12, 13, 18, 19)):
            got = should_preserve(enabled, race, headless, active, stage)
            expected = all([enabled, race in (10, 11), headless, active, stage in range(8, 19)])
            self.assertEqual(got, expected)

    def test_off_makes_every_state_legacy(self):
        for race in (1, 10, 11):
            for stage in range(0, 20):
                self.assertFalse(should_preserve(False, race, True, True, stage))

    def test_blood_elf_draenei_stage8_not_rewritten(self):
        for race in (10, 11):
            for stage in range(8, 19):
                self.assertTrue(should_preserve(True, race, True, True, stage))

    def test_unselected_type0_type2_and_altbots_never_positive(self):
        for race in (10, 11):
            for stage in (8, 12, 18):
                self.assertFalse(should_preserve(True, race, True, False, stage))
                self.assertFalse(should_preserve(True, race, False, True, stage))

    def test_stage0_never_auto_upgraded(self):
        for race in (10, 11):
            for stage in (0, 1, 7):
                self.assertFalse(should_preserve(True, race, True, True, stage))


if __name__ == "__main__":
    unittest.main()
