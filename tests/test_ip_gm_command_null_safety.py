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

    def test_command_table_unchanged(self):
        self.assertIn('{ "set",    HandleSetIndividualProgressionCommand,    SEC_GAMEMASTER,    Console::Yes }', SRC)
        self.assertIn('{ "setbot", HandleSetBotIndividualProgressionCommand, SEC_GAMEMASTER,    Console::Yes }', SRC)


if __name__ == "__main__":
    unittest.main()
