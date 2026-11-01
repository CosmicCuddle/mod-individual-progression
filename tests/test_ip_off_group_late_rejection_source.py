#!/usr/bin/env python3
"""Source-pinned negative witness: ordinary group accept can change IP quests too early.

This test deliberately PASSES when the known historical hazard is present.
It does not fix the hazard, model a real database, or test a running Worldserver.
Do not adapt its assertions to call the existing pre-accept mutation safe.
"""
from __future__ import annotations

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / ".pinned-core-for-off-accept-audit"
IP_PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
IP_RULES = (ROOT / "src/IndividualProgression.cpp").read_text()
CORE_HANDLER = (CORE / "src/server/game/Handlers/GroupHandler.cpp").read_text()
CORE_GROUP = (CORE / "src/server/game/Groups/Group.cpp").read_text()


def between(source: str, start: str, end: str) -> str:
    assert source.count(start) == 1, (start, source.count(start))
    part = source.split(start, 1)[1]
    assert end in part, end
    return part.split(end, 1)[0]


ACCEPT = between(IP_PLAYER, "bool OnPlayerCanGroupAccept(Player* player, Group* group) override",
                 "void OnPlayerCreatureKill(Player* killer, Creature* killed)")
POST_ADD = between(IP_PLAYER, "class IndividualPlayerProgression_GroupScript : public GroupScript",
                   "void AddSC_mod_individual_progression_player()")
SYNC = between(IP_RULES, "void IndividualProgression::SyncBotsProgressionToLeader(Group* group)",
               "void IndividualProgression::checkIPPhasing(Player* player, uint32 newArea)")
HANDLER = between(CORE_HANDLER, "void WorldSession::HandleGroupAcceptOpcode(WorldPacket& recvData)",
                  "void WorldSession::HandleGroupDeclineOpcode")
ADD_MEMBER = between(CORE_GROUP, "bool Group::AddMember(Player* player, uint8 roles /* = 0 */)",
                     "bool Group::RemoveMember(") if "bool Group::RemoveMember(" in CORE_GROUP else (
                         CORE_GROUP.split("bool Group::AddMember(Player* player, uint8 roles /* = 0 */)", 1)[1][:15000])


def normalized(s: str) -> str:
    # Strip C++ line comments only; this is an ordering smoke test, not a C++ parser.
    return re.sub(r"//[^\n]*", "", s)


class RealPinnedSourceOrdering(unittest.TestCase):
    def test_accept_predicate_runs_before_core_later_guards(self):
        h = normalized(HANDLER)
        order = ["OnPlayerCanGroupAccept(", "CONFIG_TRIAL_RESTRICTION_PARTY",
                 "group->GetLeaderGUID() == GetPlayer()->GetGUID()",
                 "group->IsFull()", "group->IsCreated()", "group->AddMember(GetPlayer())"]
        positions = [h.index(x) for x in order]
        self.assertEqual(positions, sorted(positions), list(zip(order, positions)))

    def test_trial_and_full_rejection_do_return_before_actual_add(self):
        h = normalized(HANDLER)
        self.assertLess(h.index("ERR_INVITE_RESTRICTED"), h.index("group->AddMember(GetPlayer())"))
        self.assertLess(h.index("ERR_GROUP_FULL"), h.index("group->AddMember(GetPlayer())"))
        self.assertIn("if (!leader || leader->IsSpectator())", h)
        self.assertIn("group->RemoveAllInvites();", h)

    def test_off_branch_mutates_rewarded_progression_before_core_accept(self):
        a = normalized(ACCEPT)
        self.assertIn("if (sIndividualProgression->enforceGroupRules)", a)
        off = a.split("if (sIndividualProgression->enforceGroupRules)", 1)[1].split(
            "// Preserve the existing OFF pathway", 1)[-1]  # comment may be stripped
        self.assertIn("!sIndividualProgression->isNormalAccount(player)", a)
        self.assertIn("joiningState != leaderState", a)
        self.assertIn("sIndividualProgression->ForceUpdateProgressionState(", a)
        self.assertIn("static_cast<ProgressionState>(leaderState)", a)
        self.assertLess(a.index("ForceUpdateProgressionState("), a.rfind("return true;"))

    def test_on_path_does_not_mutate_quests_in_accept(self):
        a = ACCEPT
        begin = a.index("if (sIndividualProgression->enforceGroupRules)")
        end = a.index("// Preserve the existing OFF pathway", begin)
        self.assertNotIn("ForceUpdateProgressionState(", a[begin:end])

    def test_c3_missing_leader_check_precedes_quest_stage_reads(self):
        a = ACCEPT
        first = a.index("if (!leader || !leader->IsInWorld())")
        self.assertLess(first, a.index("GetPlayerProgressionFromQuests(player)"))
        self.assertIn("return !sIndividualProgression->enforceGroupRules;", a)

    def test_core_add_commits_member_before_post_add_script(self):
        a = ADD_MEMBER
        self.assertLess(a.index("m_memberSlots.push_back(member);"),
                        a.index("sScriptMgr->OnGroupAddMember(this, player->GetGUID());"))
        self.assertLess(a.index("CharacterDatabase.Execute(stmt);"),
                        a.index("sScriptMgr->OnGroupAddMember(this, player->GetGUID());"))

    def test_existing_ip_post_add_sync_invoked(self):
        self.assertIn("void OnAddMember(Group* group, ObjectGuid guid) override", POST_ADD)
        self.assertIn("sIndividualProgression->SyncBotsProgressionToLeader(group);", POST_ADD)

    def test_simple_move_would_drop_off_tier_zero_sync(self):
        # This is the central preservation catch: OFF sync intentionally ignores
        # a stage-0 leader, whereas old accept rewrites stage-N joining bots to 0.
        self.assertIn("if (!refProgress && !enforceGroupRules)", SYNC)
        self.assertIn("return;", SYNC.split("if (!refProgress && !enforceGroupRules)", 1)[1][:90])
        self.assertIn("joiningState != leaderState", ACCEPT)
        self.assertIn("static_cast<ProgressionState>(leaderState)", ACCEPT)

    def test_direct_group_add_is_different_seam(self):
        self.assertNotIn("OnPlayerCanGroupAccept(", ADD_MEMBER)
        self.assertIn("sScriptMgr->OnGroupAddMember(", ADD_MEMBER)


def historical_order_model(*, enforce: bool, normal: bool, leader_present: bool,
                           joining: int, leader: int, late_rejection: bool) -> tuple[int, bool]:
    """A tiny witness of SOURCE ORDER, not a game behavior simulator."""
    if not leader_present:
        return joining, False if enforce else not late_rejection
    # Existing ON accept validates then defers all writes.
    if enforce:
        return joining, not late_rejection
    saved_stage = joining
    if not normal and joining != leader:
        saved_stage = leader  # existing OFF preaccept ForceUpdate
    return saved_stage, not late_rejection


class StageMarkerLossWitness(unittest.TestCase):
    def test_off_late_trial_reject_can_already_have_rewritten_saved_stage(self):
        after, member = historical_order_model(enforce=False, normal=False, leader_present=True,
                                                joining=8, leader=0, late_rejection=True)
        self.assertEqual((after, member), (0, False))

    def test_off_late_full_party_reject_can_already_have_rewritten_saved_stage(self):
        after, member = historical_order_model(enforce=False, normal=False, leader_present=True,
                                                joining=13, leader=7, late_rejection=True)
        self.assertEqual((after, member), (7, False))

    def test_off_success_to_tier_zero_currently_downgrades(self):
        after, member = historical_order_model(enforce=False, normal=False, leader_present=True,
                                                joining=4, leader=0, late_rejection=False)
        self.assertEqual((after, member), (0, True))

    def test_off_normal_human_not_rewritten(self):
        after, member = historical_order_model(enforce=False, normal=True, leader_present=True,
                                                joining=8, leader=0, late_rejection=True)
        self.assertEqual((after, member), (8, False))

    def test_enforced_branch_never_writes_before_accept(self):
        after, member = historical_order_model(enforce=True, normal=False, leader_present=True,
                                                joining=8, leader=0, late_rejection=True)
        self.assertEqual((after, member), (8, False))

    def test_missing_leader_c3_does_not_mutate_saved_stage(self):
        after, member = historical_order_model(enforce=False, normal=False, leader_present=False,
                                                joining=8, leader=0, late_rejection=True)
        self.assertEqual((after, member), (8, False))


if __name__ == "__main__":
    unittest.main()
