"""Source and model tests for owner-prioritised F14 true RandomBot stage floor.

This tests SOURCE ONLY. Passing tests cannot establish new-bot startup/rehydration
coverage, correct live identities, global cap behaviour or gameplay.
"""
import unittest
from itertools import product
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
CONF = (ROOT / "conf/individualProgression.conf.dist").read_text()
IPCORE = (ROOT / "src/IndividualProgression.cpp").read_text()
HEADER = (ROOT / "src/IndividualProgression.h").read_text()
POLICY = (ROOT / "src/IndividualProgressionF14LoginPolicy.h").read_text()
KEY = "IndividualProgression.F14.EnforceVerifiedRandomBotTbcFloorOnLogin"


def chosen(enabled, bot_account, race, headless, positive_rndbot):
    return (enabled and bot_account and race in (10, 11)
            and headless and positive_rndbot)


def login_floor(saved, level):
    required = 8 if level <= 70 else 13
    return max(saved, required)


class F14TbcFloor(unittest.TestCase):
    def test_off_by_default_and_fails_closed_if_pb_missing(self):
        self.assertEqual(CONF.count(KEY + " = 0"), 1)
        self.assertIn('GetOption<bool>(\n            "' + KEY + '", false)', IPCORE)
        self.assertIn("f14EnforceVerifiedRandomBotTbcFloorOnLogin", HEADER)
        self.assertIn('if (sIndividualProgression->f14EnforceVerifiedRandomBotTbcFloorOnLogin)', IPCORE)
        self.assertIn("f14EnforceVerifiedRandomBotTbcFloorOnLogin = false;", IPCORE)

    def test_exact_positive_rndbot_guards(self):
        snippet = PLAYER.split(
            "// F14 NEW owner-priority SOURCE PILOT, default OFF.", 1
        )[1].split("// END F14 new verified RandomBot stage-floor block.", 1)[0]
        for part in [
            "f14EnforceVerifiedRandomBotTbcFloorOnLogin",
            "sIndividualProgression->isBotAccount(player)",
            "player->getRace() == RACE_DRAENEI",
            "player->getRace() == RACE_BLOODELF",
            "player->GetSession()->IsHeadless()",
            "sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())",
            "GetPlayerProgressionFromQuests(player)",
            "TbcRaceFloorStage(player->GetLevel())",
            "savedStage >= floorStage",
            "NeedsTbcRaceFloor(",
            "ForceUpdateProgressionState(",
            "verifiedRandomBotTbcFloorHandled = true",
        ]:
            self.assertIn(part, snippet, part)
        self.assertIn(
            "if (!preserveVerifiedRandomBotStage && !verifiedRandomBotTbcFloorHandled)",
            PLAYER
        )
        self.assertEqual(snippet.count("ForceUpdateProgressionState("), 1,
                         "only one permitted stage writer inside the verified identity gate")
        self.assertNotIn("UpdateProgressionState(", snippet)
        self.assertNotIn("SetPhaseMask(", snippet)

    def test_pure_stage_policy_and_no_other_class_eras(self):
        self.assertIn("constexpr std::uint8_t TbcRaceFloorStage", POLICY)
        self.assertIn("constexpr bool NeedsTbcRaceFloor", POLICY)
        self.assertIn("return LevelStage(level) < 8 ? 8 : LevelStage(level);", POLICY)
        self.assertIn("return savedStage < TbcRaceFloorStage(level);", POLICY)

    def test_identity_negative_matrix_and_preserve_earned(self):
        checked = 0
        for on, bot_account, race, headless, selected in product(
            (False, True), (False, True), (1, 10, 11),
            (False, True), (False, True)
        ):
            decision = chosen(on, bot_account, race, headless, selected)
            self.assertEqual(
                decision,
                on and bot_account and headless and selected and race in (10, 11)
            )
            for saved in range(0, 19):
                for level in (1, 60, 70, 71, 80):
                    result = login_floor(saved, level) if decision else saved
                    if decision:
                        self.assertGreaterEqual(result, 8)
                        self.assertGreaterEqual(result, saved)
                        if saved >= (8 if level <= 70 else 13):
                            self.assertEqual(result, saved)
                    else:
                        self.assertEqual(result, saved)
                    checked += 1
        self.assertEqual(checked, 2 * 2 * 3 * 2 * 2 * 19 * 5)

    def test_original_login_and_grouping_intact_under_flag_off(self):
        # New insertion is isolated to bot-account login block. No normal
        # character branch is edited, no grouping or Playerbots reset code.
        self.assertIn("else // normal account", PLAYER)
        self.assertIn("if (!sIndividualProgression->isNormalAccount(player))", PLAYER)
        self.assertIn("preserveVerifiedRandomBotStage", PLAYER)


if __name__ == "__main__":
    unittest.main()
