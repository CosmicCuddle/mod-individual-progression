"""#103 exact immutable source contract for pure F14 per-GUID send-call adapter.

Does not wire scripts, hide, restore, ACK or mutate original Claude source.
"""
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "aa60e5892d98509d6bcbf62b791eb0c759018766"
CHANGES = {
    "src/IndividualProgressionF14GuidSendAttemptEvidence.h",
    "tests/f14_guid_send_attempt_evidence.cpp",
    "tests/test_ip_f14_guid_send_attempt_evidence_source.py",
    ".github/workflows/ip-f14-guid-send-attempt-evidence-pure-20261105.yml",
    "docs/F14_GUID_SEND_ATTEMPT_EVIDENCE_2026-11-05.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True,
                          text=True, capture_output=True).stdout.strip()

def source(path):
    return (ROOT / path).read_text(encoding="utf-8")

class Contracts(unittest.TestCase):
    def test_exact_six_paths_and_original_protocol_immutable(self):
        self.assertEqual(set(git("diff", "--name-only", BASE, "HEAD").splitlines()), CHANGES)
        self.assertEqual(git("diff", "--name-only", BASE, "HEAD", "--",
                             "conf", "sql", "data", "optional",
                             "src/IndividualProgression.cpp",
                             "src/IndividualProgressionPlayer.cpp",
                             "src/IndividualProgressionF14LifecycleScript.cpp",
                             "src/IndividualProgressionF14LifecycleRegistry.h",
                             "src/IndividualProgressionF14ViewerRestoreProtocol.h",
                             "src/IndividualProgressionF14PriorHiddenLedger.h"), "")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14LifecycleRegistry.h"),
                         "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14ViewerRestoreProtocol.h"),
                         "f520356a8152d98580751604f4649fe1b564f072")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14PriorHiddenLedger.h"),
                         "2250bae56bb09a810d337927f738bd3cbcf5fddd")

    def test_exact_core_source_attribution_and_no_client_ack(self):
        body = source("src/IndividualProgressionF14GuidSendAttemptEvidence.h")
        for needle in (
            '#include "IndividualProgressionF14ViewerRestoreProtocol.h"',
            "sourceAttributedByExactCoreF14Hook = false;",
            "afterActualSessionSendDirectMessageCall = false;",
            "sameOwningMapThread = false;",
            "reportedGuidExactlyMatchesCreature = false;",
            "currentViewerLoginEpoch = 0;",
            "currentCreatureIncarnation = 0;",
            "MissingOrUnverifiedCoreAttribution",
            "StaleOrCrossMapViewerCreature",
            "FrozenGlobalRestore",
            "MissingMatchingPriorF14Suppression",
            "WrongDirectionOrServerStage",
            "NotPendingSameDirectionBatch",
            "ExactTaggedServerSendCallOnly",
            "protocol.Find(viewer, creature)",
            "protocol.NeedsRestore(viewer, creature)",
            "if (protocol.NeedsRestore(viewer, creature))",
            "if (!protocol.NeedsRestore(viewer, creature) ||",
            "protocol.GlobalRestoreRequired()",
            "protocol.ObserveProposedBatchFlush(",
            "constexpr bool HasIndependentlyVerifiedClientPresentation() { return false; }",
            "constexpr bool IsRuntimeF14ConcealmentAuthorized() { return false; }",
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, body)
        for forbidden in (
            "CompleteRestoreOnlyWithClientProof(",
            "ClearFrozenAfterGlobalProof(",
            "SendDirectMessage(", "SetPhaseMask(", "UpdateObjectVisibility(",
            "OnF14ShouldSuppressCreatureForViewer(",
            "CALL_ENABLED_BOOLEAN_HOOKS_WITH_DEFAULT_FALSE(",
        ):
            # Existing documentation may mention SendDirectMessage; only
            # code calls with semicolon are disallowed in the adapter.
            if forbidden == "SendDirectMessage(":
                self.assertNotIn("SendDirectMessage(&", body)
            else:
                self.assertNotIn(forbidden, body)

    def test_not_wired_into_live_module_and_flags_still_off(self):
        h = "IndividualProgressionF14GuidSendAttemptEvidence.h"
        for p in (ROOT / "src").rglob("*.cpp"):
            self.assertNotIn('"' + h + '"', p.read_text(errors="replace"), str(p))
        for p in (ROOT / "src").rglob("*.h"):
            if p.name != h:
                self.assertNotIn('"' + h + '"', p.read_text(errors="replace"), str(p))
        config = source("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", config)
        self.assertIn("IndividualProgression.PaidExpansion.Enable = 0", config)

if __name__ == "__main__":
    unittest.main()
