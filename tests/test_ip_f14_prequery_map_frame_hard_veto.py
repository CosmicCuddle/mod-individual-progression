#!/usr/bin/env python3
"""F14 pre-query map-frame hard veto: disposable Core-only contract tests.

This is a negative/keep-visible gate, not a positive concealment policy.
"""
import ast
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tests/core_patches/apply_f14_prequery_map_frame_hard_veto.py"
PLAYER = "src/server/game/Entities/Player/PlayerUpdates.cpp"
MAP_H = "src/server/game/Maps/Map.h"
MAP_CPP = "src/server/game/Maps/Map.cpp"

QUERY = """    bool const f14BaselineVisible = CanSeeOrDetect(target, false, true);
    bool const f14PositiveSuppression = f14BaselineVisible && target->IsCreature() &&
        !target->ToCreature()->IsPet() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&
        !F14ViewerHasDirectInteractionVeto(this, target->ToCreature());
    if (f14PositiveSuppression)
        sScriptMgr->OnF14BeforeCreatureSuppression(this, target, f14WasLinked, true);
    if (HaveAtClient(target))
        return;
    if (f14PositiveSuppression)
        sScriptMgr->OnF14CreatureSuppressionDecision(this, target, f14WasLinked, true);
"""

def fixture(broken_direct=False, missing_frame=False):
    t = tempfile.TemporaryDirectory()
    root = Path(t.name)
    for name in (PLAYER, MAP_H, MAP_CPP):
        (root / name).parent.mkdir(parents=True, exist_ok=True)
    (root / MAP_H).write_text(
        "class Map {\n[[nodiscard]] bool IsWithinF14MapUpdateInvocation() const;\n};\n"
        if not missing_frame else "class Map {};\n", encoding="utf-8")
    (root / MAP_CPP).write_text(
        "F14ScopedMapUpdateInvocation f14ScopedInvocation(this);\n"
        "return f14CurrentMapUpdateInvocation == this;\n", encoding="utf-8")
    direct = QUERY.replace("f14WasLinked, true", "f14WasLinked, false")
    if broken_direct:
        direct = direct.replace("!F14ViewerHasDirectInteractionVeto", "!MissingDirectVeto")
    (root / PLAYER).write_text(
        '#include "Language.h"\n#include "Player.h"\n'
        'template <class T>\nvoid Player::UpdateVisibilityOf(T* target, UpdateData& data,'
        ' std::vector<Unit*>& visibleNow)\n{\n' + QUERY + '}\n'
        '\nvoid Player::GetInitialVisiblePackets(Unit* target)\n{}\n'
        '\nvoid Player::UpdateVisibilityOf(WorldObject* target)\n{\n'
        + direct + '}\n'
        '\nvoid Player::UpdateTriggerVisibility()\n{}\n',
        encoding="utf-8")
    return t, root

def apply(root):
    return subprocess.run([sys.executable, str(PATCHER), str(root)],
                          text=True, capture_output=True, check=False)

class HardVetoCoreSource(unittest.TestCase):
    def test_python_patcher_syntax(self):
        ast.parse(PATCHER.read_text(encoding="utf-8"))

    def test_two_paths_reject_before_positive_policy_and_viewer_link(self):
        temp, root = fixture()
        with temp:
            result = apply(root)
            self.assertEqual(result.returncode, 0, result.stderr)
            s = (root / PLAYER).read_text(encoding="utf-8")
            self.assertEqual(s.count("GetMap()->IsWithinF14MapUpdateInvocation() &&"), 2)
            self.assertEqual(s.count('include "Map.h"'), 1)
            self.assertEqual(s.count("bool const f14BaselineVisible"), 2)
            self.assertEqual(s.count("bool const f14PositiveSuppression"), 2)
            self.assertEqual(s.count("!F14ViewerHasDirectInteractionVeto"), 2)
            self.assertEqual(s.count("OnF14BeforeCreatureSuppression(this, target"), 2)
            for start, end in (
                ("void Player::UpdateVisibilityOf(T* target, UpdateData& data,",
                 "void Player::GetInitialVisiblePackets(Unit* target)"),
                ("void Player::UpdateVisibilityOf(WorldObject* target)",
                 "void Player::UpdateTriggerVisibility()"),
            ):
                part = s.split(start, 1)[1].split(end, 1)[0]
                self.assertLess(part.index("GetMap()->IsWithinF14MapUpdateInvocation()"),
                                part.index("ShouldF14SuppressCreatureForViewer"))
                self.assertLess(part.index("ShouldF14SuppressCreatureForViewer"),
                                part.index("OnF14BeforeCreatureSuppression"))
                self.assertLess(part.index("OnF14BeforeCreatureSuppression"),
                                part.index("if (HaveAtClient(target))"))
            self.assertNotIn("SetPhaseMask", s)
            self.assertNotIn("CanSeeOrDetect(target, false, false)", s)
            self.assertNotIn("CreateUpdateBlock", s)

    def test_fails_atomic_on_missing_real_core_map_frame(self):
        t, root = fixture(missing_frame=True)
        with t:
            original = (root / PLAYER).read_bytes()
            result = apply(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), original)

    def test_fails_atomic_on_one_missing_visibility_veto(self):
        t, root = fixture(broken_direct=True)
        with t:
            original = (root / PLAYER).read_bytes()
            result = apply(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), original)

    def test_refuses_reapplication_without_change(self):
        t, root = fixture()
        with t:
            self.assertEqual(apply(root).returncode, 0)
            original = (root / PLAYER).read_bytes()
            result = apply(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), original)

    def test_no_real_positive_hide_listener_added_or_claude_ledger_wiring(self):
        source = (ROOT / "src/IndividualProgressionF14LifecycleScript.cpp").read_text()
        self.assertNotIn("OnF14ShouldSuppressCreatureForViewer(", source)
        self.assertNotIn("ObserveProposedBatchFlush(", source)
        self.assertNotIn("ClearPreviouslyHidden(", source)
        self.assertIn("COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT", source)

if __name__ == "__main__":
    unittest.main()
