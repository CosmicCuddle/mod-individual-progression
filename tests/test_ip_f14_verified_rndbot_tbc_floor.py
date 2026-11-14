"""Source contract for proposed default-OFF F14 racial RandomBot TBC login floor."""
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

class F14Floor(unittest.TestCase):
    def test_off_without_core_visibility_bridge(self):
        conf=read("conf/individualProgression.conf.dist")
        core=read("src/IndividualProgression.cpp")
        header=read("src/IndividualProgression.h")
        key="IndividualProgression.F14.ForceVerifiedRandomBotTbcFloorOnLogin"
        self.assertEqual(len(re.findall(r"^"+re.escape(key)+r"\s*=\s*0\s*$",conf,re.M)),1)
        self.assertIn("bool f14ForceVerifiedRandomBotTbcFloorOnLogin = false;",header)
        self.assertIn('"'+key+'", false)',core)
        self.assertIn("#if !defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION)",core)
        self.assertIn("requires Core visibility refresh integration; forcing OFF",core)
        self.assertIn("requires Playerbots API header; forcing OFF",core)
        self.assertIn("f14ForceVerifiedRandomBotTbcFloorOnLogin = false;",core)

    def test_verified_identity_precedes_any_stage_write(self):
        full=read("src/IndividualProgressionPlayer.cpp")
        login=full.split("void OnPlayerLogin(Player* player) override",1)[1].split(
            "// Release only the F14 group identity cache",1)[0]
        unit=login.split("// F14: independently default-OFF TBC floor",1)[1].split(
            "// F14 partial safeguard, OFF by default",1)[0]
        required=("f14ForceVerifiedRandomBotTbcFloorOnLogin","isBotAccount(player)",
                  "RACE_DRAENEI","RACE_BLOODELF","IsHeadless()",
                  "sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())",
                  "CanSetVerifiedTbcRaceStage(","DesiredVerifiedTbcRaceStage(",
                  "ForceUpdateProgressionState(", "appliedVerifiedRandomBotTbcFloor = true")
        for token in required:
            self.assertIn(token,unit)
        self.assertIn("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE && defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION)",unit)
        self.assertLess(unit.index("IsRandomBot("),unit.index("ForceUpdateProgressionState("))
        self.assertLess(unit.index("CanSetVerifiedTbcRaceStage("),unit.index("ForceUpdateProgressionState("))
        self.assertIn("if (savedStage < newStage)",unit)
        self.assertIn("if (!preserveVerifiedRandomBotStage && !appliedVerifiedRandomBotTbcFloor)",login)

    def test_existing_owner_defaults_and_pure_policy(self):
        conf=read("conf/individualProgression.conf.dist")
        for key in ("IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin = 0",
                    "IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor = 0",
                    "IndividualProgression.PaidExpansion.Enable = 0",
                    "IndividualProgression.ProfessionTierCap = 0"):
            self.assertIn(key,conf)
        policy=read("src/IndividualProgressionF14LoginPolicy.h")
        for marker in ("DesiredVerifiedTbcRaceStage(","CanSetVerifiedTbcRaceStage(",
                       "savedStage > 18","LevelStage(level)"):
            self.assertIn(marker,policy)
        for forbidden in ("RewardQuest(","ForceUpdateProgressionState(","TeleportTo("):
            self.assertNotIn(forbidden,policy)

if __name__=="__main__":
    unittest.main()
