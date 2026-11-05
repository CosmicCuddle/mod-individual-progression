"""Pure, UNWIRED F14 prior-hidden ledger invariants. No runtime permission."""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "src/IndividualProgressionF14PriorHiddenLedger.h").read_text()
SCRIPT = (ROOT / "src/IndividualProgressionF14LifecycleScript.cpp").read_text()
class PriorHiddenSafety(unittest.TestCase):
    def test_value_only_records_explicit_f14_proof(self):
        for required in ("ViewerKey", "loginEpoch", "Registry::CreatureKey", "incarnation",
                         "positivelyProvenF14Suppression", "std::array<SuppressedPair, Capacity>",
                         "_poisoned = true;", "RequestGlobalRestore();",
                         "CompleteRestoreOnlyWithClientProof", "separatelyVerifiedClientRestore",
                         "RevokeViewer", "RevokeCreature"):
            self.assertIn(required, H)
    def test_no_live_wiring_or_concealment(self):
        self.assertNotIn('#include "IndividualProgressionF14PriorHiddenLedger.h"', SCRIPT)
        self.assertNotIn("RecordConfirmedF14Suppression(", SCRIPT)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", H)
        for dangerous in ("WorldPacket", "SendPacket(", "SetPhaseMask(", "RemoveRewardedQuest(",
                          "sRandomPlayerbotMgr", "UpdateObjectVisibility("):
            self.assertNotIn(dangerous, H.split("namespace IndividualProgressionF14PriorHiddenLedger", 1)[1])
    def test_overflow_never_wipes_old_pairs(self):
        before = H.split("if (_size == Capacity)", 1)[1].split("_pairs[_size++]",1)[0]
        self.assertIn("_poisoned = true;", before)
        self.assertIn("RequestGlobalRestore();", before)
        self.assertNotIn("_size = 0", before)
        self.assertNotIn("_pairs = {}", before)
    def test_restore_requires_external_positive_evidence(self):
        method = H.split("bool CompleteRestoreOnlyWithClientProof",1)[1].split("bool ClearFrozenAfterGlobalProof",1)[0]
        self.assertIn("if (!separatelyVerifiedClientRestore", method)
        self.assertIn("_pairs[i].restorePending", method)
        self.assertIn("_pairs[i].incarnation == originalIncarnation", method)
if __name__ == "__main__":
    unittest.main()
