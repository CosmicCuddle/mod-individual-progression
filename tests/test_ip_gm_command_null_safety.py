#!/usr/bin/env python3
"""Review-only source regression for independently scoped IP GM crash fix.

These checks do not start Worldserver or assert the command parser's runtime
behaviour. Native compile and actual in-game/console tests remain separate.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "src/cs_individualProgression.cpp").read_text()

GET = SRC.split("static bool HandleGetIndividualProgressionCommand(", 1)[1].split(
    "static bool HandleSetIndividualProgressionCommand(", 1)[0]
SET = SRC.split("static bool HandleSetIndividualProgressionCommand(", 1)[1].split(
    "static bool HandleSetBotIndividualProgressionCommand(", 1)[0]
SETBOT = SRC.split("static bool HandleSetBotIndividualProgressionCommand(", 1)[1].split(
    "static bool HandleSetRepIndividualProgressionCommand(", 1)[0]
SETREP = SRC.split("static bool HandleSetRepIndividualProgressionCommand(", 1)[1].split(
    "static bool HandlePetIndividualProgressionCommand(", 1)[0]
TELE = SRC.split("static bool HandleTeleIndividualProgressionCommand(", 1)[1].split(
    "static bool HandleAttuneIndividualProgressionCommand(", 1)[0]
ATTUNE = SRC.split("static bool HandleAttuneIndividualProgressionCommand(", 1)[1].split(
    "static bool HandlePVPIndividualProgressionCommand(", 1)[0]


class Scope(unittest.TestCase):
    def test_get_command_preserved(self):
        self.assertIn("PlayerIdentifier::FromTargetOrSelf(handler)", GET)
        self.assertIn("Player |cff00ffff{}|r is not online.", GET)

    def test_set_optional_name_not_overwritten(self):
        self.assertIn("if (!player)\n            player = PlayerIdentifier::FromTargetOrSelf(handler);", SET)
        self.assertEqual(SET.count("player = PlayerIdentifier::FromTargetOrSelf(handler);"), 1)

    def test_set_absent_target_refused_before_dereference(self):
        self.assertIn('handler->SendSysMessage("Player not found. Specify an online player or target.");', SET)
        self.assertLess(SET.index("if (!player)\n        {"), SET.index("player->GetConnectedPlayer()"))

    def test_set_offline_or_logging_target_refused_before_writes(self):
        self.assertIn("if (!target || !target->IsInWorld())", SET)
        self.assertLess(SET.index("if (!target || !target->IsInWorld())"),
                        SET.index("CheckProgressionAchievements(target"))
        self.assertLess(SET.index("if (!target || !target->IsInWorld())"),
                        SET.index("ForceUpdateProgressionState(target"))
        self.assertLess(SET.index("if (!target || !target->IsInWorld())"),
                        SET.index("target->GetAreaId()"))

    def test_set_preserves_original_valid_state_write_and_achievements(self):
        self.assertIn("if (progressionLevel < currentState)", SET)
        self.assertIn("CheckProgressionAchievements(target, currentState, progressionLevel);", SET)
        self.assertIn("ForceUpdateProgressionState(target, static_cast<ProgressionState>(progressionLevel))", SET)
        self.assertIn("checkIPPhasing(target, currentArea)", SET)

    def test_setbot_console_or_missing_session_refused(self):
        self.assertIn("WorldSession* session = handler->GetSession();", SETBOT)
        self.assertIn("if (!session)", SETBOT)
        self.assertLess(SETBOT.index("if (!session)"), SETBOT.index("session->GetPlayer()"))
        self.assertNotIn("handler->GetSession()->GetPlayer()", SETBOT)

    def test_setbot_missing_or_unloaded_player_refused_before_group(self):
        self.assertIn("if (!player || !player->IsInWorld())", SETBOT)
        self.assertLess(SETBOT.index("if (!player || !player->IsInWorld())"),
                        SETBOT.index("player->GetGroup()"))

    def test_setbot_existing_valid_group_sync_untouched(self):
        self.assertIn("Group* group = player->GetGroup();", SETBOT)
        self.assertIn("if (!group)", SETBOT)
        self.assertIn("if (!member || sIndividualProgression->isNormalAccount(member))", SETBOT)
        self.assertIn("ForceUpdateProgressionState(member, static_cast<ProgressionState>(currentState))", SETBOT)

    def test_tele_respects_explicit_player_identifier(self):
        self.assertIn("if (!player)\n            player = PlayerIdentifier::FromTargetOrSelf(handler);", TELE)
        self.assertEqual(TELE.count("player = PlayerIdentifier::FromTargetOrSelf(handler);"), 1)

    def test_tele_missing_or_offline_target_refused_before_teleport(self):
        self.assertIn("if (!player)\n        {", TELE)
        self.assertIn("if (!target || !target->IsInWorld())", TELE)
        self.assertLess(TELE.index("if (!target || !target->IsInWorld())"),
                        TELE.index("GetPlayerProgressionFromQuests(target)"))
        self.assertLess(TELE.index("if (!target || !target->IsInWorld())"),
                        TELE.index("target->TeleportTo("))
        self.assertIn("Group* group = target->GetGroup();", TELE)

    def test_setrep_console_is_rejected_before_session_dereference(self):
        self.assertIn("WorldSession* session = handler->GetSession();", SETREP)
        self.assertIn("if (!session)", SETREP)
        self.assertLess(SETREP.index("if (!session)"), SETREP.index("session->GetPlayer()"))
        self.assertLess(SETREP.index("if (!session)"), SETREP.index("session->GetAccountId()"))
        self.assertNotIn("handler->GetSession()->GetPlayer()", SETREP)
        self.assertNotIn("handler->GetSession()->GetAccountId()", SETREP)

    def test_setrep_inworld_player_guard_before_group_or_reputation(self):
        self.assertIn("if (!player || !player->IsInWorld() || !accountId)", SETREP)
        self.assertLess(SETREP.index("if (!player || !player->IsInWorld() || !accountId)"),
                        SETREP.index("player->GetGroup()"))
        self.assertIn('The .ip setrep command is currently disabled.', SETREP)

    def test_attune_console_is_rejected_before_player_dereference(self):
        self.assertIn("WorldSession* session = handler->GetSession();", ATTUNE)
        self.assertIn("if (!session)", ATTUNE)
        self.assertLess(ATTUNE.index("if (!session)"), ATTUNE.index("session->GetPlayer()"))
        self.assertNotIn("handler->GetSession()->GetPlayer()", ATTUNE)

    def test_attune_missing_or_nonworld_player_refused_before_change(self):
        self.assertIn("if (!player || !player->IsInWorld())", ATTUNE)
        self.assertLess(ATTUNE.index("if (!player || !player->IsInWorld())"),
                        ATTUNE.index("UpdateGroupAttunement(player, location)"))
        self.assertIn('location != "onyxia40" && location != "onyxia"', ATTUNE)

    def test_command_table_unchanged(self):
        self.assertIn('{ "set",    HandleSetIndividualProgressionCommand,    SEC_GAMEMASTER,    Console::Yes }', SRC)
        self.assertIn('{ "setbot", HandleSetBotIndividualProgressionCommand, SEC_GAMEMASTER,    Console::Yes }', SRC)


if __name__ == "__main__":
    unittest.main()
