#!/usr/bin/env python3
"""Regression of missing-leader safety ported to combined Hunter + expansion grouping.

Exact source contract only; does not run Worldserver, exercise runtime
Playerbots direct-add, or replace manual group/quest acceptance tests.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
ACCEPT = SOURCE.split("bool OnPlayerCanGroupAccept(Player* player, Group* group) override", 1)[1].split(
    "void OnPlayerCreatureKill(Player* killer, Creature* killed)", 1
)[0]
SYNC = (ROOT / "src/IndividualProgression.cpp").read_text()
HUNTER = (ROOT / "src/IndividualProgressionSpells.cpp").read_text()


class CombinedSourceSafety(unittest.TestCase):
    def test_disabled_path_unchanged(self):
        self.assertIn("if (!sIndividualProgression->enabled)\n            return true;", ACCEPT)

    def test_unknown_leader_checked_before_any_saved_quest_stage_read(self):
        self.assertIn("if (!leader || !leader->IsInWorld())", ACCEPT)
        self.assertIn("return !sIndividualProgression->enforceGroupRules;", ACCEPT)
        first = ACCEPT.index("if (!leader || !leader->IsInWorld())")
        self.assertLess(first, ACCEPT.index("GetPlayerProgressionFromQuests(player)"))
        self.assertLess(first, ACCEPT.index("GetPlayerProgressionFromQuests(leader)"))
        self.assertLess(first, ACCEPT.index("ForceUpdateProgressionState"))

    def test_enforced_same_expansion_policy_preserved(self):
        self.assertIn("IndividualProgressionGroupPolicy::MayGroup(", ACCEPT)
        self.assertIn("for (auto const& slot : group->GetMemberSlots())", ACCEPT)
        self.assertIn("if (!member || !member->IsInWorld())", ACCEPT)
        self.assertIn("return false;", ACCEPT)

    def test_on_does_not_mutate_quests_during_accept(self):
        begin = ACCEPT.index("if (sIndividualProgression->enforceGroupRules)")
        end = ACCEPT.index("// Preserve the existing OFF pathway", begin)
        self.assertNotIn("ForceUpdateProgressionState(", ACCEPT[begin:end])
        self.assertIn("return true;", ACCEPT[begin:end])

    def test_off_real_leader_still_syncs_to_its_tier(self):
        self.assertIn("if (!sIndividualProgression->isNormalAccount(player) && joiningState != leaderState)", ACCEPT)
        self.assertIn("ForceUpdateProgressionState(", ACCEPT)
        self.assertIn("static_cast<ProgressionState>(leaderState)", ACCEPT)

    def test_existing_post_add_member_still_owns_legal_bot_sync(self):
        self.assertIn("SyncBotsProgressionToLeader(group)", SOURCE)
        self.assertIn("void IndividualProgression::SyncBotsProgressionToLeader(Group* group)", SYNC)
        self.assertIn("IndividualProgressionGroupPolicy::MayGroup(", SYNC)

    def test_hunter_autocast_source_is_present_without_behavioral_claim(self):
        self.assertIn("ToggleAutocast(", HUNTER)
        self.assertIn("checkHunterPetSpells", HUNTER)


if __name__ == "__main__":
    unittest.main()
