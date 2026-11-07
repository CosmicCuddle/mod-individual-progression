#!/usr/bin/env python3
"""Unwired F14 starting-tier model must never silently become live stage writer."""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
H=ROOT/"tests/f14_first_start_eligibility_model.h"
CPP=ROOT/"tests/f14_first_start_eligibility_model.cpp"

class F14FirstStartSourceContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.h=H.read_text()
        cls.cpp=CPP.read_text()
    def test_unwired_staging_only(self):
        self.assertIn("TEST ONLY", self.h)
        for banned in ("ForceUpdateProgressionState(", "RewardQuest(", "GetQuestStatus(",
                       "GetSession(", "IsRandomBot(", "SaveToDB(", "SQL", "WorldSession*"):
            self.assertNotIn(banned, self.h.split("#include <cstdint>",1)[1])
    def test_default_is_safe(self):
        self.assertIn("bool PilotEnabled = false;", self.h)
        self.assertIn("bool ExistingCharacterData = true;", self.h)
        self.assertIn("std::int32_t ProgressionLimit = -1;", self.h)
    def test_first_creation_required(self):
        self.assertIn("f.ExistingCharacterData || !f.GenuineFirstCreationEvidence", self.h)
        self.assertIn("f.SavedStage != 0", self.h)
    def test_headless_verified_special_races_only(self):
        for x in ("!f.PositiveVerifiedRandomBot", "!f.Headless",
                  "(f.Race != 10 && f.Race != 11)"):
            self.assertIn(x, self.h)
    def test_paid_and_cap_safeguards(self):
        self.assertIn("f.PaidEraFeatureEnabled && !f.OwnerApprovedPaidBotException", self.h)
        self.assertIn("f.ProgressionLimit != 0 && f.ProgressionLimit < 8", self.h)
        self.assertIn("f.SavedStage > f.ProgressionLimit", self.h)
    def test_preserve_old_stages_without_writes(self):
        self.assertIn("return Verdict::PreserveEarned;", self.h)
        self.assertIn("existing tier4 requires review", self.cpp)

if __name__=="__main__":
    unittest.main()
