"""CS01 GM commands: exact-source guardrails. No Worldserver or DB."""
import re
import unittest
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
SRC = (BASE / "src/cs_individualProgression.cpp").read_text()
POLICY = (BASE / "src/IndividualProgressionGmCommandAccessPolicy.h").read_text()
IP_H = (BASE / "src/IndividualProgression.h").read_text()

def section(begin, end):
    return SRC[SRC.index(begin):SRC.index(end, SRC.index(begin))]

class CommandSourcePolicy(unittest.TestCase):
    def test_get_is_the_only_player_level_command(self):
        commands = section("static ChatCommandTable ipTable", "static ChatCommandTable commandTable")
        for name in ("set", "pet", "tele", "setbot", "setrep", "pvp", "attune"):
            self.assertRegex(commands, r'\{ "' + name + r'",.*SEC_GAMEMASTER')
        self.assertRegex(commands, r'\{ "get",.*SEC_PLAYER')
        self.assertEqual(commands.count("SEC_PLAYER"), 1)

    def test_self_only_before_lookup(self):
        get = section("static bool HandleGetIndividualProgressionCommand(", "static bool HandleSetIndividualProgressionCommand(")
        self.assertIn("handler->GetSession()", get)
        self.assertIn("session->GetSecurity() >= SEC_GAMEMASTER", get)
        self.assertIn("player->GetConnectedPlayer() == issuer", get)
        self.assertIn("DecideGetTarget(", get)
        self.assertIn("if (access == Lookup::Refuse)", get)
        self.assertIn("You can only view your own progression.", get)
        self.assertIn("target = issuer;", get)
        self.assertIn("PlayerIdentifier::FromTargetOrSelf(handler)", get)
        self.assertLess(get.index("if (access == Lookup::Refuse)"),
                        get.index("PlayerIdentifier::FromTargetOrSelf(handler)"))
        self.assertIn("if (!target || !target->IsInWorld())", get)

    def test_raw_off_value_and_paid_access_display(self):
        get = section("static bool HandleGetIndividualProgressionCommand(", "static bool HandleSetIndividualProgressionCommand(")
        self.assertIn('Progression Level for |cff00ffff{}|r = |cff00ffff{}|r', get)
        self.assertIn("IsPaidExpansionActive()", get)
        self.assertIn("isNormalAccount(target)", get)
        self.assertIn("GetEffectiveProgression(target)", get)
        self.assertIn("Earned progression stage:", get)
        self.assertIn("Current access stage:", get)
        self.assertNotIn("SetPaidExpansionFlag(", get)

    def test_set_preserves_force_and_audits_new_entitlements(self):
        method = section("static bool HandleSetIndividualProgressionCommand(", "static bool HandleSetBotIndividualProgressionCommand(")
        self.assertEqual(method.count("ForceUpdateProgressionState("), 1)
        self.assertIn("CheckProgressionAchievements(", method)
        self.assertIn("checkIPPhasing(target, currentArea)", method)
        self.assertIn("bool const paidAudit = sIndividualProgression->IsPaidExpansionActive()", method)
        self.assertIn("sIndividualProgression->isNormalAccount(target)", method)
        self.assertIn("HasExpansionEntitlement(", method)
        self.assertIn("NeedsPaidGrantAudit(", method)
        self.assertIn('LOG_INFO("module",', method)
        self.assertIn("issuerSession ? issuerSession->GetAccountId() : 0u", method)
        self.assertIn('"console"', method)
        self.assertIn("target->GetGUID().GetCounter()", method)
        self.assertIn("No paid entitlement flag was written.", method)
        self.assertNotIn("SetPaidExpansionFlag(", method)

    def test_pure_logic_restrictions(self):
        self.assertIn("explicitName && !nameIsSelf ? Lookup::Refuse : Lookup::UseSelf", POLICY)
        self.assertIn("paidFeatureOn && normalAccount", POLICY)
        self.assertIn("(!tbcBefore && tbcAfter)", POLICY)
        self.assertIn("(!wrathBefore && wrathAfter)", POLICY)

    def test_tele_raw_stage_original_and_actual_threshold(self):
        method = section("static bool HandleTeleIndividualProgressionCommand(", "static bool HandlePVPIndividualProgressionCommand(")
        self.assertIn("GetPlayerProgressionFromQuests(", method)
        self.assertIn("progressionLevel < PROGRESSION_TBC_TIER_5", method)
        self.assertRegex(IP_H, r'PROGRESSION_TBC_TIER_5\s*=\s*13')
        self.assertNotIn("GetEffectiveProgression(", method)

if __name__ == "__main__":
    unittest.main()
