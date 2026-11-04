"""F14 attack/corpse lifecycle consumer: source contracts, not in-game proof."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


KEY = "IndividualProgression.F14.ExperimentalAttackLifecycleRegistry"
SCRIPT = "src/IndividualProgressionF14LifecycleScript.cpp"
REGISTRY = "src/IndividualProgressionF14LifecycleRegistry.h"


class DefaultOff(unittest.TestCase):
    def test_conf_header_and_fallback_are_off(self):
        conf = read("conf/individualProgression.conf.dist")
        self.assertEqual(len(re.findall(r"^" + re.escape(KEY) + r"\s*=\s*0\s*$", conf, re.M)), 1)
        self.assertNotRegex(conf, r"(?m)^" + re.escape(KEY) + r"\s*=\s*[1-9]")
        header = read("src/IndividualProgression.h")
        self.assertIn("bool f14ExperimentalAttackLifecycleRegistry = false;", header)
        core = read("src/IndividualProgression.cpp")
        self.assertIn('"' + KEY + '", false)', core)

    def test_forced_off_without_hook_playerbots_or_identity_source(self):
        core = read("src/IndividualProgression.cpp")
        start = core.index('"' + KEY + '", false)')
        part = core[start:core.index("sIndividualProgression->fishingFix", start)]
        self.assertIn("!defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION) || COSMIC_F14_ATTACK_EVENT_HOOK_VERSION < 1", part)
        self.assertIn('#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")', part)
        self.assertIn("!sIndividualProgression->f14ProtectVerifiedRandomBotGroupFloor", part)
        self.assertIn("!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility", part)
        self.assertEqual(part.count("f14ExperimentalAttackLifecycleRegistry = false;"), 3)
        self.assertIn("sIndividualProgression->ResetF14LifecycleRegistry();", part)
        # The forcing block comes AFTER both identity flags are final.
        self.assertGreater(start, core.index('"IndividualProgression.F14.ExperimentalRndbotPlayerVisibility", false)'))
        self.assertGreater(start, core.index('"IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor", false)'))

    def test_refresh_bridge_never_defined_here(self):
        for path in sorted((ROOT / "src").rglob("*")):
            if path.suffix in (".h", ".cpp"):
                text = path.read_text(encoding="utf-8", errors="replace")
                self.assertNotRegex(text, r"#\s*define\s+COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", str(path))
                self.assertNotRegex(text, r"#\s*define\s+COSMIC_F14_ATTACK_EVENT_HOOK_VERSION", str(path))


class ConsumerIsBookkeepingOnly(unittest.TestCase):
    def test_hook_class_only_with_core_prototype(self):
        src = read(SCRIPT)
        guard = "#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION) && COSMIC_F14_ATTACK_EVENT_HOOK_VERSION >= 1"
        self.assertEqual(src.count(guard), 2)
        cls = src.index("class IndividualProgressionF14LifecycleScript : public UnitScript")
        self.assertLess(src.index(guard), cls)
        self.assertLess(cls, src.index("#endif", cls))
        for hook in ("UNITHOOK_ON_F14_ATTACK_TARGET_TRANSITION",
                     "UNITHOOK_ON_F14_CREATURE_CORPSE_READY",
                     "UNITHOOK_ON_F14_CREATURE_RESPAWNED"):
            self.assertEqual(src.count(hook), 1)

    def test_no_side_effects_playerbots_or_visibility(self):
        code = strip_comments(read(SCRIPT)) + strip_comments(read(REGISTRY))
        for forbidden in ("sRandomPlayerbotMgr", "RandomPlayerbotMgr", "ObjectAccessor",
                          "UpdateObjectVisibility", "UpdateVisibility", "DestroyForPlayer",
                          "SetVisible", "SetPhaseMask", "WorldPacket", "SendDirectMessage",
                          "ForceUpdateProgressionState", "UpdateProgressionState",
                          "SetPlayerSetting", "CharacterDatabase", "WorldDatabase",
                          "LoginDatabase", "RewardQuest", "ModifyMoney", "SaveToDB",
                          "IsConcealmentCandidate(", "Kill(", "DespawnOrUnsummon",
                          "CombatStop(", "AttackStop(", "Attack("):
            if forbidden == "IsConcealmentCandidate(":
                # Declared/defined in the pure header, never CALLED by the consumer.
                self.assertNotIn(forbidden, strip_comments(read(SCRIPT)))
                continue
            self.assertNotIn(forbidden, code, forbidden)

    def test_attack_handler_checks_enable_first(self):
        src = strip_comments(read(SCRIPT))
        body = src.split("void OnF14AttackTargetTransition(", 1)[1]
        body = body[:body.index("void OnF14CreatureCorpseReady(")]
        first = body.index("{") + 1
        self.assertTrue(body[first:].lstrip().startswith("if (!LifecycleEnabled() || !attacker"))
        self.assertIn("return sIndividualProgression->enabled && sIndividualProgression->f14ExperimentalAttackLifecycleRegistry;", src)

    def test_no_retained_unit_pointers(self):
        src = strip_comments(read(SCRIPT))
        # No namespace-scope or static object of a game-object pointer type.
        self.assertNotRegex(src, r"\bstatic\s+(?:\w+::)*(Unit|Creature|Player|Map|WorldObject)\s*\*")
        self.assertNotRegex(src, r"std::(vector|set|unordered_set|unordered_map|map)<\s*(Unit|Creature|Player|Map|WorldObject)\s*\*")
        # The per-creature state is a value token in the creature's own CustomData.
        self.assertIn("struct F14CreatureIncarnation : public DataMap::Base", src)
        self.assertIn("std::uint64_t token = 0;", src)
        self.assertIn("std::vector<std::uint64_t> others;", src)
        # Pure header never names game-object types at all.
        reg = strip_comments(read(REGISTRY))
        self.assertNotRegex(reg, r"\b(Unit|Creature|Player|Map|WorldObject|ObjectGuid)\b")
        includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', reg)
        self.assertEqual(sorted(includes), sorted([
            "IndividualProgressionF14MultiAttackerPolicy.h", "cstddef", "cstdint",
            "functional", "limits", "unordered_map"]))

    def test_actor_identity_is_cache_only_and_players_only(self):
        src = strip_comments(read(SCRIPT))
        fn = src.split("std::uint64_t ActorTokenOf(Unit const* unit)", 1)[1].split("bool LifecycleEnabled()", 1)[0]
        self.assertIn("if (!unit || !unit->IsPlayer())", fn)
        self.assertIn("GetF14VerifiedRandomBotLoginEpoch(unit->GetGUID())", fn)
        # Pets/minions/charmed units of players are never world-creature entries.
        world = src.split("Creature* AsWorldCreature(Unit* unit)", 1)[1].split("Registry::CreatureKey KeyOf(", 1)[0]
        for token in ("IsPet()", "IsTotem()", "IsControlledByPlayer()", "GetCharmerOrOwnerPlayerOrPlayerItself()", "IsInWorld()"):
            self.assertIn(token, world)

    def test_every_registry_use_holds_the_mutex(self):
        src = strip_comments(read(SCRIPT))
        for fn in ("void IndividualProgression::ResetF14LifecycleRegistry()",
                   "void IndividualProgression::ExpireF14LifecycleActor(",
                   "IndividualProgression::F14LifecycleStatus IndividualProgression::GetF14LifecycleStatus()",
                   "void OnF14AttackTargetTransition(",
                   "static void RetireIncarnation("):
            body = src.split(fn, 1)[1]
            body = body[:body.index("\n}\n") if fn.startswith(("void Ind", "Ind")) else body.index("\n    }\n")]
            self.assertIn("std::lock_guard<std::mutex> guard(LifecycleLock());", body, fn)
            self.assertLess(body.index("std::lock_guard<std::mutex> guard(LifecycleLock());"),
                            body.index("LifecycleRegistry()"), fn)


class LockOrderAndRegistration(unittest.TestCase):
    def test_logout_expiry_runs_after_identity_lock_released(self):
        core = read("src/IndividualProgression.cpp")
        seg = core.split("void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(", 1)[1].split(
            "void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(", 1)[0]
        lam_open = seg.index("[&]()")
        lock = seg.index("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);")
        lam_close = seg.index("}();")
        expire = seg.index("ExpireF14LifecycleActor(expiredLoginEpoch);")
        self.assertLess(lam_open, lock)
        self.assertLess(lock, lam_close)
        self.assertLess(lam_close, expire)
        self.assertEqual(seg.count("ExpireF14LifecycleActor("), 1)
        # Old epoch captured under the lock before it is erased.
        self.assertLess(seg.index("expiredLoginEpoch = previousEpoch->second;"),
                        seg.index("_f14VerifiedRandomBotLoginEpoch.erase(guid);"))
        self.assertNotIn("sRandomPlayerbotMgr", seg)

    def test_registered_without_touching_upstream_loader(self):
        core = read("src/IndividualProgression.cpp")
        reg = core.split("void AddSC_mod_individual_progression()", 1)[1]
        self.assertIn("AddSC_mod_individual_progression_f14_lifecycle();", reg[:reg.index("}")])
        self.assertNotIn("f14_lifecycle", read("src/IndividualProgression_loader.cpp"))
        src = read(SCRIPT)
        self.assertIn("void AddSC_mod_individual_progression_f14_lifecycle()", src)
        tail = src.split("void AddSC_mod_individual_progression_f14_lifecycle()", 1)[1]
        self.assertIn("#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION)", tail)
        self.assertIn("new IndividualProgressionF14LifecycleScript();", tail)

    def test_corpse_and_respawn_always_clear_stamp(self):
        src = strip_comments(read(SCRIPT))
        body = src.split("static void RetireIncarnation(Creature* creature, bool respawn)", 1)[1]
        body = body[:body.index("\n    }\n")]
        self.assertIn("OnCreatureCorpseOrRespawn(KeyOf(creature), stamp, respawn);", body)
        self.assertIn("SetStamp(creature, 0);", body)
        self.assertNotIn("f14ExperimentalAttackLifecycleRegistry", body)


if __name__ == "__main__":
    unittest.main()
