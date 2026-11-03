"""F14 Playerbots ResetQuests release blocker: source-only negative contracts.

No bot is logged in, no quests changed, no AzerothCore Worldserver launched.
The negative model documents a known LOSS, not a working preservation fix.
Run:
  python3 -B -m unittest discover -s tests -p 'test_ip_f14_playerbots_reset_blocker.py' -v
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE_IP46 = "26d5b7a1de30ad92a595aaf251fc14754cf54f32"
PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
POLICY = (ROOT / "src/IndividualProgressionF14LoginPolicy.h").read_text()
CONFIG = (ROOT / "conf/individualProgression.conf.dist").read_text()
DOC = (ROOT / "docs/F14_PLAYERBOTS_RESET_PRESERVATION_BLOCKER_2026-11-03.md").read_text()


def git(*args):
    return subprocess.run(
        ["git", "-C", str(ROOT), *args], capture_output=True, text=True,
        check=True,
    ).stdout.strip()


class SourceOwnership(unittest.TestCase):
    def test_no_executable_source_or_configuration_writes_in_test_child(self):
        # This branch may propose tests/docs/CI only. No executable IP
        # workaround, CharacterDatabase SQL, config flag or client patch.
        self.assertEqual(
            git("diff", "--name-only", BASE_IP46, "HEAD", "--",
                "src", "conf", "sql", "data", "optional"), ""
        )

    def test_login_guard_preserves_only_preexisting_earned_markers(self):
        self.assertIn("const savedStage = sIndividualProgression->GetPlayerProgressionFromQuests(player);", PLAYER)
        self.assertIn("savedStage >= IndividualProgressionGroupPolicy::FirstTbcStage", PLAYER)
        self.assertIn("savedStage <= IndividualProgressionGroupPolicy::LastSupportedStage", PLAYER)
        self.assertIn("PreservesAtLeastLevelStage(", PLAYER)
        self.assertIn("if (!preserveVerifiedRandomBotStage)", PLAYER)
        self.assertIn("ForceUpdateProgressionState(player, static_cast<ProgressionState>(0))", PLAYER)
        self.assertIn("ForceUpdateProgressionState(player, static_cast<ProgressionState>(8))", PLAYER)
        self.assertIn("ForceUpdateProgressionState(player, static_cast<ProgressionState>(13))", PLAYER)

    def test_no_unsafe_ip_save_or_level_hook_repair(self):
        # RewardQuest -> SaveToDB may reenter an existing save. Never
        # invent a bot progression stage during a Player save or level hook.
        self.assertNotRegex(PLAYER, r"\bOnPlayerSave\s*\(")
        self.assertNotIn("PLAYERHOOK_ON_SAVE", PLAYER)
        level_hook = PLAYER.split("void OnPlayerLevelChanged(", 1)[1].split("\n    }", 1)[0]
        self.assertNotIn("ForceUpdateProgressionState", level_hook)
        self.assertNotIn("RewardQuest", level_hook)
        self.assertNotIn("AddQuest", level_hook)
        self.assertNotIn("RemoveRewardedQuest", level_hook)
        self.assertNotIn("RemoveActiveQuest", level_hook)

    def test_group_floor_fails_closed_for_erased_stage_zero(self):
        self.assertIn("return currentStage >= floor && requestedStage < floor;", POLICY)
        self.assertIn("return 8;", POLICY)
        self.assertIn("PB reset is unsolved", POLICY)
        self.assertNotIn("RewardQuest(", POLICY)

    def test_partial_pilot_disabled_by_default(self):
        for key in (
            "IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin",
            "IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor",
        ):
            self.assertEqual(len(re.findall(r"^" + re.escape(key) + r"\s*=\s*0\s*$",
                                            CONFIG, flags=re.M)), 1, key)


class ReleaseBlockerContract(unittest.TestCase):
    def test_pin_and_no_claim_of_recovery(self):
        for phrase in (
            "BLOCKED_CORE_PB_HOOK", "66001–66018",
            "RemoveRewardedQuest(entry)", "RemoveActiveQuest(entry, false)",
            "PlayerbotFactory::ResetQuests", "SaveToDB(false, false)",
            "not a working preservation fix", "default empty",
        ):
            # The run-time terms and explicit unsafe-limit disclosures
            # should remain in the source-backed release memo.
            if phrase == "not a working preservation fix":
                self.assertIn("not a working preservation fix", DOC.lower())
            else:
                self.assertIn(phrase, DOC)

    def test_disallow_on_save_stage_reaward_as_suggested_solution(self):
        self.assertIn("OnPlayerSave", DOC)
        self.assertIn("rejected", DOC.lower())
        self.assertIn("Do not implement", DOC)
        self.assertIn("owner", DOC.lower())

    def test_source_trace_is_a_negative_model_not_server_integration(self):
        src = (ROOT / "tests/f14_playerbots_reset_blocker.cpp").read_text()
        self.assertIn("Playerbots ResetQuests BLOCKER model", src)
        self.assertIn("ModelLoginStage(true, true, 0, 50)", src)
        self.assertIn("ModelLoginStage(true, true, 0, 65)", src)
        self.assertIn("ModelLoginStage(true, true, 0, 75)", src)
        self.assertNotIn("RewardQuest(", src)
        self.assertNotIn("SaveToDB(", src.replace("SaveToDB(false, false)", ""))


class PinnedPlayerbotsSource(unittest.TestCase):
    def test_optional_external_pinned_pb_source_has_unprotected_reset(self):
        # CI can set this after checking out the historical PB commit.
        # Local module-only tests skip rather than invent an external path.
        pb = os.environ.get("COSMIC_PINNED_PB_FACTORY")
        if not pb:
            self.skipTest("historical Playerbots checkout not supplied")
        text = Path(pb).read_text()
        start = text.index("void PlayerbotFactory::ResetQuests()")
        chunk = text[start:text.index("uint32 PlayerbotFactory::InitTalentsTree", start)]
        self.assertIn("bot->RemoveRewardedQuest(entry);", chunk)
        self.assertIn("bot->RemoveActiveQuest(entry, false);", chunk)
        self.assertNotRegex(chunk, r"6600[1-9]|6601[0-8]")
        self.assertIn("if (bot->GetQuestStatus(entry) == QUEST_STATUS_NONE)", chunk)
        begin = text.index("void PlayerbotFactory::Randomize(bool incremental)")
        randomize = text[begin:text.index("void PlayerbotFactory::Refresh()", begin)]
        self.assertIn("if (!incremental)", randomize)
        self.assertIn("ResetQuests();", randomize)
        self.assertIn("bot->SaveToDB(false, false);", randomize)


if __name__ == "__main__":
    unittest.main()
