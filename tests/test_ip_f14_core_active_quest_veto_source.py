"""F14 #101: only a negative viewer-creature active quest rights veto.

Exact original #99+#100 transformed helper, not a parallel implementation.
The pure C++ tests use stubs. The native job separately compiles the actual
patched pinned Grim Core PlayerUpdates.cpp with real module sources.
"""
import importlib.util
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT = "8d9bb660116d196e1ea8b9d20e72f7bfb549825b"
CHANGES = {
    "tests/core_patches/apply_f14_core_active_quest_veto.py",
    "tests/f14_core_active_quest_veto_model.cpp",
    "tests/test_ip_f14_core_active_quest_veto_source.py",
    ".github/workflows/ip-f14-core-active-quest-veto-native-20261105.yml",
    "docs/F14_CORE_ACTIVE_QUEST_VETO_2026-11-05.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}
PATCH = ROOT / "tests/core_patches/apply_f14_core_active_quest_veto.py"

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          text=True, capture_output=True, check=True).stdout.strip()

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

def module_from(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

def parent_source():
    previous = module_from(ROOT / "tests/test_ip_f14_core_group_aura_veto_source.py",
                           "f14_group_aura_source")
    return '#include "Pet.h"\n#include "Player.h"\n' + previous.transformed_synthetic()

def transformed_synthetic():
    patch = module_from(PATCH, "f14_quest_veto")
    return patch.transform(parent_source())

def extract_helper():
    patch = module_from(PATCH, "f14_quest_veto")
    output = transformed_synthetic()
    return patch.BEGIN + output.split(patch.BEGIN, 1)[1].split(patch.END, 1)[0].rstrip() + "\n"

class Contracts(unittest.TestCase):
    def test_six_path_child_does_not_modify_gameplay_config_sql_or_old_guard(self):
        self.assertEqual(set(git("diff", "--name-only", PARENT, "HEAD").splitlines()), CHANGES)
        self.assertEqual(git("diff", "--name-only", PARENT, "HEAD", "--",
                              "src", "conf", "sql", "data", "optional"), "")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14LifecycleRegistry.h"),
                         "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14ViewerRestoreProtocol.h"),
                         "f520356a8152d98580751604f4649fe1b564f072")
        self.assertEqual(git("rev-parse", "HEAD:tests/core_patches/apply_f14_direct_interaction_veto.py"),
                         "98ca29d14fd702cb4f961177de16d905eaf567e3")
        self.assertEqual(git("rev-parse", "HEAD:tests/core_patches/apply_f14_core_group_aura_veto.py"),
                         "28a9c1efe2bd61a71f3f3b3948e6a14dfd3bd805")

    def test_real_previous_core_helper_is_extended_only_at_tail(self):
        original = parent_source()
        transformed = transformed_synthetic()
        function = extract_helper()
        self.assertIn("viewer->GetTarget() == creatureGuid", function)
        self.assertIn("viewer->GetPet()", function)
        self.assertIn("viewer->GetCharm()", function)
        self.assertIn("viewer->GetGuardianPet()", function)
        self.assertIn("viewer->GetGroup()", function)
        self.assertIn("creature->GetAppliedAuras()", function)
        self.assertIn("f14Auras.size() > 64", function)
        for expected in (
            "f14Slot < MAX_QUEST_LOG_SIZE",
            "GetQuestSlotQuestId(f14Slot)",
            "GetQuestStatus(f14QuestId) != QUEST_STATUS_INCOMPLETE",
            "sObjectMgr->GetQuestTemplate(f14QuestId)",
            "if (!f14Quest)",
            "RequiredNpcOrGo[f14Objective]",
            "RequiredNpcOrGoCount[f14Objective]",
            "required <= 0",
            "requiredCreature == creature->GetEntry()",
            "f14Credit < MAX_KILL_CREDIT",
            "questCreature->KillCredit[f14Credit]",
        ):
            with self.subTest(expected=expected):
                self.assertIn(expected, function)
        self.assertIn('#include "ObjectMgr.h"', transformed)
        self.assertIn('#include "QuestDef.h"', transformed)
        self.assertEqual(transformed.count("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&"), 2)
        self.assertEqual(transformed.count("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())"), 2)
        self.assertEqual(function.count("return false;"), 1)
        self.assertEqual(original.split("static bool F14ViewerHasDirectInteractionVeto(", 1)[1]
                         .split("    return false;\n}", 1)[0],
                         function.split("static bool F14ViewerHasDirectInteractionVeto(", 1)[1]
                         .split("    // Real Core quest log", 1)[0])

    def test_reapplication_or_partial_parent_refused_without_writing(self):
        patch = module_from(PATCH, "f14_quest_again")
        output = transformed_synthetic()
        with self.assertRaises(ValueError):
            patch.transform(output)
        with self.assertRaises(ValueError):
            patch.transform(parent_source().replace("viewer->GetGuardianPet()", ""))
        with self.assertRaises(ValueError):
            patch.transform(parent_source().replace('#include "Pet.h"', ""))
        with self.assertRaises(ValueError):
            patch.transform(parent_source().replace("return false;\n}", "return true;\n}"))

    def test_no_feature_activation_or_prior_hidden_clearance(self):
        patch = PATCH.read_text(encoding="utf-8")
        for forbidden in ("CompleteRestoreOnlyWithClientProof(", "ClearFrozenAfterGlobalProof(",
                          "SendDirectMessage(", "SetPhaseMask(", "OnF14ShouldSuppressCreatureForViewer("):
            self.assertNotIn(forbidden, patch)
        for runtime in ("src/IndividualProgressionF14LifecycleScript.cpp",
                        "src/IndividualProgression.cpp"):
            self.assertNotIn("F14ViewerHasDirectInteractionVeto", read(runtime))
        settings = read("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.PaidExpansion.Enable = 0", settings)
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", settings)

if __name__ == "__main__":
    unittest.main()
