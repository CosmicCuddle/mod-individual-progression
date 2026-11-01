"""Read-only source wiring regression for temporary GM command DRAFT.

This suite is NOT a substitute for a full pinned Worldserver native link or a
GM/normal-player client test. Deliberately never invokes production commands.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "src" / "cs_individualProgression.cpp").read_text()
POLICY = (ROOT / "src" / "IndividualProgressionGmPolicy.h").read_text()
HEADER = (ROOT / "src" / "IndividualProgression.h").read_text()


def body(start, end):
    a = SRC.index(start)
    b = SRC.index(end, a + len(start))
    return SRC[a:b]


class GmSourceWiring(unittest.TestCase):
    def test_only_get_security_downgraded_to_player(self):
        table = body('static ChatCommandTable ipTable =', 'static ChatCommandTable commandTable')
        self.assertIn('{ "get",    HandleGetIndividualProgressionCommand,    SEC_PLAYER,', table)
        for name in ("set", "pet", "tele", "setbot", "setrep", "pvp", "attune"):
            self.assertRegex(table, r'\{ "' + name + r'",\s+\w+,\s+SEC_GAMEMASTER,')
        self.assertNotIn("SEC_PLAYER", table.replace(
            '{ "get",    HandleGetIndividualProgressionCommand,    SEC_PLAYER,', ""))

    def test_regular_player_cannot_use_selection_or_explicit_other(self):
        get = body('static bool HandleGetIndividualProgressionCommand', 'static bool HandleSetIndividualProgressionCommand')
        self.assertIn('session->GetSecurity() >= SEC_GAMEMASTER', get)
        self.assertIn('Player* const self = session->GetPlayer();', get)
        self.assertIn('player ? player->GetConnectedPlayer() : self', get)
        self.assertIn('requested->GetGUID() == self->GetGUID()', get)
        self.assertIn('CanViewTarget(false, true,', get)
        self.assertIn('You can only view your own progression.', get)
        self.assertLess(get.index('if (!privileged)'), get.index('PlayerIdentifier::FromTargetOrSelf(handler)'))
        self.assertIn('target = self;', get)
        self.assertIn('Earned progression stage:', get)
        self.assertIn('Current access stage:', get)

    def test_gm_audit_is_paid_only_and_never_creates_paid_flag(self):
        forced = body('static bool HandleSetIndividualProgressionCommand', 'static bool HandleSetBotIndividualProgressionCommand')
        self.assertIn('IsPaidExpansionActive()', forced)
        self.assertIn('isNormalAccount(target)', forced)
        self.assertIn('HasExpansionEntitlement(target, Era::Tbc)', forced)
        self.assertIn('HasExpansionEntitlement(target, Era::Wrath)', forced)
        self.assertIn('NewlyGrantedEra(true, true, oldTbc, newTbc)', forced)
        self.assertIn('NewlyGrantedEra(true, true, oldWrath, newWrath)', forced)
        self.assertIn('WorldSession* const issuerSession = handler->GetSession();', forced)
        self.assertIn(': "console";', forced)
        self.assertIn('LOG_INFO("module", "IndividualProgression GM forced-era grant:', forced)
        self.assertNotIn('SetPaidExpansionFlag(', forced)
        self.assertNotIn('UpdatePlayerSetting(', forced)

    def test_historic_raw_gm_tele_contract_preserved(self):
        tele = body('static bool HandleTeleIndividualProgressionCommand', 'static bool HandlePVPIndividualProgressionCommand')
        self.assertIn('GetPlayerProgressionFromQuests(target)', tele)
        self.assertGreaterEqual(tele.count('progressionLevel < PROGRESSION_TBC_TIER_5'), 2)
        self.assertRegex(HEADER, r'PROGRESSION_TBC_TIER_5\s*= 13')
        self.assertNotIn('GetEffectiveProgression(', tele)
        self.assertIn('constexpr bool CanViewTarget(', POLICY)
        self.assertIn('constexpr bool NewlyGrantedEra(', POLICY)


if __name__ == "__main__":
    unittest.main()
