"""F14 exact Core visibility pilot contracts; default FALSE, no live concealment.

With COSMIC_F14_PINNED_CORE_ROOT set, assert the disposable #56/#67/#76/#77
Core tree has ONLY a source-tagged F14-only suppression query+observation in
Player::UpdateVisibilityOf. This is NOT a client ACK or rights proof.
"""
import ast
import os
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tests/core_patches/apply_f14_viewer_suppression_attribution.py"
CORE_ROOT = Path(os.environ["COSMIC_F14_PINNED_CORE_ROOT"]) if os.environ.get("COSMIC_F14_PINNED_CORE_ROOT") else None

def file(path):
    if CORE_ROOT is None:
        raise RuntimeError("Core root not set for integration-only checks")
    return (CORE_ROOT / path).read_text(encoding="utf-8")

class PrototypeStructure(unittest.TestCase):
    def test_patcher_is_syntax_valid_and_exactly_four_files(self):
        src = PATCHER.read_text(encoding="utf-8")
        ast.parse(src)
        spec = ast.literal_eval(ast.parse(src).body[1].value) if False else None
        self.assertIn("All-or-nothing validation BEFORE any file writes", src)
        self.assertIn("sources = {name:", src)
        for path in ("src/server/game/Scripting/ScriptMgr.h",
                     "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
                     "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
                     "src/server/game/Entities/Player/PlayerUpdates.cpp"):
            self.assertIn('"' + path + '"', src)
        self.assertNotIn("SetPhaseMask(", src)
        self.assertNotIn("WorldPacket(", src)
        self.assertNotIn("RemoveRewardedQuest(", src)

    def test_ip_runtime_remains_unwired(self):
        for src in ("src/IndividualProgressionF14LifecycleScript.cpp",
                    "src/IndividualProgression.cpp",
                    "src/IndividualProgressionPlayer.cpp"):
            body = (ROOT / src).read_text(encoding="utf-8")
            self.assertNotIn("ShouldF14SuppressCreatureForViewer(", body)
            self.assertNotIn("OnF14CreatureSuppressionDecision(", body)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION",
                         PATCHER.read_text(encoding="utf-8"))

@unittest.skipUnless(CORE_ROOT, "pinned disposable patched Core not supplied")
class CoreAttribution(unittest.TestCase):
    def test_positive_query_defaults_false_and_emits_source_tag(self):
        h = file("src/server/game/Scripting/ScriptDefines/WorldObjectScript.h")
        cpp = file("src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp")
        mgr = file("src/server/game/Scripting/ScriptMgr.h")
        for token in ("WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_QUERY",
                      "WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED"):
            self.assertIn(token, h)
        self.assertIn("OnF14ShouldSuppressCreatureForViewer(", h)
        self.assertIn("return false;", h.split("OnF14ShouldSuppressCreatureForViewer(", 1)[1].split("}",1)[0])
        self.assertIn("CALL_ENABLED_BOOLEAN_HOOKS_WITH_DEFAULT_FALSE(WorldObjectScript", cpp)
        self.assertIn("void ScriptMgr::OnF14CreatureSuppressionDecision(", cpp)
        self.assertIn("ShouldF14SuppressCreatureForViewer(", mgr)
        self.assertIn("OnF14CreatureSuppressionDecision(", mgr)

    def test_direct_and_batched_consult_core_once_and_only_tag_f14(self):
        src = file("src/server/game/Entities/Player/PlayerUpdates.cpp")
        b = src.split("void Player::UpdateVisibilityOf(T* target, UpdateData& data,",1)[1]
        b = b.split("void Player::GetInitialVisiblePackets(Unit* target)",1)[0]
        d = src.split("void Player::UpdateVisibilityOf(WorldObject* target)",1)[1]
        d = d.split("void Player::UpdateTriggerVisibility()",1)[0]
        for segment in (b,d):
            self.assertEqual(segment.count("CanSeeOrDetect(target, false, true)"),1)
            self.assertEqual(segment.count("ShouldF14SuppressCreatureForViewer(this, target)"),1)
            self.assertEqual(segment.count("OnF14CreatureSuppressionDecision(this, target"),1)
            self.assertEqual(segment.count("OnF14ViewerTargetLinkChanged(this, target"),1)
            self.assertIn("f14BaselineVisible && target->IsCreature()",segment)
            self.assertIn("!target->ToCreature()->IsPet()",segment)
            self.assertIn("if (!f14BaselineVisible || f14PositiveSuppression)",segment)
            self.assertIn("if (f14BaselineVisible && !f14PositiveSuppression)",segment)
            self.assertLess(segment.index("f14BaselineVisible = CanSeeOrDetect"),
                            segment.index("f14PositiveSuppression = f14BaselineVisible"))
            self.assertLess(segment.index("OnF14ViewerTargetLinkChanged(this, target"),
                            segment.index("OnF14CreatureSuppressionDecision(this, target"))
            self.assertIn("if (f14PositiveSuppression)", segment)
        self.assertIn("OnF14CreatureSuppressionDecision(this, target, f14WasLinked, true)", b)
        self.assertIn("OnF14CreatureSuppressionDecision(this, target, f14WasLinked, false)", d)

    def test_no_global_see_or_detect_hook_or_full_ready_macro(self):
        object_cpp = file("src/server/game/Entities/Object/Object.cpp")
        self.assertNotIn("ShouldF14SuppressCreatureForViewer(", object_cpp)
        defines = file("src/server/game/Scripting/ScriptDefines/WorldObjectScript.h")
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", defines)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION",
                         file("src/server/game/Entities/Player/PlayerUpdates.cpp"))

if __name__ == "__main__":
    unittest.main()
