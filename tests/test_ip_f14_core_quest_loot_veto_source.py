"""F14 #102: actual pinned Core quest-loot table per-viewer negative veto.

This test composes original #99/#100/#101 SOURCE transforms and #102 exactly,
then runs a model under interface stubs. Not live loot rights or client ACK.
"""
import importlib.util
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT = "be42db4cf4c1a89d300aa607118c3690b4d1f287"
PATHS = {
    "tests/core_patches/apply_f14_core_quest_loot_veto.py",
    "tests/f14_core_quest_loot_veto_model.cpp",
    "tests/test_ip_f14_core_quest_loot_veto_source.py",
    ".github/workflows/ip-f14-core-quest-loot-veto-native-20261105.yml",
    "docs/F14_CORE_QUEST_LOOT_VETO_2026-11-05.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}
PATCH = ROOT / "tests/core_patches/apply_f14_core_quest_loot_veto.py"

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          text=True, capture_output=True, check=True).stdout.strip()

def file(path):
    return (ROOT / path).read_text(encoding="utf-8")

def import_file(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    obj = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(obj)
    return obj

def prior():
    mod = import_file(ROOT / "tests/test_ip_f14_core_active_quest_veto_source.py",
                      "f14_original_quest_objective")
    return mod.transformed_synthetic()

def transformed():
    mod = import_file(PATCH, "f14_quest_loot_patch")
    return mod.transform(prior())

def exact_helper():
    mod = import_file(PATCH, "f14_quest_loot_patch")
    return mod.BEGIN + transformed().split(mod.BEGIN, 1)[1].split(mod.END, 1)[0].rstrip() + "\n"

class Contracts(unittest.TestCase):
    def test_exact_additive_six_paths_parent_patches_immutable(self):
        self.assertEqual(set(git("diff", "--name-only", PARENT, "HEAD").splitlines()), PATHS)
        self.assertEqual(git("diff", "--name-only", PARENT, "HEAD", "--",
                             "src", "conf", "sql", "data", "optional"), "")
        self.assertEqual(git("rev-parse", "HEAD:tests/core_patches/apply_f14_core_active_quest_veto.py"),
                         "e91656843088a9a8b7ff771a763544746d39e2a2")
        self.assertEqual(git("rev-parse", "HEAD:tests/core_patches/apply_f14_core_group_aura_veto.py"),
                         "28a9c1efe2bd61a71f3f3b3948e6a14dfd3bd805")
        self.assertEqual(git("rev-parse", "HEAD:tests/core_patches/apply_f14_direct_interaction_veto.py"),
                         "98ca29d14fd702cb4f961177de16d905eaf567e3")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14ViewerRestoreProtocol.h"),
                         "f520356a8152d98580751604f4649fe1b564f072")

    def test_exact_helper_composes_all_priors_and_only_new_quest_loot_veto(self):
        old = prior()
        new = transformed()
        h = exact_helper()
        self.assertEqual(old.count("HaveQuestLootForPlayer("), 0)
        self.assertEqual(new.count("HaveQuestLootForPlayer("), 1)
        self.assertIn('#include "LootMgr.h"', new)
        for keep in ("viewer->GetTarget() == creatureGuid",
                     "viewer->GetPet()", "viewer->GetCharm()",
                     "viewer->GetGuardianPet()", "viewer->GetGroup()",
                     "creature->GetAppliedAuras()",
                     "viewer->GetQuestSlotQuestId(f14Slot)",
                     "questCreature->KillCredit[f14Credit]",
                     "creature->GetLootRecipientGroupGUID()"):
            with self.subTest(keep=keep):
                self.assertIn(keep, h)
        self.assertIn("questCreature->lootid &&", h)
        self.assertIn("LootTemplates_Creature.HaveQuestLootForPlayer(questCreature->lootid, viewer)", h)
        self.assertIn("if (!questCreature)\n        return true;", h)
        self.assertLess(h.index("if (!questCreature)"), h.index("HaveQuestLootForPlayer("))
        self.assertEqual(h.count("return false;"), 1)
        self.assertEqual(new.count("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&"), 2)
        self.assertEqual(new.count("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())"), 2)

    def test_wrong_parent_and_duplicate_fail_before_file_write(self):
        mod = import_file(PATCH, "f14_bad_parent")
        with self.assertRaises(ValueError):
            mod.transform(transformed())
        for missing in ('#include "ObjectMgr.h"', "viewer->GetCharm()",
                        "viewer->GetQuestSlotQuestId(f14Slot)"):
            with self.subTest(missing=missing):
                with self.assertRaises(ValueError):
                    mod.transform(prior().replace(missing, ""))
        with self.assertRaises(ValueError):
            mod.transform(prior().replace("questCreature->KillCredit[f14Credit]", ""))

    def test_no_runtime_policies_activated_or_old_ledger_clear(self):
        code = PATCH.read_text()
        for forbidden in ("CompleteRestoreOnlyWithClientProof(",
                          "ClearFrozenAfterGlobalProof(", "SendDirectMessage(",
                          "SetPhaseMask(", "OnF14ShouldSuppressCreatureForViewer("):
            self.assertNotIn(forbidden, code)
        config = file("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.PaidExpansion.Enable = 0", config)
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", config)
        self.assertNotIn("HaveQuestLootForPlayer(", file("src/IndividualProgressionF14LifecycleScript.cpp"))

if __name__ == "__main__":
    unittest.main()
