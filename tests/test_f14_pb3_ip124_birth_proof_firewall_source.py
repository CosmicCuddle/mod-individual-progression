#!/usr/bin/env python3
"""Cross-owner test-only trust boundary: PB #3 does not prove durable IP birth.

No server, SQL, paid purchase, visibility, stage write or state mutation.
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODEL = ROOT / "tests/f14_first_start_eligibility_model.h"
BRIDGE = ROOT / "tests/f14_pb3_ip124_birth_proof_firewall.cpp"


class PlayerbotsBirthTrustFirewall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model = MODEL.read_text(encoding="utf-8")
        cls.bridge = BRIDGE.read_text(encoding="utf-8")

    def test_uses_real_owner_headers_not_copies(self):
        self.assertIn('#include "F14NewRandomBotBirthProbe.h"', self.bridge)
        self.assertIn('#include "f14_first_start_eligibility_model.h"', self.bridge)
        self.assertIn('NewRandomBotBirthProbe::Instance()', self.bridge)
        self.assertIn('probe.HasUnretiredAdmission(guid, account, race)', self.bridge)
        self.assertIn('Decide(OnlyDiagnosticEvidence(', self.bridge)

    def test_diagnostic_never_automatically_proves_birth(self):
        self.assertIn('facts.GenuineFirstCreationEvidence = false;', self.bridge)
        self.assertIn('facts.ExistingCharacterData = true;', self.bridge)
        self.assertIn('f.ExistingCharacterData || !f.GenuineFirstCreationEvidence',
                      self.model)
        self.assertIn('if (f.ExistingCharacterData || !f.GenuineFirstCreationEvidence)',
                      self.model)
        self.assertIn('hypothetical.GenuineFirstCreationEvidence = true;', self.bridge)
        self.assertIn('hypothetical.ExistingCharacterData = false;', self.bridge)

    def test_keeps_stage0_to_7_and_earned8_plus(self):
        for marker in (
            'for (unsigned saved = 0; saved < 8; ++saved)',
            'Decide(f) != Verdict::EligibleNew',
            'Verdict::PreserveEarned',
            'saved stage above current cap is not rewritten',
            'Vanilla cap never permits selected TBC start',
        ):
            self.assertIn(marker, self.bridge)

    def test_replay_and_paid_are_negative(self):
        for marker in (
            'probe.RecordCandidate(guid, account, be)',
            'probe.ObserveAdmission(guid, account, be, true, true, true, true)',
            'probe.Retire(guid)',
            'relog cannot replay retired admission',
            'paid expansion feature cannot be bypassed',
            'hypothetical paid exception cannot replace birth proof',
            'normal human character excluded',
        ):
            self.assertIn(marker, self.bridge)

    def test_no_runtime_or_storage_or_paid_mutation(self):
        executable = self.bridge.split('int main()', 1)[-1]
        for forbidden in (
            'ForceUpdateProgressionState(', 'RewardQuest(', 'CompleteQuest(',
            'SaveToDB(', 'SetQuestStatus(', 'SetPhaseMask(', 'UpdatePlayerSetting(',
            'CharacterDatabase.', 'WorldDatabase.', 'SendUpdateToPlayer(',
        ):
            self.assertNotIn(forbidden, executable)
        self.assertIn('TEST-ONLY', self.bridge)
        self.assertIn('bool PilotEnabled = false;', self.model)


if __name__ == '__main__':
    unittest.main()
