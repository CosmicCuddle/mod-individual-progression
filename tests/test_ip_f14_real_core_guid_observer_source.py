"""F14 #104 real Core tagged-GUID hook observer: five-path default-OFF contract.

The new WorldObjectScript is compiled and registered ONLY with an explicit
CI build macro. It never claims a prior-hidden pair, invokes Claude's #94
protocol, grants hide, creates a packet, or stores GUID/session data.
"""
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "b2b546e490d2ce5966dce610086b32e6ad3012b7"
FILE = "src/IndividualProgressionF14LifecycleScript.cpp"
CHANGED = {
    FILE,
    "tests/test_ip_f14_real_core_guid_observer_source.py",
    ".github/workflows/ip-f14-real-core-guid-observer-native-20261105.yml",
    "docs/F14_REAL_CORE_GUID_OBSERVER_2026-11-05.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}
MACRO = ("defined(COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT) && "
         "COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT >= 1")

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], text=True,
                          capture_output=True, check=True).stdout.strip()

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

class Contracts(unittest.TestCase):
    def test_five_paths_and_immutable_old_protocol_and_paid_source(self):
        self.assertEqual(set(git("diff", "--name-only", BASE, "HEAD").splitlines()), CHANGED)
        self.assertEqual(git("diff", "--name-only", BASE, "HEAD", "--",
                             "conf", "sql", "data", "optional",
                             "src/IndividualProgression.cpp",
                             "src/IndividualProgressionExpansion.cpp",
                             "src/IndividualProgressionF14ViewerRestoreProtocol.h",
                             "src/IndividualProgressionF14PriorHiddenLedger.h"), "")
        blobs = {
            "src/IndividualProgressionF14LifecycleRegistry.h": "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8",
            "src/IndividualProgressionF14ViewerRestoreProtocol.h": "f520356a8152d98580751604f4649fe1b564f072",
            "src/IndividualProgressionF14PriorHiddenLedger.h": "2250bae56bb09a810d337927f738bd3cbcf5fddd",
        }
        for path, blob in blobs.items():
            self.assertEqual(git("rev-parse", "HEAD:" + path), blob)

    def test_original_gameplay_source_byte_for_byte_after_unapplying_observer(self):
        original = git("show", BASE + ":" + FILE) + "\n"
        current = read(FILE)
        include = ('#if ' + MACRO + '\n#include "WorldObjectScript.h"\n'
                   '#include <atomic>\n#endif\n')
        self.assertEqual(current.count(include), 1)
        current = current.replace('#include "ScriptMgr.h"\n' + include,
                                  '#include "ScriptMgr.h"\n', 1)
        marker = "// This is real Core -> IP event wiring, but STRICTLY OBSERVER-ONLY."
        self.assertEqual(current.count(marker), 1)
        start = current.index(marker)
        tail = "\n#endif\n#endif\n\nvoid AddSC_mod_individual_progression_f14_lifecycle()"
        self.assertEqual(current.count(tail), 1)
        end = current.index(tail, start) + len("\n#endif")
        current = current[:start].rstrip("\n") + current[end:]
        new_registration = ('#if ' + MACRO + '\n'
                            '    new IndividualProgressionF14TaggedGuidObserverScript();\n'
                            '#endif\n')
        self.assertEqual(current.count(new_registration), 1)
        current = current.replace(new_registration, "")
        self.assertEqual(current, original, "Existing runtime lifecycle must stay identical")

    def test_exact_core_hooks_but_no_positive_policies_or_ledger_mutations(self):
        source = read(FILE)
        self.assertIn('WorldObjectScript("IndividualProgression_F14TaggedGuidObserver"', source)
        self.assertIn('WORLDOBJECTHOOK_ON_F14_BATCH_OUT_OF_RANGE_ATTEMPT', source)
        self.assertIn('WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTRIBUTION_QUERY', source)
        self.assertIn('WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTEMPT', source)
        self.assertIn('void OnF14VisibilityBatchOutOfRangeAttempt(', source)
        self.assertIn('void OnF14VisibilityBatchCreateAttempt(', source)
        self.assertIn('bool OnF14ShouldAttributeCreatureCreateAttempt(', source)
        self.assertIn('return false;', source.split(
            'bool OnF14ShouldAttributeCreatureCreateAttempt(', 1)[1].split(
            'void OnF14VisibilityBatchOutOfRangeAttempt(', 1)[0])
        self.assertNotIn('OnF14ShouldSuppressCreatureForViewer(', source)
        part = source.split("// This is real Core -> IP event wiring", 1)[1]
        for forbidden in (
            'ObserveProposedBatchFlush(', 'ObserveF14SuppressionDecision(',
            'ClearFrozenAfterGlobalProof(', 'CompleteRestoreOnlyWithClientProof(',
            'SendDirectMessage(', 'SetPhaseMask(', 'UpdateObjectVisibility(',
            'SpellCast(', 'sIndividualProgression->',
        ):
            self.assertNotIn(forbidden, part)
        self.assertIn('std::atomic<std::uint64_t> _outAttempts{0};', source)
        self.assertIn('std::atomic<std::uint64_t> _createAttempts{0};', source)
        self.assertIn('std::atomic<std::uint64_t> _invalidAttempts{0};', source)
        self.assertIn('if (invalidCount.fetch_add(1, std::memory_order_relaxed) == 0)', source)
        self.assertIn('if (count.fetch_add(1, std::memory_order_relaxed) == 0)', source)
        self.assertNotIn('std::vector<ObjectGuid>', part)
        self.assertNotIn('std::map<ObjectGuid', part)

    def test_feature_default_off_and_unchanged_claude_is_unwired(self):
        conf = read("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", conf)
        self.assertIn("IndividualProgression.PaidExpansion.Enable = 0", conf)
        self.assertNotIn("COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT", conf)
        self.assertNotIn("IndividualProgressionF14GuidSendAttemptEvidence.h", read(FILE))
        self.assertEqual(read(FILE).count("#if " + MACRO), 3)
        self.assertNotIn("IndividualProgressionF14GuidSendAttemptEvidence.h",
                         read("src/IndividualProgression.cpp"))

if __name__ == "__main__":
    unittest.main()
