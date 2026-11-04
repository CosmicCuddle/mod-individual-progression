"""F14 direct/batched per-viewer Core server-link hook prototype contracts.

Read-only source checks; *not* client acknowledgement, concealment or deployment.
Real pinned Core patch apply and full native compile are separate CI steps.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = (ROOT / "tests/core_patches/f14_per_viewer_server_link_observed_prototype.patch").read_text()
DOC = (ROOT / "docs/F14_PER_VIEWER_SERVER_LINK_PROTOTYPE_2026-11-04.md").read_text()
HOOK = "OnF14ViewerTargetLinkChanged"


class ViewerLinkPrototype(unittest.TestCase):
    def test_scope_and_order(self):
        files = re.findall(r"^diff --git a/(\S+) b/\1$", PATCH, re.M)
        self.assertEqual(files, [
            "src/server/game/Scripting/ScriptMgr.h",
            "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
            "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
            "src/server/game/Entities/Player/PlayerUpdates.cpp",
        ])
        self.assertEqual(len(re.findall(r"^@@ ", PATCH, re.M)), 8)
        self.assertIn("AFTER original Core #56 + Core after-pass #67", PATCH)

    def test_deny_and_complete_refresh_remain_unimplemented(self):
        for forbidden in ("+#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION",
                          "ShouldConcealCreatureFromObserver(",
                          "DestroyForPlayer(this);\n+",
                          "UpdateObjectVisibility(true);\n+",
                          "SetPhaseMask(",
                          "SendDirectMessage("):
            self.assertNotIn(forbidden, PATCH)
        self.assertNotRegex(PATCH, r"(?m)^\+\s*bool\s+OnWorldObjectCanBeSeenByPlayer\(")

    def test_viewer_and_target_identity_and_batched_indicator(self):
        self.assertIn("WORLDOBJECTHOOK_ON_F14_VIEWER_TARGET_LINK_CHANGED", PATCH)
        self.assertIn("void ScriptMgr::" + HOOK + "(Player const* observer, WorldObject const* target, bool nowLinked, bool batched)", PATCH)
        self.assertIn("script->" + HOOK + "(observer, target, nowLinked, batched)", PATCH)
        self.assertIn("virtual void " + HOOK + "(Player const* /*observer*/, WorldObject const* /*target*/,", PATCH)

    def test_both_live_and_batched_visibility_are_instrumented(self):
        self.assertEqual(PATCH.count("sScriptMgr->" + HOOK + "(this, target, HaveAtClient(target),"), 2)
        self.assertIn("sScriptMgr->" + HOOK + "(this, target, HaveAtClient(target), true);", PATCH)
        self.assertIn("sScriptMgr->" + HOOK + "(this, target, HaveAtClient(target), false);", PATCH)
        self.assertEqual(PATCH.count("f14WasLinked != HaveAtClient(target)"), 2)
        self.assertEqual(PATCH.count("bool const f14Reportable = target->IsPlayer() ||"), 2)
        self.assertEqual(PATCH.count("target->IsCreature() && !target->ToCreature()->IsPet()"), 2)

    def test_changed_link_only_no_packet_ack(self):
        self.assertIn("This is not proof a WoW 3.3.5a client rendered the packet.", PATCH)
        self.assertIn("Batched visibility link bookkeeping precedes actual packet building.", PATCH)
        self.assertIn("No verified permission to hide", DOC)
        self.assertIn("client acknowledgement", DOC)
        self.assertIn("default ALLOW", DOC)

    def test_patch_lines_whitespace_clean(self):
        for line in PATCH.splitlines():
            self.assertFalse(line.endswith((" ", "\t")), line)
        self.assertEqual(len(PATCH.splitlines()), len([s for s in PATCH.splitlines() if s]))

if __name__ == "__main__":
    unittest.main()
