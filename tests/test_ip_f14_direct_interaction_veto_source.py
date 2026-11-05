"""F14 Core pinned-viewer direct-interest negative veto, never positive policy.

When COSMIC_F14_PINNED_CORE_ROOT is set, assert the AFTER-#95 disposable
Core includes the veto on both visibility paths. Does not claim complete
quest/loot/group/DoT rights or a 3.3.5a client hide/restore.
"""
import ast
import os
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "tests/core_patches/apply_f14_direct_interaction_veto.py"
SOURCE = PATCH.read_text(encoding="utf-8")
CORE = Path(os.environ["COSMIC_F14_PINNED_CORE_ROOT"]) if os.environ.get("COSMIC_F14_PINNED_CORE_ROOT") else None


def core(path):
    if CORE is None:
        raise RuntimeError("Core path not supplied in source-only job")
    return (CORE / path).read_text(encoding="utf-8")


class SourceOnly(unittest.TestCase):
    def test_valid_exact_transaction_and_f14_off(self):
        ast.parse(SOURCE)
        self.assertIn("All anchors are validated before constructing/writing any change", SOURCE)
        self.assertIn("if s.count(OLD) != 2:", SOURCE)
        self.assertIn("missing exact #95 Core CREATE provenance prerequisite", SOURCE)
        self.assertIn("missing exact #93 Core OUT provenance prerequisite", SOURCE)
        self.assertIn("veto must execute only following positive F14", SOURCE)
        self.assertIn('if "#include \\"Pet.h\\"" not in s:', SOURCE.replace("'", '"'))
        self.assertNotIn("SetPhaseMask(", SOURCE)
        self.assertNotIn("SendDirectMessage(&", SOURCE)
        self.assertNotIn("CompleteRestoreOnlyWithClientProof(", SOURCE)
        self.assertNotIn("ClearFrozenAfterGlobalProof(", SOURCE)
        self.assertNotIn("OnF14VisibilityBatchOutOfRangeAttempt(&", SOURCE)
        self.assertNotIn("WORLDOBJECTHOOK_ON_F14", SOURCE)

    def test_narrow_negative_interaction_cases_and_no_positive_provider(self):
        needed = (
            "viewer->IsGameMaster()",
            "!creature->IsAlive()",
            "viewer->GetMap() != creature->GetMap()",
            "viewer->GetTarget() == creatureGuid",
            "viewer->GetVictim() == creature",
            "creature->GetVictim() == viewer",
            "creature->GetLootRecipientGroupGUID() != 0",
            "creature->GetLootRecipientGUID() == viewer->GetGUID()",
            "Pet const* pet = viewer->GetPet()",
            "pet->GetTarget() == creatureGuid",
            "pet->GetVictim() == creature",
            "creature->GetVictim() == pet",
        )
        for token in needed:
            with self.subTest(token=token):
                self.assertIn(token, SOURCE)
        self.assertIn("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&", SOURCE)
        self.assertIn("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature());", SOURCE)
        self.assertNotIn("OnF14ShouldSuppressCreatureForViewer(", SOURCE)
        for path in (
            "src/IndividualProgressionF14LifecycleScript.cpp",
            "src/IndividualProgression.cpp",
            "src/IndividualProgressionPlayer.cpp",
        ):
            body = (ROOT / path).read_text(encoding="utf-8")
            self.assertNotIn("F14ViewerHasDirectInteractionVeto", body)
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0",
                      (ROOT / "conf/individualProgression.conf.dist").read_text())


@unittest.skipUnless(CORE, "pinned Core only in native disposable job")
class CoreAfterOverlay(unittest.TestCase):
    def test_actual_core_both_paths_and_short_circuit(self):
        src = core("src/server/game/Entities/Player/PlayerUpdates.cpp")
        self.assertEqual(src.count("static bool F14ViewerHasDirectInteractionVeto("), 1)
        self.assertIn('#include "Pet.h"', src)
        b = src.split("void Player::UpdateVisibilityOf(T* target, UpdateData& data,", 1)[1].split(
            "void Player::GetInitialVisiblePackets(Unit* target)", 1)[0]
        d = src.split("void Player::UpdateVisibilityOf(WorldObject* target)", 1)[1].split(
            "void Player::UpdateTriggerVisibility()", 1)[0]
        for segment in (b, d):
            with self.subTest(segment=segment[:80]):
                self.assertEqual(segment.count("ShouldF14SuppressCreatureForViewer(this, target)"), 1)
                self.assertEqual(segment.count("F14ViewerHasDirectInteractionVeto(this, target->ToCreature())"), 1)
                self.assertIn("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&\n"
                              "        !F14ViewerHasDirectInteractionVeto", segment)
                self.assertIn("if (!f14BaselineVisible || f14PositiveSuppression)", segment)
                self.assertIn("if (f14BaselineVisible && !f14PositiveSuppression)", segment)
                self.assertEqual(segment.count("CanSeeOrDetect(target, false, true)"), 1)
                self.assertEqual(segment.count("OnF14CreatureSuppressionDecision(this, target"), 1)
                self.assertNotIn("SetPhaseMask(", segment)
                self.assertLess(segment.index("ShouldF14SuppressCreatureForViewer(this, target)"),
                                segment.index("!F14ViewerHasDirectInteractionVeto"))

    def test_only_pinned_playerupdates_negative_guard_changes_packet_nothing(self):
        helper = core("src/server/game/Entities/Player/PlayerUpdates.cpp").split(
            "static bool F14ViewerHasDirectInteractionVeto(", 1)[1].split("template <class T>", 1)[0]
        for key in ("return true;", "return false;", "creature->GetVictim()", "viewer->GetPet()"):
            self.assertIn(key, helper)
        self.assertNotIn("Send", helper)
        self.assertNotIn("Set", helper)
        self.assertNotIn("sScriptMgr", helper)
        self.assertIn("return false;", core("src/server/game/Scripting/ScriptDefines/WorldObjectScript.h").split(
            "OnF14ShouldSuppressCreatureForViewer(", 1)[1].split("}", 1)[0])
        self.assertNotIn("F14ViewerHasDirectInteractionVeto", core(
            "src/server/game/Entities/Object/Updates/UpdateData.cpp"))


if __name__ == "__main__":
    unittest.main()
