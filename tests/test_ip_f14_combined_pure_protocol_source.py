"""Exact-source F14 #94 protocol imported onto #92 rights/stagefloor source.

These are source-only preservation guards. Not packet, client, or gameplay
proof. The untouched #94 model is also compiled against #92's newer registry.
"""
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "869325e7d78391b6924901699093c58487aec2d8"  # #96 CI-only head
IMPORTED = {
    "src/IndividualProgressionF14PriorHiddenLedger.h":
        "2250bae56bb09a810d337927f738bd3cbcf5fddd",
    "src/IndividualProgressionF14ViewerRestoreProtocol.h":
        "f520356a8152d98580751604f4649fe1b564f072",
    "tests/f14_viewer_restore_protocol.cpp":
        "9bce90ed2c3e0f05dc41583d8e96b4f53f661422",
}
WORKFLOW = ".github/workflows/ip-f14-pure-protocol-on-ip92-source-20261105.yml"
DOC = "docs/F14_VIEWER_PROTOCOL_INTEGRATION_2026-11-05.md"
REGISTER = "docs/CUSTOM_MODIFICATIONS_REGISTER.md"
THIS = "tests/test_ip_f14_combined_pure_protocol_source.py"

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          check=True, capture_output=True, text=True).stdout.strip()

def source(path):
    return (ROOT / path).read_text(encoding="utf-8")

def uncomment(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)

class PreserveSource(unittest.TestCase):
    def test_child_adds_only_its_seven_reviewable_paths(self):
        self.assertEqual(set(git("diff", "--name-only", BASE, "HEAD").splitlines()),
                         {*IMPORTED, WORKFLOW, DOC, REGISTER, THIS})
        self.assertEqual(git("diff", "--name-only", BASE, "HEAD", "--",
                             "conf", "sql", "data", "optional",
                             "src/IndividualProgression.cpp",
                             "src/IndividualProgressionPlayer.cpp",
                             "src/IndividualProgressionF14LifecycleScript.cpp"), "")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14LifecycleRegistry.h"),
                         "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8")
        for path, sha in IMPORTED.items():
            with self.subTest(path=path):
                self.assertEqual(git("rev-parse", "HEAD:" + path), sha)

    def test_pure_protocol_unwired_and_absent_from_normal_build(self):
        for path in (ROOT / "src").rglob("*"):
            if not path.is_file() or path.name in (
                "IndividualProgressionF14ViewerRestoreProtocol.h",
                "IndividualProgressionF14PriorHiddenLedger.h"):
                continue
            body = path.read_text(encoding="utf-8", errors="replace")
            self.assertNotIn('"IndividualProgressionF14ViewerRestoreProtocol.h"', body, str(path))
            self.assertNotIn('"IndividualProgressionF14PriorHiddenLedger.h"', body, str(path))
        cmake = ROOT / "CMakeLists.txt"
        if cmake.exists():
            body = cmake.read_text()
            self.assertNotIn("f14_viewer_restore_protocol.cpp", body)
            self.assertNotIn("IndividualProgressionF14ViewerRestoreProtocol", body)
        header = uncomment(source("src/IndividualProgressionF14ViewerRestoreProtocol.h"))
        for forbidden in ("CompleteRestoreOnlyWithClientProof(", "ClearFrozenAfterGlobalProof(",
                          "SendDirectMessage(", "SetPhaseMask(", "sScriptMgr",
                          "WorldPacket", "ClientAcknowledged"):
            self.assertNotIn(forbidden, header)
        self.assertIn("bool pairRightsProven = false;", header)
        self.assertIn("NotServerObservable", header)

    def test_flags_off_and_no_full_ready_or_positive_core_listener(self):
        conf = source("conf/individualProgression.conf.dist")
        for key in ("IndividualProgression.F14.ProtectVerifiedRandomBotStageFloor",
                    "IndividualProgression.F14.ForceVerifiedRandomBotTbcFloorOnLogin",
                    "IndividualProgression.F14.ExperimentalRndbotPlayerVisibility",
                    "IndividualProgression.PaidExpansion.Enable"):
            self.assertRegex(conf, r"(?m)^" + re.escape(key) + r"\s*=\s*0\s*$")
        for path in (ROOT / "src").rglob("*"):
            if path.is_file():
                text = path.read_text(encoding="utf-8", errors="replace")
                self.assertNotRegex(text, r"#\s*define\s+COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION")
        self.assertNotIn("OnF14ShouldAttributeCreatureCreateAttempt(",
                         source("src/IndividualProgressionF14LifecycleScript.cpp"))


if __name__ == "__main__":
    unittest.main()
