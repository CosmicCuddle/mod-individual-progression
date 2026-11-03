"""Source contracts for F14 event callbacks in disposable Core and disabled IP branch."""
import os
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = (ROOT / "tests/core_patches/f14_attack_corpse_event_bridge_prototype.patch").read_text()
P = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()
CORE = (ROOT / "src/IndividualProgression.cpp").read_text()

def method(text, head):
    start = text.index(head)
    begin = text.index("{", start)
    depth = 0
    for n in range(begin, len(text)):
        if text[n] == "{": depth += 1
        if text[n] == "}":
            depth -= 1
            if not depth: return text[start:n+1]
    raise AssertionError("unbalanced method: " + head)

class F14TransitionBridge(unittest.TestCase):
    def test_disposable_patch_scope(self):
        paths = re.findall(r"^diff --git a/(\S+) b/\1$", PATCH, re.M)
        self.assertEqual(set(paths), {
            "src/server/game/Scripting/ScriptDefines/UnitScript.h",
            "src/server/game/Scripting/ScriptDefines/UnitScript.cpp",
            "src/server/game/Scripting/ScriptMgr.h",
            "src/server/game/Entities/Unit/Unit.cpp",
            "src/server/game/Entities/Creature/Creature.cpp"
        })
        self.assertEqual(len(paths), 5)
        self.assertEqual(PATCH.count("@@ -"), 7)

    def test_distinct_partial_contract(self):
        self.assertIn("#define COSMIC_F14_ATTACK_EVENT_HOOK_VERSION 1", PATCH)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", PATCH)
        self.assertIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", CORE)
        self.assertIn("#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION)", P)
        self.assertIn("new IndividualProgressionF14AttackEventScript();", P)
        self.assertIn("UNITHOOK_ON_F14_ATTACK_TARGET_TRANSITION", PATCH)
        self.assertIn("UNITHOOK_ON_F14_CREATURE_CORPSE_READY", PATCH)

    def test_attack_event_positions(self):
        unit = PATCH.split("diff --git a/src/server/game/Entities/Unit/Unit.cpp", 1)[1].split("\ndiff --git a/",1)[0]
        self.assertIn("Unit* const f14PreviousVictim = m_attacking;", unit)
        self.assertIn("sScriptMgr->OnF14AttackTargetTransition(this, f14PreviousVictim, victim);", unit)
        self.assertIn("sScriptMgr->OnF14AttackTargetTransition(this, victim, nullptr);", unit)
        self.assertIn("m_attacking = victim;", unit)
        self.assertIn("SendMeleeAttackStop(victim);", unit)
        self.assertIn("Unit* owner = GetCharmerOrOwner();", unit)
        self.assertLess(unit.index("m_attacking = victim;"),
                        unit.index("sScriptMgr->OnF14AttackTargetTransition(this, f14PreviousVictim, victim);"))
        self.assertLess(unit.index("sScriptMgr->OnF14AttackTargetTransition(this, f14PreviousVictim, victim);"),
                        unit.index("Unit* owner = GetCharmerOrOwner();"),
                        "Notify before unrelated AI or pet callbacks can invalidate old target")

    def test_corpse_after_state_promotion(self):
        corpse = PATCH.split("diff --git a/src/server/game/Entities/Creature/Creature.cpp", 1)[1]
        self.assertIn("Unit::setDeathState(DeathState::Corpse, despawn);", corpse)
        self.assertIn("sScriptMgr->OnF14CreatureCorpseReady(this);", corpse)
        self.assertLess(corpse.index("Unit::setDeathState(DeathState::Corpse, despawn);"),
                        corpse.index("sScriptMgr->OnF14CreatureCorpseReady(this);"))

    def test_verified_rndbot_only_and_force_immediate(self):
        cls = P.split("class IndividualProgressionF14AttackEventScript",1)[1].split("\n#endif",1)[0]
        attack = method(cls, "void OnF14AttackTargetTransition(")
        for yes in ("!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility",
                    "attacker->IsPlayer()", "candidate->IsInWorld()",
                    "candidate->GetSession()->IsHeadless()",
                    "candidate->getRace() != RACE_DRAENEI",
                    "candidate->getRace() != RACE_BLOODELF",
                    "HasF14VerifiedRandomBotIdentity(candidate)",
                    "oldVictim != newVictim", "oldVictim->UpdateObjectVisibility(true)",
                    "newVictim->UpdateObjectVisibility(true)"):
            self.assertIn(yes, attack)
        corpse = method(cls, "void OnF14CreatureCorpseReady(")
        self.assertIn("!sIndividualProgression->f14ExperimentalRndbotPlayerVisibility", corpse)
        self.assertIn("creature->UpdateObjectVisibility(true)", corpse)
        for code in ("UpdateObjectVisibility(false)", "sRandomPlayerbotMgr", "ModifyMoney(", "RewardQuest(",
                     "SetPhaseMask(", "DestroyForVisiblePlayers(", "SaveToDB(", "CharacterDatabase"):
            self.assertNotIn(code, attack.replace("Unit::UpdateObjectVisibility(false)'s delayed notification", ""))
            self.assertNotIn(code, corpse)

    def test_real_patched_core_when_available(self):
        path = os.environ.get("COSMIC_F14_PATCHED_CORE")
        if not path:
            self.skipTest("disposable Core not supplied")
        root = Path(path) / "src/server/game"
        unit = (root/"Entities/Unit/Unit.cpp").read_text()
        creature = (root/"Entities/Creature/Creature.cpp").read_text()
        script = (root/"Scripting/ScriptDefines/UnitScript.h").read_text()
        mgr = (root/"Scripting/ScriptMgr.h").read_text()
        self.assertIn("COSMIC_F14_ATTACK_EVENT_HOOK_VERSION", script)
        self.assertIn("void OnF14AttackTargetTransition(", mgr)
        self.assertEqual(unit.count("sScriptMgr->OnF14AttackTargetTransition(this,"), 2)
        self.assertEqual(creature.count("sScriptMgr->OnF14CreatureCorpseReady(this);"), 1)
        self.assertNotIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", script)

if __name__ == "__main__":
    unittest.main()
