"""F14 observer-specific Core visibility seam - TEST-ONLY contract.

This validates a proposed, NOT INSTALLED / NOT MERGED, pinned Core patch.
The patch adds default-ALLOW observer visibility hooks. It implements NO
Blood Elf/Draenei policy, no bot identity reader, no combat visibility
refresh and no corpse/loot mutations.

Run against disposable Core:
  COSMIC_F14_PATCHED_CORE=/path/to/patched/core python3 -B -m unittest
      discover -s tests -p test_ip_f14_core_observer_seam_prototype.py -v
"""
import os
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FROZEN = "ce69a6891470693a471896b999c4fc97424f8676"
PATCH = (ROOT / "tests/core_patches/f14_observer_visibility_default_allow_prototype.patch").read_text()
PINNED_PATHS = {
    "src/server/game/Entities/Object/Object.cpp",
    "src/server/game/Entities/Player/Player.cpp",
    "src/server/game/Scripting/ScriptMgr.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
}


def git(*args):
    return subprocess.run(
        ["git", "-C", str(ROOT), *args], text=True, capture_output=True,
        check=True,
    ).stdout.strip()


class PrototypeBoundary(unittest.TestCase):
    def test_stacked_on_frozen_f14_branch_without_runtime_changes(self):
        subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor",
                        FROZEN, "HEAD"], check=True)
        self.assertEqual(git("diff", "--name-only", FROZEN, "HEAD", "--",
                             "src", "conf", "sql", "data", "optional"), "")

    def test_only_five_existing_pinned_core_paths_touched(self):
        paths = re.findall(r"^diff --git a/(\S+) b/\1$", PATCH, re.M)
        self.assertEqual(len(paths), 5)
        self.assertEqual(set(paths), PINNED_PATHS)
        self.assertEqual(PATCH.count("diff --git "), 5)
        self.assertEqual(PATCH.count("@@ -"), 6)
        for path in paths:
            self.assertIn(f"--- a/{path}\n+++ b/{path}", PATCH)

    def test_default_allow_script_contract(self):
        self.assertIn("virtual bool OnWorldObjectCanBeSeenByPlayer(", PATCH)
        self.assertIn("return true; }", PATCH)
        self.assertIn("CALL_ENABLED_BOOLEAN_HOOKS(WorldObjectScript,", PATCH)
        self.assertIn("!script->OnWorldObjectCanBeSeenByPlayer(observer, target)", PATCH)
        self.assertIn("WORLDOBJECTHOOK_ON_WORLD_OBJECT_CAN_BE_SEEN_BY_PLAYER", PATCH)
        self.assertIn("bool ScriptMgr::OnWorldObjectCanBeSeenByPlayer(", PATCH)
        self.assertNotIn("+        return false;", PATCH)
        self.assertNotIn("+    return false;", PATCH)

    def test_visibility_checks_before_normal_always_visible_bypass(self):
        self.assertIn("if (CanNeverSee(obj))", PATCH)
        self.assertIn("if (obj->IsAlwaysVisibleFor(this) || CanAlwaysSee(obj))", PATCH)
        self.assertIn("(obj->IsPlayer() || obj->IsCreature())", PATCH)
        self.assertIn("!sScriptMgr->OnWorldObjectCanBeSeenByPlayer(observer, obj)", PATCH)
        self.assertIn("!sScriptMgr->OnWorldObjectCanBeSeenByPlayer(u, this)", PATCH)
        self.assertIn("AccountMgr::IsPlayerAccount(u->GetSession()->GetSecurity())", PATCH)
        # Stock order of self, detection and GM rights still protects self.
        self.assertIn("    if (u == this)", PATCH)
        self.assertIn("    if (IsVisible())", PATCH)

    def test_no_bot_policy_implemented_or_creature_lifecycle_changed(self):
        for forbidden in ("+    player->RewardQuest(", "+    bot->RewardQuest(",
                          "+    obj->SetPhaseMask(", "+    obj->DestroyForNearbyPlayers(",
                          "+    creature->DespawnOrUnsummon(", "66008",
                          "IsRandomBot", "RandomPlayerbotMgr"):
            self.assertNotIn(forbidden, PATCH)

    def test_ci_source_files_only(self):
        changed = git("diff", "--name-only", FROZEN, "HEAD").splitlines()
        self.assertTrue(changed)
        self.assertTrue(all(p.startswith(("tests/", "docs/", ".github/workflows/"))
                            for p in changed), changed)


class ActualPatchedCore(unittest.TestCase):
    def test_optional_disposable_core_has_default_allow_hook_and_correct_seams(self):
        root = os.environ.get("COSMIC_F14_PATCHED_CORE")
        if not root:
            self.skipTest("disposable Core checkout not available")
        root = Path(root)
        obj = (root / "src/server/game/Entities/Object/Object.cpp").read_text()
        player = (root / "src/server/game/Entities/Player/Player.cpp").read_text()
        script = (root / "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h").read_text()
        impl = (root / "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp").read_text()
        mgr = (root / "src/server/game/Scripting/ScriptMgr.h").read_text()

        self.assertEqual(script.count("virtual bool OnWorldObjectCanBeSeenByPlayer"), 1)
        self.assertIn("return true; }", script)
        self.assertEqual(mgr.count("bool OnWorldObjectCanBeSeenByPlayer("), 1)
        self.assertEqual(impl.count("bool ScriptMgr::OnWorldObjectCanBeSeenByPlayer("), 1)
        start = obj.index("bool WorldObject::CanSeeOrDetect(")
        body = obj[start:obj.index("bool WorldObject::CanNeverSee(", start)]
        self.assertLess(body.index("CanNeverSee(obj)"),
                        body.index("OnWorldObjectCanBeSeenByPlayer(observer, obj)"))
        self.assertLess(body.index("OnWorldObjectCanBeSeenByPlayer(observer, obj)"),
                        body.index("obj->IsAlwaysVisibleFor(this)"))
        start = player.index("bool Player::IsVisibleGloballyFor(")
        glob = player[start:player.index("void Player::InitPrimaryProfessions()", start)]
        self.assertLess(glob.index("if (u == this)"),
                        glob.index("OnWorldObjectCanBeSeenByPlayer(u, this)"))
        self.assertLess(glob.index("OnWorldObjectCanBeSeenByPlayer(u, this)"),
                        glob.index("if (IsVisible())"))
        self.assertIn("AccountMgr::IsPlayerAccount(u->GetSession()->GetSecurity())", glob)


if __name__ == "__main__":
    unittest.main()
