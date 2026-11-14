"""F14 #106: exact existing #105 lifecycle source + source-attributed decision snapshot.

The real WorldObjectScript callback is diagnostic and macro-guarded; no
positive hide callback exists, no Claude #94 record is written and a
current same-map pointer alone never counts as owning-thread proof.
"""
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "48c0e3613473b2b48cbea4830fdba68b968bdc74"
LIFE = "src/IndividualProgressionF14LifecycleScript.cpp"
HEADER = "src/IndividualProgressionF14SuppressionDecisionIdentity.h"
CHANGED = {
    LIFE, HEADER, "tests/f14_suppression_decision_identity.cpp",
    "tests/test_ip_f14_source_decision_creature_snapshot.py",
    ".github/workflows/ip-f14-source-decision-incarnation-snapshot-20261105.yml",
    "docs/F14_SOURCE_DECISION_INCARNATION_SNAPSHOT_2026-11-05.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True,
                          capture_output=True, text=True).stdout.strip()

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

class Contracts(unittest.TestCase):
    def test_exact_seven_paths_original_claude_ledger_and_paid_not_edited(self):
        self.assertEqual(set(git("diff", "--name-only", BASE, "HEAD").splitlines()), CHANGED)
        self.assertEqual(git("diff", "--name-only", BASE, "HEAD", "--",
                             "conf", "sql", "data", "optional",
                             "src/IndividualProgression.cpp",
                             "src/IndividualProgressionPlayer.cpp",
                             "src/IndividualProgressionExpansion.cpp",
                             "src/IndividualProgressionF14ViewerRestoreProtocol.h",
                             "src/IndividualProgressionF14PriorHiddenLedger.h"), "")
        for path, sha in {
            "src/IndividualProgressionF14LifecycleRegistry.h": "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8",
            "src/IndividualProgressionF14ViewerRestoreProtocol.h": "f520356a8152d98580751604f4649fe1b564f072",
            "src/IndividualProgressionF14PriorHiddenLedger.h": "2250bae56bb09a810d337927f738bd3cbcf5fddd",
            "src/IndividualProgressionF14GuidSendAttemptEvidence.h": "f89e87428ffe173468ad60a3cfcad1b4f6bf1c81",
        }.items():
            with self.subTest(path=path):
                self.assertEqual(git("rev-parse", "HEAD:" + path), sha)

    def test_original_105_lifecycle_reconstructs_byte_identical(self):
        original = git("show", BASE + ":" + LIFE) + "\n"
        now = read(LIFE)
        new_include = '#include "IndividualProgressionF14SuppressionDecisionIdentity.h"\n'
        self.assertEqual(now.count(new_include), 1)
        now = now.replace(new_include, "")
        new_hook = ('             WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTEMPT,\n'
                    '             WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED}) { }')
        old_hook = '             WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTEMPT}) { }'
        self.assertEqual(now.count(new_hook), 1)
        now = now.replace(new_hook, old_hook, 1)

        start = '    // Called ONLY for the Core\'s explicitly F14-attributed positive\n'
        end = '    void OnF14VisibilityBatchOutOfRangeAttempt('
        self.assertEqual(now.count(start), 1)
        self.assertEqual(now.count(end), 1)
        first, last = now.index(start), now.index(end)
        now = now[:first] + now[last:]
        counters = ('    std::atomic<std::uint64_t> _sourceDecisionSnapshots{0};\n'
                    '    std::atomic<std::uint64_t> _rejectedDecisionSnapshots{0};\n')
        self.assertEqual(now.count(counters), 1)
        now = now.replace(counters, "", 1)
        self.assertEqual(now, original)

    def test_real_core_positive_attribution_only_not_generic_link(self):
        code = read(LIFE)
        h = read(HEADER)
        for expected in (
            "WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED",
            "void OnF14CreatureSuppressionDecision(Player const* viewer,",
            "facts.explicitlySourceAttributedF14Decision = true;",
            "facts.viewerLoginEpoch = CurrentHumanViewerEpoch(viewer);",
            "facts.sameLiveCoreMapPointer = viewer && creature &&",
            "std::unique_lock<std::mutex> guard(LifecycleLock(), std::try_to_lock);",
            "facts.creatureIncarnationStamp = StampOf(const_cast<Creature*>(creature));",
            "LifecycleRegistry().Lookup(facts.creatureIncarnationStamp, entry)",
            "facts.registryCreatureFullGuid = entry.key.guidRaw;",
            "facts.registryIncarnationStamp = entry.token;",
            "Decision::Result::CurrentSourceDecisionIdentitySnapshotOnly",
            "_sourceDecisionSnapshots.fetch_add(1, std::memory_order_relaxed)",
        ):
            with self.subTest(expected=expected):
                self.assertIn(expected, code)
        for expected in (
            "constexpr bool ProvesOwningMapThread() { return false; }",
            "constexpr bool ProvesClientVisibilityOrRestoration() { return false; }",
            "constexpr bool AuthorizesConcealment() { return false; }",
            "constexpr bool AuthorizesPriorHiddenLedgerMutation() { return false; }",
            "registryIncarnationStamp != f.creatureIncarnationStamp",
            "f.viewerMapId != f.creatureMapId",
            "f.viewerInstanceId != f.creatureInstanceId",
        ):
            self.assertIn(expected, h)

    def test_negative_only_macro_and_no_original_ledger_or_positive_hide(self):
        code = read(LIFE)
        segment = code.split("    // Called ONLY for the Core's explicitly", 1)[1].split(
            "    void OnF14VisibilityBatchOutOfRangeAttempt(", 1)[0]
        for forbidden in (
            "ObserveProposedBatchFlush(", "ObserveF14SuppressionDecision(",
            "CompleteRestoreOnlyWithClientProof(", "ClearFrozenAfterGlobalProof(",
            "UpdateObjectVisibility(", "SetPhaseMask(", "SendDirectMessage(",
            "OnF14ShouldSuppressCreatureForViewer(",
            "sRandomPlayerbotMgr", "new Player(", "DeleteCreature(",
        ):
            self.assertNotIn(forbidden, segment)
        self.assertIn("#if defined(COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT)", code)
        self.assertIn("COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT >= 1", code)
        config = read("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", config)
        self.assertIn("IndividualProgression.PaidExpansion.Enable = 0", config)
        self.assertNotIn("COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT", config)

if __name__ == "__main__":
    unittest.main()
