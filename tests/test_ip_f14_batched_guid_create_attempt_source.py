"""Source contracts for CI-only F14 batched CREATE send-attempt attribution.

No script returns TRUE, no feature hide, no client ACK or ledger clearance.
The pinned Core checkout is supplied by hosted CI after #86 and #93 patchers.
"""
import ast
import os
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tests/core_patches/apply_f14_batched_guid_create_attempt.py"
SOURCE = PATCHER.read_text(encoding="utf-8")
CORE = Path(os.environ["COSMIC_F14_PINNED_CORE_ROOT"]) if os.environ.get("COSMIC_F14_PINNED_CORE_ROOT") else None


def core(name):
    if CORE is None:
        raise RuntimeError("requires disposable Core tree")
    return (CORE / name).read_text(encoding="utf-8")


class SourceOnly(unittest.TestCase):
    def test_patcher_parses_and_is_transactional(self):
        ast.parse(SOURCE)
        self.assertIn("Transaction-like source validation: EVERY anchor before ANY disk write.", SOURCE)
        self.assertIn("missing exact #93 source-attributed batch precursor", SOURCE)
        self.assertIn("missing default-false #86 visibility precursor", SOURCE)
        self.assertIn("if any(changed[name] == originals[name] for name in FILES):", SOURCE)
        self.assertNotIn("SetPhaseMask(", SOURCE)
        self.assertNotIn("CompleteRestoreOnlyWithClientProof(", SOURCE)
        self.assertNotIn("ClearFrozenAfterGlobalProof(", SOURCE)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", SOURCE)
        self.assertNotIn("SendDirectMessage(&f14", SOURCE)

    def test_child_does_not_wire_a_positive_ip_runtime_listener(self):
        for name in ("src/IndividualProgression.cpp",
                     "src/IndividualProgressionPlayer.cpp",
                     "src/IndividualProgressionF14LifecycleScript.cpp"):
            body = (ROOT / name).read_text(encoding="utf-8")
            self.assertNotIn("OnF14ShouldAttributeCreatureCreateAttempt(", body, name)
            self.assertNotIn("OnF14VisibilityBatchCreateAttempt(", body, name)
        self.assertIn("return false; }", SOURCE)


@unittest.skipUnless(CORE, "pinned disposable Core supplied only by native CI")
class PinnedCore(unittest.TestCase):
    def test_batched_only_and_after_actual_normal_create(self):
        body = core("src/server/game/Entities/Player/PlayerUpdates.cpp")
        batch = body.split("void Player::UpdateVisibilityOf(T* target, UpdateData& data,", 1)[1].split(
            "void Player::GetInitialVisiblePackets(Unit* target)", 1)[0]
        direct = body.split("void Player::UpdateVisibilityOf(WorldObject* target)", 1)[1].split(
            "void Player::UpdateTriggerVisibility()", 1)[0]
        token = "data.RememberF14CreateAttemptGUID(target->GetGUID());"
        self.assertEqual(batch.count(token), 1)
        self.assertIn("if (target->IsCreature() && !target->ToCreature()->IsPet() &&", batch)
        self.assertIn("sScriptMgr->ShouldF14AttributeCreatureCreateAttemptForViewer(this, target)", batch)
        self.assertLess(batch.index("target->BuildCreateUpdateBlockForPlayer(&data, this);"), batch.index(token))
        self.assertLess(batch.index("UpdateVisibilityOf_helper(this, target, visibleNow);"), batch.index(token))
        self.assertNotIn("RememberF14CreateAttemptGUID(", direct)
        self.assertNotIn("ShouldF14AttributeCreatureCreateAttemptForViewer(", direct)

    def test_both_directions_share_512_limit_and_abort_all_attribution(self):
        h = core("src/server/game/Entities/Object/Updates/UpdateData.h")
        cpp = core("src/server/game/Entities/Object/Updates/UpdateData.cpp")
        self.assertEqual(h.count("_f14OutOfRangeGuids.size() + _f14CreateAttemptGuids.size() >= 512"), 2)
        self.assertEqual(h.count("_f14AttributionOverflow = true;"), 2)
        self.assertEqual(h.count("_f14OutOfRangeGuids.clear();"), 2)
        self.assertEqual(h.count("_f14CreateAttemptGuids.clear();"), 2)
        self.assertIn("GuidVector _f14CreateAttemptGuids;", h)
        self.assertIn("GuidVector const& F14CreateAttemptGUIDsForAttemptOnly() const", h)
        clear = cpp.split("void UpdateData::Clear()", 1)[1]
        for token in ("_f14OutOfRangeGuids.clear();", "_f14CreateAttemptGuids.clear();",
                      "_f14AttributionOverflow = false;"):
            self.assertIn(token, clear)
        packet = cpp.split("bool UpdateData::BuildPacket(WorldPacket& packet)", 1)[1].split(
            "void UpdateData::Clear()", 1)[0]
        self.assertNotIn("_f14", packet)
        self.assertIn("for (ObjectGuid const& guid : m_outOfRangeGUIDs)", packet)

    def test_per_guid_create_attempt_after_send_without_client_receipt(self):
        grid = core("src/server/game/Grids/Notifiers/GridNotifiers.cpp")
        section = grid.split("void VisibleNotifier::SendToSelf()", 1)[1].split(
            "void VisibleChangesNotifier::Visit", 1)[0]
        event = "sScriptMgr->OnF14VisibilityBatchCreateAttempt(&i_player, f14Guid);"
        self.assertEqual(section.count(event), 1)
        self.assertLess(section.index("i_player.SendDirectMessage(&packet);"), section.index(event))
        self.assertIn("if (!i_data.F14OutOfRangeAttributionOverflowed())", section)
        self.assertIn("for (ObjectGuid const& f14Guid : i_data.F14CreateAttemptGUIDsForAttemptOnly())", section)
        self.assertNotIn("RememberF14CreateAttemptGUID(", section)
        self.assertNotIn("CompleteRestoreOnlyWithClientProof(", section)
        self.assertNotIn("ClientAcknowledged", section)

    def test_f14_query_is_default_false_and_event_observer_only(self):
        mgr = core("src/server/game/Scripting/ScriptMgr.h")
        h = core("src/server/game/Scripting/ScriptDefines/WorldObjectScript.h")
        cpp = core("src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp")
        self.assertIn("ShouldF14AttributeCreatureCreateAttemptForViewer(", mgr)
        self.assertIn("OnF14VisibilityBatchCreateAttempt(", mgr)
        self.assertIn("WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTRIBUTION_QUERY", h)
        self.assertIn("WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTEMPT", h)
        self.assertIn("OnF14ShouldAttributeCreatureCreateAttempt(", h)
        query = h.split("OnF14ShouldAttributeCreatureCreateAttempt(", 1)[1].split("}", 1)[0]
        self.assertIn("return false;", query)
        self.assertIn("CALL_ENABLED_BOOLEAN_HOOKS_WITH_DEFAULT_FALSE(WorldObjectScript", cpp)
        self.assertIn("void ScriptMgr::OnF14VisibilityBatchCreateAttempt(", cpp)
        for body in (mgr, h, cpp):
            self.assertNotIn("ClientRestored", body)
            self.assertNotIn("CompleteRestoreOnlyWithClientProof(", body)


if __name__ == "__main__":
    unittest.main()
