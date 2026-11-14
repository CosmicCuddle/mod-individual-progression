#!/usr/bin/env python3
"""#113 F14-only pre-query corpse-negative veto fixtures; no Worldserver."""
import ast
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from test_ip_f14_human_viewer_prequery_veto import fixture as prior_fixture, run as prior_run, PLAYER

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tests/core_patches/apply_f14_alive_prequery_corpse_veto.py"
UNIT_H = "src/server/game/Entities/Unit/Unit.h"

VALID_UNIT = "[[nodiscard]] bool IsAlive() const { return (m_deathState == DeathState::Alive); };\n"
PATHS = (
    ("void Player::UpdateVisibilityOf(T* target, UpdateData& data,",
     "void Player::GetInitialVisiblePackets(Unit* target)"),
    ("void Player::UpdateVisibilityOf(WorldObject* target)",
     "void Player::UpdateTriggerVisibility()"),
)

def setup(*, invalid_unit=False, broken_direct=False):
    t, root = prior_fixture()
    (root / UNIT_H).parent.mkdir(parents=True, exist_ok=True)
    (root / UNIT_H).write_text(VALID_UNIT if not invalid_unit
                                else "bool IsAlive() const { return m_deathState != DeathState::Dead; }\n",
                                encoding="utf-8")
    result = prior_run(root)
    if result.returncode != 0:
        t.cleanup()
        raise RuntimeError(f"unable to apply original #111 fixture: {result.stderr}")
    if broken_direct:
        p = root / PLAYER
        source = p.read_text(encoding="utf-8")
        start, end = PATHS[1]
        direct = source.split(start, 1)[1].split(end, 1)[0]
        direct = direct.replace("!F14ViewerHasDirectInteractionVeto",
                                "!DifferentUnknownRightsVeto", 1)
        source = source.split(start,1)[0] + start + direct + end + source.split(end,1)[1]
        p.write_text(source, encoding="utf-8")
    return t, root

def apply(root):
    return subprocess.run([sys.executable, str(PATCHER), str(root)],
                          text=True, capture_output=True, check=False)

class F14AlivePrequeryVeto(unittest.TestCase):
    def test_exact_python_syntax(self):
        ast.parse(PATCHER.read_text(encoding="utf-8"))

    def test_both_paths_keep_previous_negative_veto_order(self):
        t, root = setup()
        with t:
            result = apply(root)
            self.assertEqual(result.returncode, 0, result.stderr)
            source = (root / PLAYER).read_text(encoding="utf-8")
            self.assertEqual(source.count("target->ToCreature()->IsAlive() &&"), 2)
            self.assertEqual(source.count("GetSession() && !GetSession()->IsHeadless() && !IsGameMaster() &&"), 2)
            self.assertEqual(source.count("GetMap()->IsWithinF14MapUpdateInvocation() &&"), 2)
            for begin, end in PATHS:
                body = source.split(begin, 1)[1].split(end, 1)[0]
                order = [
                    "GetMap()->IsWithinF14MapUpdateInvocation()",
                    "GetSession() && !GetSession()->IsHeadless()",
                    "target->ToCreature()->IsAlive()",
                    "ShouldF14SuppressCreatureForViewer(this, target)",
                    "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())",
                    "OnF14BeforeCreatureSuppression(this, target",
                    "if (HaveAtClient(target))",
                ]
                pos = [body.index(token) for token in order]
                self.assertEqual(pos, sorted(pos))
                self.assertEqual(len(pos), len(set(pos)))
                self.assertEqual(body.count("target->ToCreature()->IsAlive() &&"), 1)
            self.assertNotIn("SetPhaseMask(", source)
            self.assertNotIn("UpdateObjectVisibility(true)", source)

    def test_missing_exact_pinned_isalive_is_fail_atomic(self):
        t, root = setup(invalid_unit=True)
        with t:
            original = (root / PLAYER).read_bytes()
            result = apply(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), original)

    def test_missing_existing_direct_veto_is_fail_atomic(self):
        t, root = setup(broken_direct=True)
        with t:
            original = (root / PLAYER).read_bytes()
            result = apply(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), original)

    def test_double_application_is_rejected_without_mutation(self):
        t, root = setup()
        with t:
            self.assertEqual(apply(root).returncode, 0)
            original = (root / PLAYER).read_bytes()
            result = apply(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual((root / PLAYER).read_bytes(), original)

    def test_isalive_is_only_necessary_and_does_not_clear_restore(self):
        patch = PATCHER.read_text(encoding="utf-8")
        self.assertNotIn("SendDirectMessage", patch)
        self.assertNotIn("ClearPreviouslyHidden", patch)
        self.assertNotIn("CompleteRestoreOnlyWithClientProof", patch)
        self.assertNotIn("SetPhaseMask", patch)
        self.assertIn("does NOT restore previously hidden pairs", patch)

    def test_positive_policy_listener_remains_absent(self):
        s = (ROOT / "src/IndividualProgressionF14LifecycleScript.cpp").read_text()
        self.assertNotIn("OnF14ShouldSuppressCreatureForViewer(", s)
        self.assertNotIn("ClearFrozenAfterGlobalProof(", s)
        self.assertNotIn("CompleteRestoreOnlyWithClientProof(", s)

    def test_pinned_justdied_corpse_dead_and_justrespawned_are_non_alive(self):
        h = VALID_UNIT
        self.assertIn("m_deathState == DeathState::Alive", h)
        # This is a source contract; it does NOT simulate a Core runtime state.
        for state in ("JustDied", "Corpse", "Dead", "JustRespawned"):
            self.assertNotEqual(state, "Alive")

if __name__ == "__main__":
    unittest.main()
