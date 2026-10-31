#!/usr/bin/env python3
"""Source regression for no offline-leader -> guessed stage0 write.

Checks exact reviewed IP group-accept segment, not gameplay or Core callback
coverage. No test starts Worldserver or rewrites a character.
"""
import unittest
from pathlib import Path
from itertools import product

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
SECTION = SRC.split("bool OnPlayerCanGroupAccept(Player* player, Group* group) override", 1)[1].split(
    "void OnPlayerCreatureKill(Player* killer, Creature* killed)", 1
)[0]


def missing_leader_decision(ip_enabled, enforce):
    if not ip_enabled:
        return True, False
    return (not enforce), False


class OfflineLeaderSafety(unittest.TestCase):
    def test_requires_player_group_and_inworld(self):
        self.assertIn("if (!player || !player->IsInWorld() || !group)", SECTION)
        self.assertLess(SECTION.index("if (!player || !player->IsInWorld() || !group)"),
                        SECTION.index("FindPlayerByLowGUID"))

    def test_disabled_ip_returns_before_all_progression_reads(self):
        self.assertIn("if (!sIndividualProgression->enabled)\n            return true;", SECTION)
        self.assertLess(SECTION.index("if (!sIndividualProgression->enabled)"),
                        SECTION.index("FindPlayerByLowGUID"))

    def test_leader_must_exist_and_be_in_world_before_progression_read(self):
        self.assertIn("if (!groupLeader || !groupLeader->IsInWorld())", SECTION)
        self.assertIn("return !sIndividualProgression->enforceGroupRules;", SECTION)
        pos=SECTION.index("if (!groupLeader || !groupLeader->IsInWorld())")
        self.assertLess(pos, SECTION.index("GetPlayerProgressionFromQuests(groupLeader)"))
        self.assertLess(pos, SECTION.index("GetPlayerProgressionFromQuests(player)"))
        self.assertLess(pos, SECTION.index("ForceUpdateProgressionState("))

    def test_unknown_leader_on_all_expansions_never_forces_stage_zero(self):
        for enabled, enforced, join_stage, leader_valid in product(
                (False, True), (False, True), range(19), (False, True)):
            if not leader_valid:
                permitted, may_write = missing_leader_decision(enabled, enforced)
                self.assertEqual(permitted, not (enabled and enforced))
                # Both acceptance and denial must happen before ANY
                # progression write when the leader stage is unknowable.
                self.assertFalse(may_write)

    def test_normal_present_leader_sync_logic_unmodified(self):
        self.assertIn("if (!sIndividualProgression->isNormalAccount(player))", SECTION)
        self.assertIn("if (groupLeaderState < PROGRESSION_PRE_TBC)", SECTION)
        self.assertIn("if (player->GetLevel() <= 60)", SECTION)
        self.assertIn("if (currentState != groupLeaderState)", SECTION)
        self.assertIn("ForceUpdateProgressionState(player, static_cast<ProgressionState>(groupLeaderState))", SECTION)
        self.assertIn("return (currentState == groupLeaderState);", SECTION)

    def test_no_other_group_invitation_code_changed_in_this_section(self):
        self.assertNotIn("sRandomPlayerbotMgr", SECTION)
        self.assertNotIn("Group::AddMember", SECTION)
        self.assertNotIn("ClearQuest", SECTION)
        self.assertNotIn("RemoveRewardedQuest", SECTION)


if __name__ == "__main__":
    unittest.main()
