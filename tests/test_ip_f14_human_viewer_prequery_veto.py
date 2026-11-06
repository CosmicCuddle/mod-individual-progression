#!/usr/bin/env python3
"""F14 disposable Core exact viewer-session guard fixture tests; NO LIVE HIDE."""
from pathlib import Path
import ast
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tests/core_patches/apply_f14_human_viewer_prequery_veto.py"
PLAYER = "src/server/game/Entities/Player/PlayerUpdates.cpp"
PLAYER_H = "src/server/game/Entities/Player/Player.h"
SESSION_H = "src/server/game/Server/WorldSession.h"
MAP_H = "src/server/game/Maps/Map.h"

def query(batched):
    b = "true" if batched else "false"
    return (
        "    bool const f14BaselineVisible = CanSeeOrDetect(target, false, true);\n"
        "    bool const f14PositiveSuppression = f14BaselineVisible && target->IsCreature() &&\n"
        "        !target->ToCreature()->IsPet() &&\n"
        "        IsInWorld() && target->IsInWorld() && GetMap() &&\n"
        "        GetMap() == target->GetMap() &&\n"
        "        GetMap()->IsWithinF14MapUpdateInvocation() &&\n"
        "        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&\n"
        "        !F14ViewerHasDirectInteractionVeto(this, target->ToCreature());\n"
        "    if (f14PositiveSuppression)\n"
        f"        sScriptMgr->OnF14BeforeCreatureSuppression(this, target, f14WasLinked, {b});\n"
        "    if (HaveAtClient(target))\n"
        "        return;\n"
        "    if (f14PositiveSuppression)\n"
        f"        sScriptMgr->OnF14CreatureSuppressionDecision(this, target, f14WasLinked, {b});\n"
    )

def fixture(*, bad_direct=False, bad_session=False, bad_core=False):
    tmp = tempfile.TemporaryDirectory()
    root = Path(tmp.name)
    for name in (PLAYER, PLAYER_H, SESSION_H, MAP_H):
        (root / name).parent.mkdir(parents=True, exist_ok=True)
    (root / PLAYER_H).write_text(
        "[[nodiscard]] WorldSession* GetSession() const;\n"
        "[[nodiscard]] bool IsGameMaster() const;\n", encoding="utf-8")
    (root / SESSION_H).write_text(
        "[[nodiscard]] bool IsHeadless() const { return _headless; }\n"
        if not bad_session else "bool IsHeadlessNonstandard() const;\n", encoding="utf-8")
    (root / MAP_H).write_text(
        "[[nodiscard]] bool IsWithinF14MapUpdateInvocation() const;\n"
        if not bad_core else "bool UnknownMapFrame() const;\n", encoding="utf-8")
    direct = query(False)
    if bad_direct:
        direct = direct.replace(
            "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())",
            "!F14UnprovenDirectVeto(this, target->ToCreature())"
        )
    (root / PLAYER).write_text(
        '#include "WorldStatePackets.h"\n#include "Map.h"\n'
        'template <class T>\nvoid Player::UpdateVisibilityOf(T* target, UpdateData& data,'
        ' std::vector<Unit*>& visibleNow)\n{\n' + query(True) + '}\n'
        '\nvoid Player::GetInitialVisiblePackets(Unit* target)\n{}\n'
        '\nvoid Player::UpdateVisibilityOf(WorldObject* target)\n{\n' + direct + '}\n'
        '\nvoid Player::UpdateTriggerVisibility()\n{}\n',
        encoding="utf-8"
    )
    return tmp, root

def run(root):
    return subprocess.run([sys.executable, str(PATCHER), str(root)],
                          text=True, capture_output=True, check=False)

class F14ViewerPrequeryVetoFixture(unittest.TestCase):
    def test_patcher_python_syntax(self):
        ast.parse(PATCHER.read_text(encoding="utf-8"))

    def test_both_paths_gate_before_policy_rights_and_link(self):
        t, root = fixture()
        with t:
            result = run(root)
            self.assertEqual(result.returncode, 0, result.stderr)
            s = (root / PLAYER).read_text(encoding="utf-8")
            self.assertEqual(s.count("GetSession() && !GetSession()->IsHeadless() && !IsGameMaster() &&"), 2)
            self.assertEqual(s.count('#include "WorldSession.h"'), 1)
            for start,end in (
                ("void Player::UpdateVisibilityOf(T* target, UpdateData& data,",
                 "void Player::GetInitialVisiblePackets(Unit* target)"),
                ("void Player::UpdateVisibilityOf(WorldObject* target)",
                 "void Player::UpdateTriggerVisibility()"),
            ):
                body = s.split(start,1)[1].split(end,1)[0]
                self.assertLess(body.index("GetMap()->IsWithinF14MapUpdateInvocation()"),
                                body.index("GetSession() && !GetSession()->IsHeadless()"))
                self.assertLess(body.index("GetSession() && !GetSession()->IsHeadless()"),
                                body.index("ShouldF14SuppressCreatureForViewer"))
                self.assertLess(body.index("ShouldF14SuppressCreatureForViewer"),
                                body.index("!F14ViewerHasDirectInteractionVeto"))
                self.assertLess(body.index("!F14ViewerHasDirectInteractionVeto"),
                                body.index("OnF14BeforeCreatureSuppression"))
                self.assertLess(body.index("OnF14BeforeCreatureSuppression"),
                                body.index("if (HaveAtClient(target))"))
            self.assertNotIn("SetPhaseMask(", s)
            self.assertNotIn("SendDirectMessage(", s)

    def test_missing_session_method_fails_without_write(self):
        t, root = fixture(bad_session=True)
        with t:
            old = (root / PLAYER).read_bytes()
            result = run(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), old)

    def test_missing_map_frame_method_fails_without_write(self):
        t, root = fixture(bad_core=True)
        with t:
            old = (root / PLAYER).read_bytes()
            result = run(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), old)

    def test_second_path_missing_existing_rights_veto_refused(self):
        t, root = fixture(bad_direct=True)
        with t:
            old = (root / PLAYER).read_bytes()
            result = run(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), old)

    def test_refuses_repeated_application_without_mutation(self):
        t, root = fixture()
        with t:
            self.assertEqual(run(root).returncode, 0)
            old = (root / PLAYER).read_bytes()
            result = run(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), old)

    def test_no_positive_listener_or_claude_adapter_wiring(self):
        source = (ROOT / "src/IndividualProgressionF14LifecycleScript.cpp").read_text()
        self.assertNotIn("OnF14ShouldSuppressCreatureForViewer(", source)
        self.assertNotIn("ObserveProposedBatchFlush(", source)
        self.assertNotIn("ClearPreviouslyHidden(", source)
        self.assertIn("COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT", source)

if __name__ == "__main__":
    unittest.main()
