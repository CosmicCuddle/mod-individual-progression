"""Source contracts for the F14 verified RandomBot TBC floor (one rule)."""
import os
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KEY = "IndividualProgression.F14.VerifiedRandomBotTbcFloor"


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def body(src, signature, end="\n}\n"):
    return src.split(signature, 1)[1].split(end, 1)[0]


class TbcFloorDefaults(unittest.TestCase):
    def test_conf_default_off(self):
        conf = read("conf/individualProgression.conf.dist")
        self.assertEqual(re.findall(r"(?m)^" + re.escape(KEY) + r"\s*=\s*(\S+)\s*$", conf), ["0"])

    def test_member_default_and_forced_off_without_playerbots(self):
        self.assertIn("bool f14VerifiedRandomBotTbcFloor = false;", read("src/IndividualProgression.h"))
        core = read("src/IndividualProgression.cpp")
        self.assertIn('"' + KEY + '", false)', core)
        block = core.split('#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")', 1)[1]
        block = block[:block.index("#endif")]
        self.assertIn("sIndividualProgression->f14VerifiedRandomBotTbcFloor = false;", block)
        # The obsolete Core visibility-refresh gate does not apply to this rule.
        vis = core.split("#if !defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION) || COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION < 1\n        // The source-only", 1)[1]
        vis = vis[:vis.index("#endif")]
        self.assertNotIn("f14VerifiedRandomBotTbcFloor", vis)

    def test_unified_supersedes_all_four_legacy_floor_options(self):
        # CS01 6884969131: legacy floor options must not veto the unified rule.
        core = read("src/IndividualProgression.cpp")
        block = core.split("        if (sIndividualProgression->f14VerifiedRandomBotTbcFloor)\n        {\n            struct LegacyFloorOption", 1)[1]
        block = block[:block.index("        // F14 attack/corpse lifecycle registry")]
        for member, key in (("f14PreserveVerifiedRandomBotStageOnLogin", "PreserveVerifiedRandomBotStageOnLogin"),
                            ("f14ForceVerifiedRandomBotTbcFloorOnLogin", "ForceVerifiedRandomBotTbcFloorOnLogin"),
                            ("f14ProtectVerifiedRandomBotStageFloor", "ProtectVerifiedRandomBotStageFloor"),
                            ("f14ProtectVerifiedRandomBotGroupFloor", "ProtectVerifiedRandomBotGroupFloor")):
            self.assertIn("{ &sIndividualProgression->%s, \"%s\" }," % (member, key), block)
        self.assertIn("*option.value = IndividualProgressionF14TbcFloorPolicy::LegacyFloorOptionActive(true, *option.value);", block)
        self.assertNotIn("SocialIsolation", block)
        # After every legacy load and both compile-time gates; before use.
        self.assertLess(core.index('"IndividualProgression.F14.VerifiedRandomBotTbcFloor", false);'), core.index("struct LegacyFloorOption"))
        self.assertLess(core.index("racial RandomBot TBC stage floor requires Core visibility refresh integration"),
                        core.index("struct LegacyFloorOption"))
        policy = strip_comments(read("src/IndividualProgressionF14TbcFloorPolicy.h"))
        self.assertIn("return legacyRequested && !unifiedOn;", policy)


class TbcFloorRuntime(unittest.TestCase):
    def setUp(self):
        self.core = strip_comments(read("src/IndividualProgression.cpp"))
        self.player = strip_comments(read("src/IndividualProgressionPlayer.cpp"))
        self.policy = strip_comments(read("src/IndividualProgressionF14TbcFloorPolicy.h"))

    def test_target_requires_option_then_verified_identity(self):
        target = body(self.core, "bool IndividualProgression::IsF14VerifiedRandomBotTbcFloorTarget(Player* player)\n{")
        self.assertRegex(target, r"if \(!enabled \|\| !f14VerifiedRandomBotTbcFloor \|\| !player \|\| !player->IsInWorld\(\)\)\s*return false;")
        for token in ("isBotAccount(player)", "RACE_DRAENEI", "RACE_BLOODELF", "IsHeadless()",
                      "HasF14VerifiedRandomBotIdentity(player)"):
            self.assertIn(token, target, token)
        self.assertIn("IndividualProgressionF14TbcFloorPolicy::FloorApplies(true, identity, progressionLimit)", target)
        self.assertNotIn("IsRandomBot", target)  # no mutable Playerbots state outside login

    def test_force_update_clamps_before_any_write(self):
        force = body(self.core, "void IndividualProgression::ForceUpdateProgressionState(Player* player, ProgressionState newState)\n{")
        clamp = force.index("IndividualProgressionF14TbcFloorPolicy::ClampWrite(")
        self.assertLess(force.index("sIndividualProgression->IsF14VerifiedRandomBotTbcFloorTarget(player)"), clamp)
        self.assertLess(clamp, force.index("BlocksF14VerifiedStageFloorDowngrade"))
        self.assertLess(clamp, force.index("RemoveRewardedQuest"))
        self.assertIn("newState < IndividualProgressionF14TbcFloorPolicy::FirstTbcStage &&", force)
        block = force[force.index("newState < IndividualProgressionF14TbcFloorPolicy::FirstTbcStage &&"):force.index("BlocksF14VerifiedStageFloorDowngrade")]
        self.assertRegex(block, r"uint8 const clamped = IndividualProgressionF14TbcFloorPolicy::ClampWrite\(static_cast<uint8>\(newState\)\);")
        self.assertRegex(block, r"newState = static_cast<ProgressionState>\(clamped\);")

    def test_login_replaces_level_write_for_floored_bots_only(self):
        login = self.player.split("void OnPlayerLogin(Player* player) override", 1)[1].split("\n    void ", 1)[0]
        # Identity is published when the rule is ON.
        self.assertIn("sIndividualProgression->f14VerifiedRandomBotTbcFloor) &&", login)
        floor = login.split("bool appliedF14TbcFloor = false;", 1)[1].split("#endif", 1)[0]
        self.assertIn("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE", login.split("bool appliedF14TbcFloor = false;", 1)[1][:80])
        self.assertIn("if (sIndividualProgression->IsF14VerifiedRandomBotTbcFloorTarget(player))", floor)
        self.assertIn("IndividualProgressionF14TbcFloorPolicy::TargetStage(", floor)
        self.assertIn("IndividualProgressionF14TbcFloorPolicy::NeedsRepair(savedStage, targetStage)", floor)
        self.assertIn("appliedF14TbcFloor = true;", floor)
        self.assertIn("if (!preserveVerifiedRandomBotStage && !appliedVerifiedRandomBotTbcFloor && !appliedF14TbcFloor)", login)
        # Only the non-normal (bot/excluded) branch; the human branch is untouched.
        self.assertLess(login.index("bool appliedF14TbcFloor = false;"), login.index("else // normal account") if "else // normal account" in login else login.index("else"))

    def test_repair_pass_values_only_and_single_write_route(self):
        repair = body(self.core, "void IndividualProgression::RepairF14VerifiedRandomBotTbcFloors()\n{")
        self.assertRegex(repair, r"if \(!enabled \|\| !f14VerifiedRandomBotTbcFloor\)\s*return;")
        locked = repair.split("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);", 1)[1].split("\n    }\n", 1)[0]
        for token in ("FindPlayer", "ForceUpdateProgressionState", "GetPlayerProgressionFromQuests"):
            self.assertNotIn(token, locked, token)
        self.assertIn("if (isVerified)", locked)
        self.assertIn("if (!player || !IsF14VerifiedRandomBotTbcFloorTarget(player))", repair)
        self.assertIn("IndividualProgressionF14TbcFloorPolicy::NeedsRepair(saved, target)", repair)
        self.assertEqual(repair.count("ForceUpdateProgressionState("), 1)
        for token in ("RemoveRewardedQuest", "RewardQuest", "AddQuest", "Query", "Execute("):
            self.assertNotIn(token, repair, token)

    def test_world_script_interval_and_registration(self):
        script = strip_comments(read("src/IndividualProgressionF14TbcFloorScript.cpp"))
        self.assertIn("{WORLDHOOK_ON_UPDATE}", script)
        self.assertRegex(script, r"if \(!sIndividualProgression->enabled \|\| !sIndividualProgression->f14VerifiedRandomBotTbcFloor\)\s*\{\s*_elapsed = 0;\s*return;")
        self.assertIn("sIndividualProgression->RepairF14VerifiedRandomBotTbcFloors();", script)
        self.assertIn("new F14TbcFloorRepairWorldScript();", script)
        reg = self.core.split("void AddSC_mod_individual_progression()", 1)[1].split("}", 1)[0]
        self.assertIn("AddSC_mod_individual_progression_f14_tbc_floor();", reg)

    def test_policy_never_vanilla_never_lowers(self):
        self.assertIn("return requestedStage < FirstTbcStage ? FirstTbcStage : requestedStage;", self.policy)
        self.assertIn("return savedStage >= required ? savedStage : required;", self.policy)
        self.assertIn("return optionEnabled && identityPositivelyVerified && LimitAllowsFloor(progressionLimit);", self.policy)
        for token in ("getRace", "Player", "#include \""):
            self.assertNotIn(token, self.policy, token)


class PlayerbotsResetQuestsContract(unittest.TestCase):
    """The re-roll path the repair pass exists for (pinned when a synced PB tree is available)."""

    def test_resetquests_removes_every_rewarded_quest(self):
        pb = os.environ.get("F14_PLAYERBOTS_SRC")
        if not pb:
            self.skipTest("set F14_PLAYERBOTS_SRC to a Playerbots src/ tree to pin the re-roll contract")
        factory = (Path(pb) / "Bot/Factory/PlayerbotFactory.cpp").read_text(encoding="utf-8")
        reset = factory.split("void PlayerbotFactory::ResetQuests()", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("bot->RemoveRewardedQuest(entry);", reset)
        self.assertNotRegex(reset, r"6600\d|IndividualProgression")  # IP markers are not spared
        rand = factory.split("void PlayerbotFactory::Randomize(bool incremental)", 1)[1].split("\n}\n", 1)[0]
        self.assertRegex(rand, r"if \(!incremental\)[\s\S]*ResetQuests\(\);")


if __name__ == "__main__":
    unittest.main()
