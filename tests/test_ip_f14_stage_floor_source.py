"""F14 verified RandomBot stage-8 floor durability: source contracts, not in-game proof."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KEY = "IndividualProgression.F14.ProtectVerifiedRandomBotStageFloor"
POLICY = "src/IndividualProgressionF14StageFloorPolicy.h"


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def body_of(src, signature, end="\n}\n"):
    rest = src.split(signature, 1)[1]
    return rest[:rest.index(end)]


class DefaultOff(unittest.TestCase):
    def test_conf_member_and_fallback_off(self):
        conf = read("conf/individualProgression.conf.dist")
        self.assertEqual(len(re.findall(r"^" + re.escape(KEY) + r"\s*=\s*0\s*$", conf, re.M)), 1)
        self.assertNotRegex(conf, r"(?m)^" + re.escape(KEY) + r"\s*=\s*[1-9]")
        self.assertIn("bool f14ProtectVerifiedRandomBotStageFloor = false;", read("src/IndividualProgression.h"))
        core = read("src/IndividualProgression.cpp")
        self.assertIn('"' + KEY + '", false)', core)
        # Forced OFF without the Playerbots API header.
        pb = core.split('#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")', 1)[1]
        pb = pb[:pb.index("#endif")]
        self.assertIn("sIndividualProgression->f14ProtectVerifiedRandomBotStageFloor = false;", pb)
        for key in ("F14.ProtectVerifiedRandomBotGroupFloor", "F14.PreserveVerifiedRandomBotStageOnLogin",
                    "F14.ForceVerifiedRandomBotTbcFloorOnLogin", "F14.ExperimentalRndbotPlayerVisibility",
                    "F14.ExperimentalAttackLifecycleRegistry", "PaidExpansion.Enable"):
            self.assertRegex(conf, r"(?m)^IndividualProgression\." + re.escape(key) + r"\s*=\s*0\s*$")


class PurePolicy(unittest.TestCase):
    def test_policy_is_pure_and_conservative(self):
        src = strip_comments(read(POLICY))
        includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', src)
        self.assertEqual(includes, ["cstdint"])
        fn = body_of(src, "constexpr bool BlocksVerifiedFloorDowngrade(", "\n    }\n")
        for token in ("if (!optionEnabled || !identityPositivelyVerified)",
                      "currentStage < FirstTbcStage || currentStage > LastSupportedStage",
                      "if (requestedStage >= FirstTbcStage)",
                      "progressionLimit > 0 && currentStage > progressionLimit"):
            self.assertIn(token, fn)
        self.assertIn("constexpr std::uint8_t FirstTbcStage = 8;", src)
        self.assertIn("constexpr std::uint8_t LastSupportedStage = 18;", src)


class CentralGuard(unittest.TestCase):
    def test_guard_is_first_and_a_pure_noop(self):
        core = strip_comments(read("src/IndividualProgression.cpp"))
        fn = body_of(core, "void IndividualProgression::ForceUpdateProgressionState(Player* player, ProgressionState newState)\n{")
        statements = [l.strip() for l in fn.splitlines() if l.strip()]
        # Null/world check, then the floor guard, before ANY marker removal.
        self.assertEqual(statements[0], "if (!player || !player->IsInWorld())")
        self.assertEqual(statements[1], "return;")
        self.assertTrue(statements[2].startswith(
            "if (sIndividualProgression->BlocksF14VerifiedStageFloorDowngrade(player, static_cast<uint8>(newState)))"))
        guard = fn[fn.index("BlocksF14VerifiedStageFloorDowngrade"):]
        guard = guard[:guard.index("return;") + len("return;")]
        self.assertNotIn("RemoveRewardedQuest", guard)
        self.assertNotIn("RewardQuest", guard)
        self.assertLess(fn.index("BlocksF14VerifiedStageFloorDowngrade"), fn.index("RemoveRewardedQuest"))

    def test_identity_is_positive_epoch_plus_requirements(self):
        core = strip_comments(read("src/IndividualProgression.cpp"))
        fn = body_of(core, "bool IndividualProgression::BlocksF14VerifiedStageFloorDowngrade(Player* player, uint8 requestedStage)\n{")
        self.assertTrue(fn.strip().startswith(
            "if (!enabled || !f14ProtectVerifiedRandomBotStageFloor || !player || !player->IsInWorld())"))
        for token in ("isBotAccount(player)", "RACE_DRAENEI", "RACE_BLOODELF", "IsHeadless()",
                      "HasF14VerifiedRandomBotIdentity(player)",
                      "IndividualProgressionF14StageFloorPolicy::BlocksVerifiedFloorDowngrade(",
                      "GetPlayerProgressionFromQuests(player)", "progressionLimit"):
            self.assertIn(token, fn)
        # Race/regex/headless/level are requirements AND-ed with the epoch,
        # never proof on their own; no Playerbots or stage writes here.
        ident = fn[fn.index("bool const identity ="):fn.index(";", fn.index("bool const identity ="))]
        self.assertEqual(ident.count("||"), 1)        # only the race pair is OR-ed
        self.assertTrue(ident.rstrip().endswith("&&\n        HasF14VerifiedRandomBotIdentity(player)")
                        or ident.rstrip().endswith("HasF14VerifiedRandomBotIdentity(player)"))
        self.assertIn("&&\n        HasF14VerifiedRandomBotIdentity(player)", ident)
        for forbidden in ("sRandomPlayerbotMgr", "GetLevel()", "ForceUpdateProgressionState",
                          "UpdateProgressionState", "RewardQuest", "isNormalAccount"):
            self.assertNotIn(forbidden, fn)

    def test_raising_and_paid_paths_untouched(self):
        core = strip_comments(read("src/IndividualProgression.cpp"))
        up = body_of(core, "void IndividualProgression::UpdateProgressionState(Player* player, ProgressionState newState) const\n{")
        self.assertNotIn("StageFloor", up)
        self.assertNotIn("StageFloor", strip_comments(read("src/IndividualProgressionExpansion.cpp")))


class LoginAndGm(unittest.TestCase):
    def test_login_gate_one_extra_reason_creation_floor_untouched(self):
        player = read("src/IndividualProgressionPlayer.cpp")
        self.assertEqual(player.count("f14ProtectVerifiedRandomBotStageFloor"), 1)
        login = player.split("void OnPlayerLogin(Player* player) override", 1)[1]
        gate = login[login.index("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE"):]
        gate = gate[:gate.index("#endif")]
        self.assertIn("sIndividualProgression->f14ProtectVerifiedRandomBotStageFloor) &&", gate)
        self.assertIn("sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())", gate)
        # The refresh-macro-gated creation floor is unchanged.
        self.assertIn("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE && defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION) && COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION >= 1", player)
        for src in ("src/IndividualProgression.cpp", "src/IndividualProgression.h", "src/IndividualProgressionPlayer.cpp",
                    "src/cs_individualProgression.cpp", POLICY):
            self.assertNotRegex(read(src), r"#\s*define\s+COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION")

    def test_ip_set_refuses_before_achievement_cleanup(self):
        cs = strip_comments(read("src/cs_individualProgression.cpp"))
        fn = body_of(cs, "static bool HandleSetIndividualProgressionCommand(", "\n    }\n")
        guard = fn.index("BlocksF14VerifiedStageFloorDowngrade(target, static_cast<uint8>(progressionLevel))")
        self.assertLess(guard, fn.index("CheckProgressionAchievements(target"))
        self.assertLess(guard, fn.index("ForceUpdateProgressionState(target"))
        refusal = fn[guard:fn.index("CheckProgressionAchievements(target")]
        self.assertIn("return false;", refusal)

    def test_ip_setbot_skips_and_reports(self):
        cs = strip_comments(read("src/cs_individualProgression.cpp"))
        fn = body_of(cs, "static bool HandleSetBotIndividualProgressionCommand(ChatHandler* handler)", "\n    }\n")
        guard = fn.index("BlocksF14VerifiedStageFloorDowngrade(member, static_cast<uint8>(currentState))")
        self.assertLess(guard, fn.index("ForceUpdateProgressionState(member"))
        self.assertIn("++floorSkipped;", fn)
        self.assertIn("if (floorSkipped)", fn)


if __name__ == "__main__":
    unittest.main()
