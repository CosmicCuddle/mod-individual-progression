"""Exact-source guard for PURE #97 combat+viewer-restore joint safety gate.

This cannot authorize live F14 visibility: no approved full-human-rights
provider, no client ACK, no gameplay callbacks, and OFF settings.
"""
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT = "d8aa20cb4f59df7caceab7842247f38d003867d5"
CHANGES = {
    "src/IndividualProgressionF14ViewerRightsGate.h",
    "tests/f14_viewer_rights_gate.cpp",
    "tests/test_ip_f14_viewer_rights_gate_source.py",
    ".github/workflows/ip-f14-joint-rights-gate-source-20261105.yml",
    "docs/F14_JOINT_VIEWER_RIGHTS_GATE_2026-11-05.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}

def run(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          capture_output=True, text=True, check=True).stdout.strip()

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

class SourceOnly(unittest.TestCase):
    def test_exact_six_path_child_and_immutable_integrated_runtime(self):
        self.assertEqual(set(run("diff", "--name-only", PARENT, "HEAD").splitlines()), CHANGES)
        self.assertEqual(run("diff", "--name-only", PARENT, "HEAD", "--",
                             "conf", "sql", "data", "optional",
                             "src/IndividualProgression.cpp",
                             "src/IndividualProgressionPlayer.cpp",
                             "src/IndividualProgressionF14LifecycleScript.cpp",
                             "src/IndividualProgressionF14LifecycleRegistry.h",
                             "src/IndividualProgressionF14PriorHiddenLedger.h",
                             "src/IndividualProgressionF14ViewerRestoreProtocol.h"), "")
        self.assertEqual(run("rev-parse", "HEAD:src/IndividualProgressionF14LifecycleRegistry.h"),
                         "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8")
        self.assertEqual(run("rev-parse", "HEAD:src/IndividualProgressionF14ViewerRestoreProtocol.h"),
                         "f520356a8152d98580751604f4649fe1b564f072")
        self.assertEqual(run("rev-parse", "HEAD:src/IndividualProgressionF14PriorHiddenLedger.h"),
                         "2250bae56bb09a810d337927f738bd3cbcf5fddd")
        self.assertEqual(run("rev-parse", "HEAD:tests/f14_viewer_restore_protocol.cpp"),
                         "9bce90ed2c3e0f05dc41583d8e96b4f53f661422")

    def test_new_gate_does_not_create_positive_runtime_hook(self):
        gate = read("src/IndividualProgressionF14ViewerRightsGate.h")
        self.assertIn('#include "IndividualProgressionF14CombatVisibilityPolicy.h"', gate)
        self.assertIn('#include "IndividualProgressionF14ViewerRestoreProtocol.h"', gate)
        self.assertIn("constexpr bool IsLiveCreatureConcealmentAuthorized() { return false; }", gate)
        self.assertIn("independentFullHumanPairRightsProven = false;", gate)
        self.assertIn("exactVerifiedSelectedRandomBotIdentity = false;", gate)
        self.assertIn("sameLiveCreatureIncarnationIndependentlyProven = false;", gate)
        self.assertIn("onConfirmedOwningMapThread = false;", gate)
        self.assertIn("HypotheticalModelOnly", gate)
        self.assertIn("protocol.MayAnswerPositiveSuppression(", gate)
        self.assertIn("Visibility::ShouldConcealCreatureFromObserver(", gate)
        self.assertNotIn("CompleteRestoreOnlyWithClientProof(", gate)
        self.assertNotIn("ClearFrozenAfterGlobalProof(", gate)
        self.assertNotIn("SetPhaseMask(", gate)
        self.assertNotIn("UpdateObjectVisibility(", gate)
        self.assertNotIn("SendDirectMessage(", gate)
        self.assertNotIn("OnF14ShouldSuppressCreatureForViewer(", gate)
        self.assertNotIn("CALL_ENABLED_BOOLEAN_HOOKS_WITH_DEFAULT_FALSE(", gate)
        # No runtime source or CMake file includes / builds the new bridge.
        for path in (ROOT / "src").rglob("*"):
            if not path.is_file() or path.name == "IndividualProgressionF14ViewerRightsGate.h":
                continue
            src = path.read_text(encoding="utf-8", errors="replace")
            self.assertNotIn('"IndividualProgressionF14ViewerRightsGate.h"', src, str(path))
        for path in (ROOT / "CMakeLists.txt", ROOT / "src" / "CMakeLists.txt"):
            if path.is_file():
                self.assertNotIn("f14_viewer_rights_gate.cpp", path.read_text())

    def test_no_paid_or_f14_flags_enabled(self):
        config = read("conf/individualProgression.conf.dist")
        for flag in ("IndividualProgression.PaidExpansion.Enable",
                     "IndividualProgression.F14.ProtectVerifiedRandomBotStageFloor",
                     "IndividualProgression.F14.ExperimentalRndbotPlayerVisibility"):
            self.assertIn(flag + " = 0", config)
        self.assertIn("No raw Core pointers", read("src/IndividualProgressionF14ViewerRightsGate.h"))


if __name__ == "__main__":
    unittest.main()
