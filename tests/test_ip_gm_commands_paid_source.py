"""Source contracts for the GM-command DRAFT: paid-mode .ip set audit and
.ip get self-only for ordinary players. Pure rules are tested by
tests/ip_gm_policy.cpp. Source checks, not gameplay tests.

Run: python3 -B -m unittest discover -s tests -p 'test_ip_gm_commands_paid_source.py'
"""
import re
import subprocess
import unittest
from pathlib import Path

import ip_gm_cs_baseline

ROOT = Path(__file__).resolve().parents[1]
PARENT_23 = "8dc16ffb3ca7f70a9c7ddc7cfbe6c5bd40088e37"
CS = (ROOT / "src/cs_individualProgression.cpp").read_text()
TOOLS = (ROOT / "src/IndividualProgressionGmTools.cpp").read_text()
POLICY = (ROOT / "src/IndividualProgressionGmPolicy.h").read_text()
HEADER = (ROOT / "src/IndividualProgression.h").read_text()


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, check=True).stdout


def body(src: str, signature: str) -> str:
    i = src.index(signature)
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        depth += src[k] == "{"
        depth -= src[k] == "}"
        if depth == 0:
            return src[i:k + 1]
    raise AssertionError(signature)


def code(text: str) -> str:
    return re.sub(r"//[^\n]*", "", text)


class Provenance(unittest.TestCase):
    def test_parent_is_pr23_head(self):
        subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", PARENT_23, "HEAD"], check=True)

    def test_cs_is_reviewed_blob_plus_documented_lines(self):
        self.assertTrue(ip_gm_cs_baseline.matches_reviewed_blob(CS))
        for new_text, _ in ip_gm_cs_baseline.DOCUMENTED_EDITS:
            self.assertEqual(CS.count(new_text), 1, new_text)

    def test_only_cs_and_new_gm_files_among_sources(self):
        changed = sorted(git("diff", "--name-only", PARENT_23, "--", "src", "conf", "data", "optional", "sql").split())
        self.assertEqual(changed, ["src/IndividualProgressionGmPolicy.h", "src/IndividualProgressionGmTools.cpp",
                                   "src/IndividualProgressionGmTools.h", "src/cs_individualProgression.cpp"])


class CommandTable(unittest.TestCase):
    def test_only_get_is_player_level(self):
        table = body(CS, "ChatCommandTable GetCommands() const override")
        self.assertEqual(table.count("SEC_PLAYER"), 1)
        self.assertIn('{ "get",    HandleGetIndividualProgressionCommand,    SEC_PLAYER,        Console::Yes },', table)
        for name in ("set", "pet", "tele", "setbot", "setrep", "pvp", "attune"):
            self.assertRegex(table, r'\{ "%s",\s+Handle\w+,\s+SEC_GAMEMASTER,\s+Console::Yes \}' % name)


class GetCommand(unittest.TestCase):
    GET = code(body(CS, "static bool HandleGetIndividualProgressionCommand("))

    def test_target_resolved_before_existing_lookup(self):
        resolve = self.GET.index("if (!IndividualProgressionGmTools::ResolveGetTarget(handler, player))\n            return false;")
        self.assertLess(resolve, self.GET.index("player = PlayerIdentifier::FromTargetOrSelf(handler);"))
        self.assertLess(self.GET.index('handler->PSendSysMessage("Progression Level for |cff00ffff{}|r = |cff00ffff{}|r", playername, progressionLevel);'),
                        self.GET.index("IndividualProgressionGmTools::SendAccessStage(handler, target);"))

    def test_ordinary_player_limited_to_self(self):
        resolve = code(body(TOOLS, "bool ResolveGetTarget("))
        self.assertIn("bool const console = !session;", resolve)
        self.assertIn("bool const gameMaster = session && session->GetSecurity() >= SEC_GAMEMASTER;", resolve)
        self.assertIn("player = PlayerIdentifier::FromSelf(handler);", resolve)
        self.assertIn('handler->SendSysMessage("You can only view your own progression.");', resolve)
        self.assertIn("DecideGetTarget(console, gameMaster, player.has_value(), requestedIsSelf)", resolve)
        self.assertIn("if (console || gameMaster)\n            return GetTarget::RequestedOrSelection;", POLICY)

    def test_access_line_only_when_paid_active_and_normal_account(self):
        send = code(body(TOOLS, "void SendAccessStage("))
        self.assertIn("if (!target || !sIndividualProgression->IsPaidExpansionActive() || !sIndividualProgression->isNormalAccount(target))\n            return;", send)
        self.assertIn("GetEffectiveProgression(target)", send)


class SetAudit(unittest.TestCase):
    SET = code(body(CS, "static bool HandleSetIndividualProgressionCommand("))

    def test_audit_wraps_existing_write(self):
        begin = self.SET.index("IndividualProgressionGmTools::BeginSetAudit(target);")
        force = self.SET.index("sIndividualProgression->ForceUpdateProgressionState(target, static_cast<ProgressionState>(progressionLevel));")
        finish = self.SET.index("IndividualProgressionGmTools::FinishSetAudit(handler, target, paidAudit, currentState, progressionLevel);")
        self.assertTrue(begin < force < finish)

    def test_audit_never_writes_purchase_flags_or_stages(self):
        for forbidden in ("SetPaidExpansionFlag", "UpdatePlayerSetting", "PurchaseExpansion", "ForceUpdateProgressionState", "UpdateProgressionState"):
            self.assertNotIn(forbidden, code(TOOLS), forbidden)

    def test_audit_skips_off_and_non_normal_accounts(self):
        begin = code(body(TOOLS, "SetAudit BeginSetAudit("))
        self.assertIn("audit.active = sIndividualProgression->IsPaidExpansionActive();", begin)
        self.assertIn("audit.gated = sIndividualProgression->isNormalAccount(target);", begin)
        finish = code(body(TOOLS, "void FinishSetAudit("))
        self.assertIn("if (!audit.active || !audit.gated || !target)\n            return;", finish)
        self.assertIn("NewlyGranted(audit.tbcBefore, tbcAfter)", finish)
        self.assertIn("NewlyGranted(audit.wrathBefore, wrathAfter)", finish)
        self.assertIn('std::string("console")', finish)
        self.assertIn("LOG_INFO(\"module\", \"IndividualProgression GM audit:", finish)
        self.assertIn("return !ownedBefore && ownedAfter;", POLICY)


class TeleUnchanged(unittest.TestCase):
    def test_tele_keeps_raw_stage_check(self):
        # Documented, deliberately unchanged: PROGRESSION_TBC_TIER_5 is 13, so an
        # unpaid raw 13 is refused this GM teleport (fail closed).
        tele = code(body(CS, "static bool HandleTeleIndividualProgressionCommand("))
        self.assertIn("uint32 progressionLevel = sIndividualProgression->GetPlayerProgressionFromQuests(target);", tele)
        self.assertEqual(tele.count("(progressionLevel < PROGRESSION_TBC_TIER_5)"), 2)
        self.assertNotIn("GetEffectiveProgression", tele)
        self.assertRegex(HEADER, r"\n\s*PROGRESSION_TBC_TIER_5\s*=\s*13,")


if __name__ == "__main__":
    unittest.main()
