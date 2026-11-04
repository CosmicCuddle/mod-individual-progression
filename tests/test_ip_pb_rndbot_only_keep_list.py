"""Source contracts for optional RNDBOT-ONLY refinement of original PB #49.

After applying #49 first, apply this one-file delta in disposable Playerbots
checkout. Never claim installed/host gameplay or grant an unearned marker.
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT = "e9252a4f37248d8727cb96004006a741a6c6c263"
DELTA = (ROOT / "tests/pb_patches/reset_earned_ip_markers_rndbot_only_refinement.patch").read_text()
MODEL = (ROOT / "tests/pb_rndbot_only_marker_preserve.cpp").read_text()
FACTORY = "src/Bot/Factory/PlayerbotFactory.cpp"


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          check=True, text=True, capture_output=True).stdout.strip()


class RefinedSource(unittest.TestCase):
    def test_original_pb_prototype_is_immutable_ancestor(self):
        subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor",
                        PARENT, "HEAD"], check=True)
        self.assertEqual(git("diff", "--name-only", PARENT, "HEAD", "--",
                             "src", "conf", "sql", "data", "optional"), "")
        self.assertEqual(git("diff", "--name-only", PARENT, "HEAD", "--",
                             "tests/pb_patches/reset_earned_ip_markers_prototype.patch"), "")

    def test_second_patch_changes_only_factory_no_default_or_sql(self):
        changed = re.findall(r"^diff --git a/(\S+) b/\1$", DELTA, re.M)
        self.assertEqual(changed, [FACTORY])
        self.assertEqual(DELTA.count("diff --git "), 1)
        self.assertEqual(DELTA.count("@@ -"), 2)
        self.assertIn("-        if (entry >= 66001 && entry <= 66018 &&", DELTA)
        self.assertIn("+        if (preserveForVerifiedRandomBot && entry >= 66001 && entry <= 66018 &&", DELTA)
        self.assertIn("preserveMask != 0 && sRandomPlayerbotMgr.IsRandomBot(bot);", DELTA)
        for forbidden in ("Playerbots.ResetQuestsPreserveQuestIds =", "RewardQuest(",
                          "SaveToDB(", "SetQuestStatus(", "SetQuestSlot("):
            self.assertNotIn("+" + forbidden, DELTA)

    def test_no_unauthenticated_quest_grants_in_refinement(self):
        self.assertIn("positivelySelectedRandomBot && keepMask != 0", MODEL)
        self.assertIn("status == QuestStatus::Rewarded", MODEL)
        self.assertIn("id >= First && id <= Last", MODEL)
        self.assertIn("std::atomic<std::uint32_t>", MODEL)
        self.assertIn("std::uint32_t{0}", MODEL)
        self.assertIn("false", MODEL)

    def test_child_changes_only_ci_tests_patch_docs(self):
        changes = git("diff", "--name-only", PARENT, "HEAD").splitlines()
        self.assertTrue(changes)
        self.assertTrue(all(x.startswith(("tests/", "docs/", ".github/workflows/"))
                            for x in changes), changes)


class PatchedPlayerbots(unittest.TestCase):
    def setUp(self):
        root = os.environ.get("COSMIC_PB_RNDBOT_ONLY_ROOT")
        if not root:
            self.skipTest("no disposable, dual-patched Playerbots checkout")
        self.root = Path(root)

    def test_selected_random_bot_checked_once_before_quest_clear(self):
        source = (self.root / FACTORY).read_text()
        first = source.index("void PlayerbotFactory::ResetQuests()")
        last = source.index("uint32 PlayerbotFactory::InitTalentsTree", first)
        reset = source[first:last]
        self.assertIn("preserveMask != 0 && sRandomPlayerbotMgr.IsRandomBot(bot);", reset)
        self.assertEqual(reset.count("sRandomPlayerbotMgr.IsRandomBot(bot)"), 1)
        self.assertLess(reset.index("bool const preserveForVerifiedRandomBot"),
                        reset.index("for (uint8 slot = 0;"))
        self.assertIn("if (preserveForVerifiedRandomBot && entry >= 66001 && entry <= 66018 &&", reset)
        self.assertIn("bot->GetQuestStatus(entry) == QUEST_STATUS_REWARDED", reset)
        self.assertIn("(preserveMask & (uint32(1) << (entry - 66001))) != 0", reset)
        self.assertIn("bot->RemoveRewardedQuest(entry);", reset)
        self.assertIn("bot->RemoveActiveQuest(entry, false);", reset)
        self.assertNotIn("if (entry >= 66001 && entry <= 66018 &&", reset)

    def test_exact_playerbots_manager_proves_positive_selected_identity(self):
        source = (self.root / "src/Bot/RandomPlayerbotMgr.cpp").read_text()
        start = source.index("bool RandomPlayerbotMgr::IsRandomBot(Player* bot)")
        end = source.index("bool RandomPlayerbotMgr::IsRandomBot(ObjectGuid::LowType bot)", start)
        player = source[start:end]
        self.assertIn("IsSelfBot(bot)", player)  # excludes controlled self/Altbots
        self.assertIn("return IsRandomBot(bot->GetGUID().GetCounter());", player)
        start = end
        end = source.index("bool RandomPlayerbotMgr::IsAddclassBot(", start)
        guid = source[start:end]
        self.assertIn("IsInRandomAccountList(", guid)
        self.assertIn("return currentBots.contains(bot);", guid)


if __name__ == "__main__":
    unittest.main()
