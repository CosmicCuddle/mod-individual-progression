"""#107: real F14-only Core PRE viewer-link mutation callback, strictly OFF.

Apply after #86/#93/#95/#99-#102 in a disposable Core checkout.
Synthetic Core fixtures validate TWO paths, exact tag, ordering,
all-or-nothing refusal, and idempotency (no live Core/project edits).
"""
import ast
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "46947e4c59a34a3de1ecc8cfb2f3bcc52e975154"
PATCHER = ROOT / "tests/core_patches/apply_f14_preunlink_suppression_decision.py"
LIFECYCLE = "src/IndividualProgressionF14LifecycleScript.cpp"
FILES = {
    "src/server/game/Scripting/ScriptMgr.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
    "src/server/game/Entities/Player/PlayerUpdates.cpp",
}
CHANGES = {
    LIFECYCLE,
    "tests/core_patches/apply_f14_preunlink_suppression_decision.py",
    "tests/test_ip_f14_preunlink_source_attribution.py",
    ".github/workflows/ip-f14-preunlink-source-attribution-native-20261106.yml",
    "docs/F14_PREUNLINK_SOURCE_ATTRIBUTION_2026-11-06.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True,
                          capture_output=True, text=True).stdout.strip()


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def sample_core():
    mgr = """    void OnF14CreatureSuppressionDecision(Player const* viewer, WorldObject const* creature,
                                         bool wasLinked, bool batched);
"""
    h = """    WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED,
    virtual void OnF14CreatureSuppressionDecision(Player const* /*viewer*/,
        WorldObject const* /*creature*/, bool /*wasLinked*/, bool /*batched*/) { }
"""
    cpp = """void ScriptMgr::OnF14CreatureSuppressionDecision(Player const* viewer,
        WorldObject const* creature, bool wasLinked, bool batched)
{
    CALL_ENABLED_HOOKS(WorldObjectScript, WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED,
        script->OnF14CreatureSuppressionDecision(viewer, creature, wasLinked, batched));
}
"""
    batched = """void Player::UpdateVisibilityOf(T* target, UpdateData& data,
                                 std::vector<Unit*>& visibleNow)
{
    GetMap()->AddObjectToPendingUpdateList(target);
    bool const f14PositiveSuppression = f14BaselineVisible && target->IsCreature() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&
        !F14ViewerHasDirectInteractionVeto(this, target->ToCreature());
    if (HaveAtClient(target))
    {
        if (!f14BaselineVisible || f14PositiveSuppression)
            GetObjectVisibilityContainer().UnlinkWorldObjectVisibility(target);
    }
    if (f14PositiveSuppression)
        sScriptMgr->OnF14CreatureSuppressionDecision(this, target, f14WasLinked, true);
}
"""
    direct = """void Player::UpdateVisibilityOf(WorldObject* target)
{
    bool const f14PositiveSuppression = f14BaselineVisible && target->IsCreature() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&
        !F14ViewerHasDirectInteractionVeto(this, target->ToCreature());
    if (HaveAtClient(target))
    {
        if (!f14BaselineVisible || f14PositiveSuppression)
            GetObjectVisibilityContainer().UnlinkWorldObjectVisibility(target);
    }
    if (f14PositiveSuppression)
        sScriptMgr->OnF14CreatureSuppressionDecision(this, target, f14WasLinked, false);
}
"""
    updates = ("static bool F14ViewerHasDirectInteractionVeto() { return false; }\n"
               "// HaveQuestLootForPlayer source from #102 pinned loot store\n"
               + batched + "\nvoid Player::GetInitialVisiblePackets(Unit* target)\n{}\n"
               + direct + "\nvoid Player::UpdateTriggerVisibility()\n{}\n")
    return dict(zip(sorted(FILES), [
        # set explicitly below, not filesystem sorted semantics
    ])) if False else {
        "src/server/game/Scripting/ScriptMgr.h": mgr,
        "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h": h,
        "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp": cpp,
        "src/server/game/Entities/Player/PlayerUpdates.cpp": updates,
    }


class ScopeAndImmutable(unittest.TestCase):
    def test_exact_six_paths_and_original_claude_preserved(self):
        self.assertEqual(set(git("diff", "--name-only", BASE, "HEAD").splitlines()), CHANGES)
        self.assertEqual(git("diff", "--name-only", BASE, "HEAD", "--",
                             "conf", "sql", "data", "optional",
                             "src/IndividualProgression.cpp",
                             "src/IndividualProgressionExpansion.cpp",
                             "src/IndividualProgressionF14ViewerRestoreProtocol.h",
                             "src/IndividualProgressionF14PriorHiddenLedger.h"), "")
        for path, sha in {
            "src/IndividualProgressionF14LifecycleRegistry.h": "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8",
            "src/IndividualProgressionF14ViewerRestoreProtocol.h": "f520356a8152d98580751604f4649fe1b564f072",
            "src/IndividualProgressionF14PriorHiddenLedger.h": "2250bae56bb09a810d337927f738bd3cbcf5fddd",
            "src/IndividualProgressionF14GuidSendAttemptEvidence.h": "f89e87428ffe173468ad60a3cfcad1b4f6bf1c81",
        }.items():
            self.assertEqual(git("rev-parse", "HEAD:" + path), sha, path)

    def test_previous_actual_ip_runtime_outside_new_observer_unchanged(self):
        original = git("show", BASE + ":" + LIFECYCLE)
        current = read(LIFECYCLE).rstrip("\n")
        marker = "class IndividualProgressionF14TaggedGuidObserverScript final : public WorldObjectScript"
        self.assertEqual(current.split(marker, 1)[0], original.split(marker, 1)[0])
        self.assertEqual(
            current.split("void AddSC_mod_individual_progression_f14_lifecycle()", 1)[1],
            original.split("void AddSC_mod_individual_progression_f14_lifecycle()", 1)[1])
        self.assertIn("void OnF14BeforeCreatureSuppression(Player const* viewer,", current)
        self.assertIn("WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_BEFORE_SUPPRESSION", current)
        self.assertIn("InspectCurrentSourceIdentity(viewer, target, true);", current)
        self.assertIn("InspectCurrentSourceIdentity(viewer, target, false);", current)
        self.assertIn("bool beforeViewerLinkMutation", current)
        self.assertIn("_beforeViewerLinkSnapshots{0}", current)
        self.assertNotIn("OnF14ShouldSuppressCreatureForViewer(", current)

    def test_patcher_is_restrictive_and_both_paths(self):
        p = read("tests/core_patches/apply_f14_preunlink_suppression_decision.py")
        ast.parse(p)
        for f in FILES:
            self.assertIn('"' + f + '"', p)
        self.assertIn("expected one anchor", p)
        self.assertIn("validate all", p)
        self.assertIn("f14PositiveSuppression", p)
        self.assertIn("OnF14BeforeCreatureSuppression(this, target, f14WasLinked", p)
        self.assertIn("missing #99/#102 negative human rights overlays", p)
        for forbidden in ("SetPhaseMask(", "WorldPacket(", "UpdateObjectVisibility(",
                          "SendDirectMessage(", "RemoveRewardedQuest("):
            self.assertNotIn(forbidden, p)
        conf = read("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", conf)
        self.assertIn("IndividualProgression.PaidExpansion.Enable = 0", conf)


class CoreFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for path, contents in sample_core().items():
            file = self.root / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text(contents, encoding="utf-8")

    def apply(self):
        return subprocess.run([sys.executable, "-B", str(PATCHER), str(self.root)],
                              capture_output=True, text=True)

    def test_prelink_both_paths_and_old_posthook_unchanged(self):
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        s = (self.root / "src/server/game/Entities/Player/PlayerUpdates.cpp").read_text()
        for flag in ("true", "false"):
            call = "OnF14BeforeCreatureSuppression(this, target, f14WasLinked, " + flag + ")"
            post = "OnF14CreatureSuppressionDecision(this, target, f14WasLinked, " + flag + ")"
            self.assertEqual(s.count(call), 1)
            self.assertEqual(s.count(post), 1)
        segments = [
            s.split("void Player::UpdateVisibilityOf(T* target,", 1)[1].split(
                "void Player::GetInitialVisiblePackets(", 1)[0],
            s.split("void Player::UpdateVisibilityOf(WorldObject* target)", 1)[1].split(
                "void Player::UpdateTriggerVisibility()", 1)[0]
        ]
        for block in segments:
            self.assertLess(block.index("bool const f14PositiveSuppression"),
                            block.index("OnF14BeforeCreatureSuppression("))
            self.assertLess(block.index("OnF14BeforeCreatureSuppression("),
                            block.index("if (HaveAtClient(target))"))
            self.assertLess(block.index("if (HaveAtClient(target))"),
                            block.index("OnF14CreatureSuppressionDecision("))
        h = (self.root / "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h").read_text()
        self.assertIn("WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_BEFORE_SUPPRESSION", h)
        self.assertIn("virtual void OnF14BeforeCreatureSuppression(", h)
        self.assertIn("OnF14CreatureSuppressionDecision(", h)

    def test_failure_is_atomic_without_partial_core_changes(self):
        snapshot = sample_core()
        missing = "src/server/game/Scripting/ScriptMgr.h"
        (self.root / missing).write_text("no source callback\n", encoding="utf-8")
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        for path, original in snapshot.items():
            if path != missing:
                self.assertEqual((self.root / path).read_text(encoding="utf-8"), original, path)

    def test_reapply_refused_without_second_mutation(self):
        first = self.apply()
        self.assertEqual(first.returncode, 0, first.stderr)
        prior = {p: (self.root / p).read_text() for p in FILES}
        second = self.apply()
        self.assertNotEqual(second.returncode, 0)
        self.assertEqual(prior, {p: (self.root / p).read_text() for p in FILES})


if __name__ == "__main__":
    unittest.main()
