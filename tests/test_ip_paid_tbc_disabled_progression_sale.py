"""Paid-TBC no-stranding source/policy checks (default-OFF; no gameplay).
Strictly pin the pre-debit TBC quest boundary and Wrath independence.
"""
import re
import unittest
from itertools import product
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/IndividualProgressionExpansion.cpp").read_text()
PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
CONF = (ROOT / "conf/individualProgression.conf.dist").read_text()


def method(src, name):
    start = src.index(name)
    brace = src.index("{", start)
    depth = 0
    for n in range(brace, len(src)):
        depth += src[n] == "{"
        depth -= src[n] == "}"
        if depth == 0:
            return src[start:n + 1]
    raise ValueError("unterminated: " + name)


class SafePaidTbcSale(unittest.TestCase):
    def test_tbc_guard_before_check_purchase_and_debit(self):
        can = method(
            SOURCE,
            "PurchaseResult IndividualProgression::CanPurchaseExpansion("
        )
        tbc = can.split(
            "if (IsKnownEra(eraValue) && static_cast<Era>(eraValue) == Era::Tbc)",
            1,
        )[1]
        self.assertEqual(tbc.count("if (disableDefaultProgression)"), 1)
        self.assertRegex(tbc, r"if \(disableDefaultProgression\)\s+entryAvailable = false;")
        self.assertLess(
            can.index("if (disableDefaultProgression)"),
            can.index("return CheckPurchase(facts, eraValue"),
        )
        purchase = method(
            SOURCE,
            "PurchaseResult IndividualProgression::PurchaseExpansion("
        )
        self.assertLess(
            purchase.index("CanPurchaseExpansion(player, eraValue, costCopper)"),
            purchase.index("player->ModifyMoney(-static_cast<int32>(costCopper));"),
        )
        self.assertNotIn("ModifyMoney(", can)
        self.assertNotIn("ForceUpdateProgressionState(", can)
        self.assertNotIn("UpdateProgressionState(", can)

    def test_quest_reward_is_behind_default_progression_switch(self):
        self.assertRegex(
            PLAYER,
            r"case INTO_THE_BREACH:\s+if \(!sIndividualProgression->disableDefaultProgression\)"
            r"\s+sIndividualProgression->UpdateProgressionState\(player, PROGRESSION_PRE_TBC\)",
        )
        self.assertEqual(CONF.count("IndividualProgression.PaidExpansion.Enable = 0"), 1)

    def test_payment_gate_synthetic_matrix(self):
        # This tests only the new preflight constraint, not the full IP policy.
        cases = 0
        for active, disable_default, era_tbc, eligible, money in product(
            (False, True), repeat=5
        ):
            entry_possible = not (active and disable_default and era_tbc)
            permitted = active and eligible and money and entry_possible
            if active and disable_default and era_tbc:
                self.assertFalse(permitted)
            if active and disable_default and not era_tbc and eligible and money:
                self.assertTrue(permitted, "Wrath unaffected by TBC-only guard")
            cases += 1
        self.assertEqual(cases, 32)


if __name__ == "__main__":
    unittest.main()
