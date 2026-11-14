"""Successor source gate for cs_individualProgression.cpp after the F14 stage-floor change.

The prior byte-exact GM pins (frozen earlier, not edited here):
  - test_ip_grim_20261104_source_preservation.test_custom_gm_command_bodies_exactly_preserved
  - test_ip_combined_c3_f14_partial_login.test_gm_source_is_exactly_the_previously_reviewed_fix
both PASS at the reviewed parent #78 4bb8a8e. This PR intentionally changes the
GM commands, so those pins report it. This successor gate proves the change is
EXACTLY the reviewed allowlisted insertions and nothing else: removing them must
reproduce the parent file byte-for-byte (so GM permissions, console/session
safety, paid-mode audit and every other command are unchanged).
Source proof only, not in-game behaviour.
"""
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GM = "src/cs_individualProgression.cpp"
REVIEWED_PARENT = "4bb8a8e32b20947d298ed0fb7974453b07829cc1"

# Exact, reviewed insertions (each must occur exactly once).
SET_REFUSAL = (
    "        // F14 stage-8 floor durability (default OFF): refuse BEFORE any\n"
    "        // achievement cleanup or marker reset, so nothing is half-applied.\n"
    "        if (progressionLevel <= PROGRESSION_WOTLK_TIER_5 &&\n"
    "            sIndividualProgression->BlocksF14VerifiedStageFloorDowngrade(target, static_cast<uint8>(progressionLevel)))\n"
    "        {\n"
    "            handler->PSendSysMessage(\"Refused: |cff00ffff{}|r is a verified RandomBot with earned TBC stage {}; \"\n"
    "                \"IndividualProgression.F14.ProtectVerifiedRandomBotStageFloor blocks lowering it below 8.\",\n"
    "                playername, currentState);\n"
    "            return false;\n"
    "        }\n"
    "\n"
)
SETBOT_COUNTER = "        uint32 floorSkipped = 0;\n"
SETBOT_SKIP = (
    "            // F14 stage-8 floor durability (default OFF): leave a verified\n"
    "            // RandomBot's earned TBC stage untouched instead of lowering it.\n"
    "            if (sIndividualProgression->BlocksF14VerifiedStageFloorDowngrade(member, static_cast<uint8>(currentState)))\n"
    "            {\n"
    "                ++floorSkipped;\n"
    "                continue;\n"
    "            }\n"
    "\n"
)
SETBOT_REPORT = (
    "        if (floorSkipped)\n"
    "            handler->PSendSysMessage(\"Skipped {} verified RandomBot(s) with an earned TBC stage (F14 stage floor ON).\", floorSkipped);\n"
)
ALLOWLIST = (SET_REFUSAL, SETBOT_COUNTER, SETBOT_SKIP, SETBOT_REPORT)


def parent_source():
    return subprocess.run(["git", "-C", str(ROOT), "show", REVIEWED_PARENT + ":" + GM],
                          check=True, capture_output=True, text=True).stdout


class GmSuccessorGate(unittest.TestCase):
    def setUp(self):
        self.current = (ROOT / GM).read_text(encoding="utf-8")

    def test_each_allowlisted_insertion_occurs_exactly_once(self):
        for block in ALLOWLIST:
            self.assertEqual(self.current.count(block), 1, block[:60])

    def test_removing_allowlist_reproduces_reviewed_parent_exactly(self):
        stripped = self.current
        for block in ALLOWLIST:
            stripped = stripped.replace(block, "", 1)
        self.assertEqual(stripped, parent_source())

    def test_insertions_sit_in_the_reviewed_functions_and_order(self):
        cur = self.current
        set_fn = cur.index("static bool HandleSetIndividualProgressionCommand(")
        setbot_fn = cur.index("static bool HandleSetBotIndividualProgressionCommand(")
        # .ip set: refusal precedes achievement cleanup and the stage write.
        r = cur.index(SET_REFUSAL)
        self.assertTrue(set_fn < r < setbot_fn)
        self.assertLess(r, cur.index("CheckProgressionAchievements(target, currentState, progressionLevel);"))
        self.assertLess(r, cur.index("ForceUpdateProgressionState(target, static_cast<ProgressionState>(progressionLevel));"))
        # .ip setbot: counter before the loop, skip before the write, report after.
        c = cur.index(SETBOT_COUNTER)
        k = cur.index(SETBOT_SKIP)
        w = cur.index("ForceUpdateProgressionState(member, static_cast<ProgressionState>(currentState));")
        rep = cur.index(SETBOT_REPORT)
        self.assertTrue(setbot_fn < c < k < w < rep)


if __name__ == "__main__":
    unittest.main()
