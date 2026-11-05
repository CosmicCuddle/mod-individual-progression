"""Source-only contract for F14-attributed per-GUID batch SEND ATTEMPT.

The F14 suppression query is default false and has no positive consumer.
Even after this event, transport, delivery and client display are UNPROVEN.
"""
import ast
import os
import unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PATCH = (ROOT/"tests/core_patches/apply_f14_batched_guid_send_attempt.py").read_text()
CORE = Path(os.environ["COSMIC_F14_PINNED_CORE_ROOT"]) if os.environ.get("COSMIC_F14_PINNED_CORE_ROOT") else None

def core(path):
    if not CORE:
        raise RuntimeError("missing disposable Core env")
    return (CORE/path).read_text()

class Source(unittest.TestCase):
    def test_exact_patcher_and_no_runtime_ip_activation(self):
        ast.parse(PATCH)
        self.assertIn("Validate ALL exact source anchors before writing ANY file",PATCH)
        self.assertIn("MISSING default-false #86 prerequisite",PATCH)
        self.assertIn("MISSING #86 source-attributed viewer decisions",PATCH)
        self.assertIn("    if (_f14AttributionOverflow)",PATCH)
        self.assertIn("_f14OutOfRangeGuids.clear();",PATCH)
        self.assertNotIn("SetPhaseMask(",PATCH)
        self.assertNotIn("CompleteRestoreOnlyWithClientProof(",PATCH)
        self.assertNotIn("SendDirectMessage(&f14",PATCH)
        for file in ("src/IndividualProgressionF14LifecycleScript.cpp",
                     "src/IndividualProgression.cpp"):
            self.assertNotIn("OnF14VisibilityBatchOutOfRangeAttempt(",(ROOT/file).read_text())
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION",PATCH)

@unittest.skipUnless(CORE,"Core checkout provided only in native job")
class CoreProvenance(unittest.TestCase):
    def test_only_source_attributed_f14_batched_unlink_tags_guids(self):
        src=core("src/server/game/Entities/Player/PlayerUpdates.cpp")
        batch=src.split("void Player::UpdateVisibilityOf(T* target, UpdateData& data,",1)[1].split(
              "void Player::GetInitialVisiblePackets(Unit* target)",1)[0]
        direct=src.split("void Player::UpdateVisibilityOf(WorldObject* target)",1)[1].split(
              "void Player::UpdateTriggerVisibility()",1)[0]
        self.assertEqual(batch.count("data.RememberF14OutOfRangeGUID(target->GetGUID());"),1)
        self.assertLess(batch.index("target->BuildOutOfRangeUpdateBlock(&data);"),
                        batch.index("data.RememberF14OutOfRangeGUID(target->GetGUID());"))
        self.assertLess(batch.index("data.RememberF14OutOfRangeGUID(target->GetGUID());"),
                        batch.index("UnlinkWorldObjectVisibility(target);"))
        self.assertIn("if (f14PositiveSuppression)\n                data.RememberF14OutOfRangeGUID",batch)
        self.assertNotIn("RememberF14OutOfRangeGUID(",direct)
        self.assertEqual(batch.count("CanSeeOrDetect(target, false, true)"),1)
        self.assertNotIn("UpdateObjectVisibility(true)",batch)

    def test_metadata_bounded_and_never_modifies_packet_payload(self):
        h=core("src/server/game/Entities/Object/Updates/UpdateData.h")
        cpp=core("src/server/game/Entities/Object/Updates/UpdateData.cpp")
        for x in ("_f14AttributionOverflow = true;","_f14OutOfRangeGuids.clear();",
                  "if (_f14OutOfRangeGuids.size() >= 512)","_f14OutOfRangeGuids.push_back(guid);"):
            self.assertIn(x,h)
        self.assertIn("_f14OutOfRangeGuids.clear();",cpp.split("void UpdateData::Clear()",1)[1])
        self.assertIn("_f14AttributionOverflow = false;",cpp.split("void UpdateData::Clear()",1)[1])
        # Actual packet builder deliberately has no branch for F14 metadata.
        packet=cpp.split("bool UpdateData::BuildPacket(WorldPacket& packet)",1)[1].split(
            "void UpdateData::Clear()",1)[0]
        self.assertNotIn("_f14",packet)
        self.assertIn("for (ObjectGuid const& guid : m_outOfRangeGUIDs)",packet)

    def test_callback_only_after_batch_senddirectmessage_call(self):
        grid=core("src/server/game/Grids/Notifiers/GridNotifiers.cpp")
        section=grid.split("void VisibleNotifier::SendToSelf()",1)[1].split(
             "void VisibleChangesNotifier::Visit",1)[0]
        self.assertEqual(section.count("sScriptMgr->OnF14VisibilityBatchOutOfRangeAttempt(&i_player, f14Guid);"),1)
        self.assertLess(section.index("i_player.SendDirectMessage(&packet);"),
                        section.index("sScriptMgr->OnF14VisibilityBatchOutOfRangeAttempt"))
        self.assertIn("if (!i_data.F14OutOfRangeAttributionOverflowed())",section)
        self.assertIn("for (ObjectGuid const& f14Guid : i_data.F14OutOfRangeGUIDsForAttemptOnly())",section)
        self.assertIn('i_data.BuildPacket(packet);',section)
        self.assertIn('i_data.AddOutOfRangeGUID(obj->GetGUID());',section)
        self.assertNotIn("RememberF14OutOfRangeGUID(",section)
        # That ordinary visible-object sweep stays unattributed.
        self.assertLess(section.index("i_data.AddOutOfRangeGUID(obj->GetGUID());"),
                        section.index("sScriptMgr->OnF14VisibilityBatchOutOfRangeAttempt"))

    def test_well_named_script_api_does_not_confuse_client_ack(self):
        h=core("src/server/game/Scripting/ScriptMgr.h")
        wh=core("src/server/game/Scripting/ScriptDefines/WorldObjectScript.h")
        wc=core("src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp")
        self.assertIn("void OnF14VisibilityBatchOutOfRangeAttempt(Player const* viewer, ObjectGuid const& creatureGuid);",h)
        self.assertIn("WORLDOBJECTHOOK_ON_F14_BATCH_OUT_OF_RANGE_ATTEMPT",wh)
        self.assertIn("virtual void OnF14VisibilityBatchOutOfRangeAttempt(",wh)
        self.assertIn("void ScriptMgr::OnF14VisibilityBatchOutOfRangeAttempt(",wc)
        self.assertIn("CALL_ENABLED_HOOKS(WorldObjectScript",wc)
        for f in [h,wh,wc,core("src/server/game/Grids/Notifiers/GridNotifiers.cpp")]:
            self.assertNotIn("ClientAcknowledged",f)
            self.assertNotIn("ClientRestored",f)
        self.assertIn("return false;",wh.split("OnF14ShouldSuppressCreatureForViewer(",1)[1].split("}",1)[0])

if __name__=="__main__":
    unittest.main()
