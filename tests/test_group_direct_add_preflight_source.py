#!/usr/bin/env python3
"""Pinned source/side-effect audit for Playerbots direct AddMember (NO server fix).

Source refs are checked out independently in GitHub Actions. On production
branches, failing this checker means RE-AUDIT SOURCE rather than deploy blindly.
"""
from __future__ import annotations

import os
from pathlib import Path
import unittest


CORE = Path(os.environ.get("PINNED_CORE_ROOT", "historical-core"))
BOTS = Path(os.environ.get("PINNED_PLAYERBOTS_ROOT", "historical-playerbots"))


def between(source: str, start: str, stop: str) -> str:
    assert source.count(start) == 1, ("ambiguous or missing start", start)
    chunk = source.split(start, 1)[1]
    assert stop in chunk, ("missing boundary; review source drift", stop)
    return chunk.split(stop, 1)[0]


def direct_operation(source: str) -> str:
    return between(source, "class GroupInviteOperation : public PlayerbotOperation",
                   "ObjectGuid GetBotGuid() const override")


def core_create(source: str) -> str:
    return between(source, "bool Group::Create(Player* leader)",
                   "bool Group::LoadGroupFromDB(")


def core_add(source: str) -> str:
    return between(source, "bool Group::AddMember(Player* player, uint8 roles",
                   "bool Group::")


def check_order(section: str, *tokens: str) -> bool:
    """All tokens once in the intended order; conservatively fail on drift."""
    positions = []
    for token in tokens:
        if section.count(token) != 1:
            return False
        positions.append(section.index(token))
    return positions == sorted(positions) and len(set(positions)) == len(positions)


def proposed_preflight_model(*, permitted: bool, group_exists: bool,
                             member_count: int, enforcement: bool) -> tuple[str, ...]:
    """Expectation only: no C++ hook, database writes, or real group operation."""
    operations = []
    if not enforcement:
        operations.append("legacy_off")
    else:
        operations.append("preflight")
        if not permitted:
            return tuple(operations + ["reject"])
    if not group_exists:
        operations.append("create")
    if member_count >= 5:
        operations.append("convert_to_raid")
    operations.append("add_member")
    return tuple(operations)


class PinnedSourceAudit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bot = (BOTS / "src/Script/WorldThr/PlayerbotOperations.h").read_text()
        cls.login = (BOTS / "src/Bot/PlayerbotMgr.cpp").read_text()
        cls.invite = (BOTS / "src/Ai/Base/Actions/InviteToGroupAction.cpp").read_text()
        cls.accept = (BOTS / "src/Ai/Base/Actions/AcceptInvitationAction.cpp").read_text()
        cls.bg = (BOTS / "src/Ai/Base/Actions/BattleGroundJoinAction.cpp").read_text()
        cls.group = (CORE / "src/server/game/Groups/Group.cpp").read_text()
        cls.hooks = (CORE / "src/server/game/Scripting/ScriptDefines/GroupScript.h").read_text()

    def test_direct_login_rejoin_is_not_packet_invite(self):
        self.assertIn("std::make_unique<GroupInviteOperation>", self.login)
        self.assertIn("QueueOperation(std::move(addOp))", self.login)
        self.assertIn("QueueOperation(std::move(inviteOp))", self.login)
        operation = direct_operation(self.bot)
        self.assertIn("group->AddMember(target)", operation)
        self.assertNotIn("HandleGroupInviteOpcode(", operation)
        self.assertNotIn("HandleGroupAcceptOpcode(", operation)
        self.assertNotIn("OnPlayerCanGroupInvite(", operation)
        self.assertNotIn("OnPlayerCanGroupAccept(", operation)

    def test_group_creation_and_raid_change_precede_direct_add(self):
        self.assertTrue(check_order(direct_operation(self.bot),
                         "group = new Group;", "group->Create(bot)",
                         "sGroupMgr->AddGroup(group)", "group->ConvertToRaid()",
                         "group->AddMember(target)"))

    def test_core_create_has_nonvetoable_leader_seed_and_persistence(self):
        self.assertTrue(check_order(core_create(self.group),
                         "CharacterDatabase.Execute(stmt)", "ASSERT(AddMember(leader))",
                         "sScriptMgr->OnCreate(this, leader)"))
        self.assertIn("else if (!AddMember(leader))", core_create(self.group))

    def test_core_post_add_callback_cannot_prevent_membership(self):
        section = core_add(self.group)
        self.assertTrue(check_order(section,
                         "m_memberSlots.push_back(member)",
                         "sCharacterCache->UpdateCharacterGroup(",
                         "CharacterDatabase.Execute(stmt)", "SendUpdate()",
                         "sScriptMgr->OnGroupAddMember("))
        self.assertNotIn("OnPlayerCanGroupAccept(", section)

    def test_existing_group_script_has_no_approved_preinsert_veto(self):
        self.assertIn("virtual void OnAddMember(", self.hooks)
        self.assertIn("virtual void OnInviteMember(", self.hooks)
        self.assertNotIn("virtual bool CanAddMember(", self.hooks)
        self.assertNotIn("virtual bool CanGroupAddMember(", self.hooks)

    def test_ordinary_invite_and_accept_remain_packet_based_controls(self):
        self.assertIn("HandleGroupInviteOpcode(p)", self.invite)
        self.assertIn("HandleGroupAcceptOpcode(p)", self.accept)

    def test_arena_direct_path_is_separately_scoped(self):
        self.assertIn("newGroup->AddMember(member)", self.bot)
        self.assertIn("group->AddMember(member)", self.bg)
        self.assertIn("leaderGroup->Disband(true)", self.bot)
        # Arena/BG policy isn't owner-selected. Do not modify these paths.

    def test_mutation_missing_direct_add_is_detected(self):
        section = direct_operation(self.bot)
        mutated = section.replace("group->AddMember(target)",
                                  "group->HandleGroupInviteOpcode(target)")
        self.assertNotIn("group->AddMember(target)", mutated)

    def test_mutation_moved_preadd_hook_does_not_pass_order(self):
        section = core_add(self.group)
        self.assertTrue(check_order(section, "m_memberSlots.push_back(member)",
                                    "sScriptMgr->OnGroupAddMember("))
        mutant = section.replace("sScriptMgr->OnGroupAddMember(this, player->GetGUID());", "")
        mutant = mutant.replace("m_memberSlots.push_back(member);",
                                "sScriptMgr->OnGroupAddMember(this, player->GetGUID());\n    m_memberSlots.push_back(member);")
        self.assertFalse(check_order(mutant, "m_memberSlots.push_back(member)",
                                     "sScriptMgr->OnGroupAddMember("))

    def test_mutation_group_conversion_after_add_detected(self):
        section = direct_operation(self.bot)
        mutated = section.replace("group->ConvertToRaid();", "GROUP_CONVERT_MARKER;")
        mutated = mutated.replace("group->AddMember(target)",
                                  "group->ConvertToRaid(); group->AddMember(target)")
        self.assertFalse(check_order(mutated, "group->ConvertToRaid()",
                                     "group->AddMember(target)"))

    def test_model_denial_has_no_group_or_raid_or_member_side_effect(self):
        for existing in (True, False):
            for count in (1, 4, 5, 6, 40):
                with self.subTest(group_exists=existing, count=count):
                    self.assertEqual(
                        proposed_preflight_model(permitted=False,
                                                 group_exists=existing,
                                                 member_count=count,
                                                 enforcement=True),
                        ("preflight", "reject"))

    def test_model_permitted_flow_keeps_seed_and_raid_actions(self):
        self.assertEqual(
            proposed_preflight_model(permitted=True, group_exists=False,
                                     member_count=1, enforcement=True),
            ("preflight", "create", "add_member"))
        self.assertEqual(
            proposed_preflight_model(permitted=True, group_exists=True,
                                     member_count=5, enforcement=True),
            ("preflight", "convert_to_raid", "add_member"))

    def test_off_remains_legacy_without_preflight(self):
        self.assertEqual(
            proposed_preflight_model(permitted=False, group_exists=True,
                                     member_count=5, enforcement=False),
            ("legacy_off", "convert_to_raid", "add_member"))


if __name__ == "__main__":
    unittest.main()
