"""F14 group/active-aura Core veto child of #99: all OFF and fail visible.

The companion C++ test runs the exact Core helper text produced by applying
this patch to the immutable #99 helper, against stubs that mimic its current
Core interfaces. This is not real client/gameplay/rights coverage.
"""
import ast
import importlib.util
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT = "0856c31fb43731821f45405ce6be8f8bebc7d4bc"
CHANGES = {
    "tests/core_patches/apply_f14_core_group_aura_veto.py",
    "tests/f14_core_group_aura_veto_model.cpp",
    "tests/test_ip_f14_core_group_aura_veto_source.py",
    ".github/workflows/ip-f14-core-group-aura-veto-model-20261105.yml",
    "docs/F14_CORE_GROUP_AURA_VETO_2026-11-05.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}
OLD = ROOT / "tests/core_patches/apply_f14_direct_interaction_veto.py"
NEW = ROOT / "tests/core_patches/apply_f14_core_group_aura_veto.py"

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          text=True, capture_output=True, check=True).stdout.strip()

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

def old_helper():
    tree = ast.parse(OLD.read_text(encoding="utf-8"))
    return next(ast.literal_eval(node.value) for node in tree.body
                if isinstance(node, ast.Assign)
                and any(isinstance(t, ast.Name) and t.id == "HELPER" for t in node.targets))

def patch_module():
    spec = importlib.util.spec_from_file_location("f14_group_aura_patcher", NEW)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def transformed_synthetic():
    module = patch_module()
    # The helper is taken byte-for-byte from the previous #99 *source*.
    # This synthetic wrapper exists only to unit-test #100's exact transformer;
    # the future native runner must separately apply both to pinned Grim Core.
    before = '#include "SpellAuraEffects.h"\n' + old_helper()
    after = """
template <class T>
void Player::UpdateVisibilityOf(T* target, UpdateData& data, std::vector<Unit*>& visibleNow)
{
    bool guard = sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&
        !F14ViewerHasDirectInteractionVeto(this, target->ToCreature());
}
void Player::UpdateVisibilityOf(WorldObject* target)
{
    bool guard = sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&
        !F14ViewerHasDirectInteractionVeto(this, target->ToCreature());
}
"""
    return module.transform(before + after)

def extract_helper(output):
    module = patch_module()
    return module.HELPER_BEGIN + output.split(module.HELPER_BEGIN, 1)[1].split(
        module.HELPER_END, 1)[0].rstrip() + "\n"

class SourceContracts(unittest.TestCase):
    def test_exact_six_new_review_paths_no_core_module_runtime_edits(self):
        self.assertEqual(set(git("diff", "--name-only", PARENT, "HEAD").splitlines()), CHANGES)
        self.assertEqual(git("diff", "--name-only", PARENT, "HEAD", "--", "src", "conf", "sql",
                              "data", "optional"), "")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14LifecycleRegistry.h"),
                         "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14ViewerRestoreProtocol.h"),
                         "f520356a8152d98580751604f4649fe1b564f072")
        self.assertEqual(git("rev-parse", "HEAD:tests/core_patches/apply_f14_direct_interaction_veto.py"),
                         "98ca29d14fd702cb4f961177de16d905eaf567e3")

    def test_core_helper_real_source_is_preserved_and_only_extended(self):
        original = old_helper()
        updated = transformed_synthetic()
        helper = extract_helper(updated)
        self.assertEqual(updated.count("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())"), 2)
        self.assertEqual(updated.count("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&"), 2)
        self.assertIn("viewer->GetTarget() == creatureGuid", helper)
        self.assertIn("viewer->GetPet()", helper)
        self.assertIn("creature->GetLootRecipientGroupGUID() != 0", helper)
        self.assertIn("viewer->GetGroup()", helper)
        self.assertIn("creature->GetAppliedAuras()", helper)
        self.assertIn("f14Auras.size() > 64", helper)
        self.assertIn("if (!app || !app->GetBase())", helper)
        self.assertIn("if (!caster || caster.IsPlayer())", helper)
        self.assertNotIn("ClientAcknowledged", helper)
        self.assertNotIn("ClearFrozenAfterGlobalProof", helper)
        self.assertNotIn("SendDirectMessage", helper)
        self.assertNotIn("SetPhaseMask", helper)
        self.assertLess(helper.index("creature->GetLootRecipientGroupGUID()"),
                        helper.index("viewer->GetGroup()"))
        self.assertEqual(helper.count("return false;"), 1)
        # Original helper is a prefix through the original return statement.
        self.assertEqual(original.split("static bool F14ViewerHasDirectInteractionVeto(", 1)[1]
                         .split("    return false;\n}", 1)[0],
                         helper.split("static bool F14ViewerHasDirectInteractionVeto(", 1)[1]
                         .split("    // Without a complete audited", 1)[0])

    def test_missing_parent_and_repatch_always_refuse(self):
        module = patch_module()
        source = transformed_synthetic()
        with self.assertRaises(ValueError):
            module.transform(source)
        with self.assertRaises(ValueError):
            module.transform("#include \"SpellAuraEffects.h\"\n")
        with self.assertRaises(ValueError):
            module.transform(source.replace('#include "SpellAuraEffects.h"', ""))
        with self.assertRaises(ValueError):
            module.transform(source.replace("if (Pet const* pet = viewer->GetPet())", ""))

    def test_no_positive_runtime_hide_listener_or_flag_activation(self):
        for p in ("src/IndividualProgression.cpp", "src/IndividualProgressionPlayer.cpp",
                  "src/IndividualProgressionF14LifecycleScript.cpp"):
            self.assertNotIn("F14ViewerHasDirectInteractionVeto", read(p))
        self.assertNotIn("OnF14ShouldSuppressCreatureForViewer(", NEW.read_text())
        cfg = read("conf/individualProgression.conf.dist")
        for key in ("IndividualProgression.PaidExpansion.Enable",
                    "IndividualProgression.F14.ExperimentalRndbotPlayerVisibility"):
            self.assertIn(key + " = 0", cfg)

if __name__ == "__main__":
    unittest.main()
