"""Playerbots F14 opt-in reset keep-list proposed patch: source-only checks.

The patch is NOT installed, merged or enabled. CI will apply it only to the
exact historical pinned Playerbots checkout after verifying all four blobs.
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PB_PATCH = (ROOT / "tests/pb_patches/reset_earned_ip_markers_prototype.patch").read_text()
MODEL = (ROOT / "tests/pb_reset_keep_list_prototype.cpp").read_text()
PARENT = "536e9ec8f145b05a82eba38a006ce70f6c92d732"
FILES = {
    "src/PlayerbotAIConfig.cpp",
    "src/PlayerbotAIConfig.h",
    "src/Bot/Factory/PlayerbotFactory.cpp",
    "conf/playerbots.conf.dist",
}


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          check=True, text=True, capture_output=True).stdout.strip()


class GuardedPrototype(unittest.TestCase):
    def test_no_ip_src_config_or_db_changes_in_child(self):
        subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor",
                        PARENT, "HEAD"], check=True)
        self.assertEqual(git("diff", "--name-only", PARENT, "HEAD", "--",
                             "src", "conf", "sql", "data", "optional"), "")

    def test_patch_changes_only_four_known_pb_files(self):
        changed = re.findall(r"^diff --git a/(\S+) b/\1$", PB_PATCH, re.M)
        self.assertEqual(len(changed), 4)
        self.assertEqual(set(changed), FILES)
        self.assertEqual(PB_PATCH.count("diff --git "), 4)
        self.assertEqual(PB_PATCH.count("@@ -"), 6)
        for path in FILES:
            self.assertIn(f"--- a/{path}\n+++ b/{path}", PB_PATCH)

    def test_only_rewarded_stage_markers_can_be_preserved(self):
        self.assertIn("entry >= 66001 && entry <= 66018", PB_PATCH)
        self.assertIn("bot->GetQuestStatus(entry) == QUEST_STATUS_REWARDED", PB_PATCH)
        self.assertIn("uint32 const preserveMask = sPlayerbotAIConfig.ResetQuestsPreserveMask.load(std::memory_order_acquire);", PB_PATCH)
        self.assertIn("(preserveMask & (uint32(1) << (entry - 66001)))", PB_PATCH)
        self.assertIn("+            continue;", PB_PATCH)
        self.assertIn(" bot->RemoveActiveQuest(entry, false);", PB_PATCH)
        self.assertIn(" bot->RemoveRewardedQuest(entry);", PB_PATCH)
        self.assertNotIn("+        bot->RewardQuest(", PB_PATCH)
        self.assertNotIn("+        bot->SaveToDB(", PB_PATCH)

    def test_default_off_and_initialize_reload_safe(self):
        self.assertIn('GetOption<std::string>("Playerbots.ResetQuestsPreserveQuestIds", "")', PB_PATCH)
        self.assertIn("+    ResetQuestsPreserveMask.store(markerMask, std::memory_order_release);", PB_PATCH)
        self.assertIn("+    std::atomic<uint32> ResetQuestsPreserveMask{0};", PB_PATCH)
        self.assertIn("+#include <atomic>", PB_PATCH)
        self.assertIn("+    if (!resetPreserveIds.empty())", PB_PATCH)
        self.assertIn("+    uint32 markerMask = 0;", PB_PATCH)
        self.assertIn("+        if (id >= 66001 && id <= 66018)", PB_PATCH)
        self.assertIn("+Playerbots.ResetQuestsPreserveQuestIds =", PB_PATCH)
        self.assertNotRegex(PB_PATCH, r"\+Playerbots\.ResetQuestsPreserveQuestIds\s*=\s*6600")

    def test_pure_model_denies_missing_or_active_and_nonmarker(self):
        for s in ("QuestState::None", "QuestState::Active", "QuestState::Rewarded",
                  "std::set<std::uint32_t> const empty", "std::atomic<std::uint32_t>", "66001", "66008", "66018",
                  "id >= FirstIPMarker && id <= LastIPMarker"):
            self.assertIn(s, MODEL)

    def test_no_accidental_pb_fork_or_worldserver_write(self):
        changed = git("diff", "--name-only", PARENT, "HEAD").splitlines()
        self.assertTrue(changed)
        self.assertTrue(all(path.startswith(("tests/", "docs/", ".github/workflows/"))
                            for path in changed), changed)


class OptionalPinnedPB(unittest.TestCase):
    def test_applied_pb_source_matches_intended_guards(self):
        pb = os.environ.get("COSMIC_PB_PATCHED_ROOT")
        if not pb:
            self.skipTest("disposable PB checkout not supplied")
        root = Path(pb)
        factory = (root / "src/Bot/Factory/PlayerbotFactory.cpp").read_text()
        cfg = (root / "src/PlayerbotAIConfig.cpp").read_text()
        conf = (root / "conf/playerbots.conf.dist").read_text()
        start = factory.index("void PlayerbotFactory::ResetQuests()")
        end = factory.index("uint32 PlayerbotFactory::InitTalentsTree", start)
        reset = factory[start:end]
        self.assertEqual(reset.count("ResetQuestsPreserveMask.load(std::memory_order_acquire)"), 1)
        self.assertLess(reset.index("uint32 const preserveMask ="), reset.index("for (uint8 slot = 0;"))
        self.assertLess(reset.index("for (uint8 slot = 0;"), reset.index("if (entry >= 66001 && entry <= 66018"))
        self.assertIn("(preserveMask & (uint32(1) << (entry - 66001))) != 0", reset)
        self.assertLess(reset.index("if (entry >= 66001 && entry <= 66018"),
                        reset.index("bot->RemoveRewardedQuest(entry);"))
        self.assertIn("bot->GetQuestStatus(entry) == QUEST_STATUS_REWARDED", reset)
        self.assertIn("bot->RemoveActiveQuest(entry, false);", reset)
        self.assertIn('GetOption<std::string>("Playerbots.ResetQuestsPreserveQuestIds", "")', cfg)
        self.assertIn("ResetQuestsPreserveMask.store(markerMask, std::memory_order_release);", cfg)
        self.assertRegex(conf, r"(?m)^Playerbots\.ResetQuestsPreserveQuestIds =\s*$")


if __name__ == "__main__":
    unittest.main()
