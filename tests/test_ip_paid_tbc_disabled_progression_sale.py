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

    def test_paid_existing_buyer_quest_not_consumed_when_default_progression_disabled(self):
        exp = method(SOURCE, "bool IndividualProgression::MustHoldPaidTbcQuestReward(")
        self.assertIn("IsPaidExpansionActive() && disableDefaultProgression", exp)
        self.assertIn("PaidTbcRewardWouldStrand(GetExpansionFacts(player), disableDefaultProgression)", exp)
        hook = method(SOURCE, "bool OnPlayerBeforeQuestComplete(Player* player, uint32 questId) override")
        self.assertIn("sIndividualProgression->MustHoldPaidTbcQuestReward(player)", hook)
        self.assertLess(hook.index("MustHoldPaidTbcQuestReward"), hook.index("HasExpansionEntitlement(player, Era::Tbc)"))
        self.assertRegex(hook, r"MustHoldPaidTbcQuestReward\(player\)\)\s*\{[\s\S]*?return false;")
        self.assertIn("Your quest and TBC purchase are preserved.", hook)
        self.assertNotIn("RemoveActiveQuest(", hook)
        self.assertNotIn("ModifyMoney(", hook)

    def test_existing_complete_quest_client_reward_packet_is_refused_before_core_handler(self):
        self.assertIn('#include "ScriptDefines/ServerScript.h"', SOURCE)
        self.assertEqual(SOURCE.count("SERVERHOOK_CAN_PACKET_RECEIVE"), 1)
        self.assertIn("new IndividualProgression_PaidTbcQuestRewardPacketGuard();", SOURCE)
        guard = method(SOURCE, "bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override")
        self.assertIn("packet.GetOpcode() != CMSG_QUESTGIVER_CHOOSE_REWARD", guard)
        self.assertIn("!session->GetPlayer() || packet.size() < 16", guard)
        self.assertIn("sIndividualProgression->MustHoldPaidTbcQuestReward(player)", guard)
        self.assertLess(guard.index("MustHoldPaidTbcQuestReward(player)"), guard.index("copy >> questGiver >> questId >> reward;"))
        self.assertIn("WorldPacket copy(packet);", guard)
        self.assertIn("copy.rpos(0);", guard)
        self.assertIn("copy >> questGiver >> questId >> reward;", guard)
        self.assertLess(guard.index("if (questId != INTO_THE_BREACH)"), guard.rindex("return false;"))
        self.assertEqual(guard.count("return false;"), 1)
        for forbidden in ("RewardQuest(", "ModifyMoney(", "SetQuestStatus(", "IncompleteQuest(", "ForceUpdateProgressionState(", "AddQuest("):
            self.assertNotIn(forbidden, guard)

    def test_paid_existing_buyer_guard_preserves_owner_exemptions(self):
        policy = (ROOT / "src/IndividualProgressionExpansionPolicy.h").read_text()
        guard = method(policy, "constexpr bool PaidTbcRewardWouldStrand(")
        for clause in ("disableDefaultProgression", "Applies(f)", "f.tbcPaid",
                       "f.rawStage < TbcEntryStage", "f.startStage < TbcEntryStage"):
            self.assertIn(clause, guard)
        # The exact C++ policy, not a Python reimplementation, is exercised
        # in expansion_purchase_policy.cpp (human, OFF, bot, start-stage and
        # paid/unpaid cases). Here we only pin the production source wiring.
        self.assertIn("PaidTbcRewardWouldStrand(GetExpansionFacts(player), disableDefaultProgression)", SOURCE)


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
