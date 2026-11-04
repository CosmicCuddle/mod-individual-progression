"""F14 Core reevaluation notification prototype: source-only fail-closed contract.

This observes a server-side visibility pass. It is neither a client packet ACK
nor evidence that concealment is safe; no module flag is enabled by this patch.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "tests/core_patches/f14_visibility_pass_observed_prototype.patch"


class CoreF14ReevaluationPrototype(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.patch = PATCH.read_text(encoding="utf-8")

    def test_exact_affected_core_files(self):
        paths = re.findall(r"^diff --git a/(\S+) b/\1$", self.patch, flags=re.M)
        self.assertEqual(set(paths), {
            "src/server/game/Scripting/ScriptMgr.h",
            "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
            "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
            "src/server/game/Entities/Object/Object.cpp",
            "src/server/game/Entities/Player/PlayerUpdates.cpp",
        })
        self.assertEqual(len(paths), 5)  # two hunks in the same script-header patch
        self.assertNotIn("src/server/game/Handlers/MiscHandler.cpp", paths)
        self.assertNotIn("src/server/game/Entities/Creature/Creature.cpp", paths)

    def test_unified_diff_has_context_and_exact_line_counts(self):
        # A zero-context or unterminated patch can pass simple string tests
        # yet be rejected by git apply before any source can compile.
        self.assertTrue(self.patch.endswith("\n"))
        rows = self.patch.splitlines()
        hunks = 0
        i = 0
        while i < len(rows):
            match = re.fullmatch(r"@@ -(\d+),(\d+) \+(\d+),(\d+) @@", rows[i])
            if not match:
                i += 1
                continue
            expect_old, expect_new = int(match[2]), int(match[4])
            i += 1
            actual_old = actual_new = context = 0
            while i < len(rows) and not rows[i].startswith(("diff --git ", "@@ ")):
                marker = rows[i][:1]
                self.assertIn(marker, (" ", "+", "-"), (i, rows[i]))
                actual_old += marker in (" ", "-")
                actual_new += marker in (" ", "+")
                context += marker == " "
                i += 1
            self.assertEqual((actual_old, actual_new), (expect_old, expect_new))
            self.assertGreater(context, 0)
            hunks += 1
        self.assertEqual(hunks, 6)

    def test_new_callback_is_observational_and_never_declares_complete_bridge(self):
        p = self.patch
        self.assertIn("void OnF14VisibilityReevaluationProcessed(WorldObject const* object, bool playerFrame);", p)
        self.assertIn("script->OnF14VisibilityReevaluationProcessed(object, playerFrame)", p)
        self.assertIn("WORLDOBJECTHOOK_ON_F14_VISIBILITY_REEVALUATION_PROCESSED", p)
        self.assertIn("not a network/client acknowledgement", p)
        self.assertNotIn("+ #define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", p)
        self.assertNotIn("+#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", p)
        self.assertNotIn("UpdateVisibilityOf(", p)
        self.assertNotIn("DestroyForPlayer(", p)

    def test_notification_follows_actual_player_and_creature_passes(self):
        p = self.patch
        object_segment = p.split("diff --git a/src/server/game/Entities/Object/Object.cpp", 1)[1]
        player_segment = p.split("diff --git a/src/server/game/Entities/Player/PlayerUpdates.cpp", 1)[1]
        self.assertLess(object_segment.index("Cell::VisitObjects(this, notifier"), 
                        object_segment.index("OnF14VisibilityReevaluationProcessed(this, false)"))
        self.assertIn("if (!IsPlayer())", object_segment)
        self.assertLess(player_segment.index("UpdateVisibilityForPlayer();"),
                        player_segment.index("OnF14VisibilityReevaluationProcessed(this, true)"))
        self.assertNotIn("+        UpdateObjectVisibility(true, true);", player_segment)

    def test_ip_flags_still_default_off_and_no_new_mutations(self):
        conf = (ROOT / "conf/individualProgression.conf.dist").read_text()
        ip = (ROOT / "src/IndividualProgression.cpp").read_text()
        for setting in (
            "IndividualProgression.F14.ExperimentalRndbotPlayerVisibility",
            "IndividualProgression.F14.ForceVerifiedRandomBotTbcFloorOnLogin",
            "IndividualProgression.PaidExpansion.Enable",
            "IndividualProgression.AutoAttune",
        ):
            self.assertEqual(len(re.findall(
                r"^" + re.escape(setting) + r"\s*=\s*0\s*$", conf, flags=re.M)), 1, setting)
        self.assertIn("!defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION)", ip)
        for forbidden in ("ModifyMoney(", "RewardQuest(", "SaveToDB(", "ForceUpdateProgressionState("):
            self.assertNotIn("+" + forbidden, self.patch)


if __name__ == "__main__":
    unittest.main()
