#!/usr/bin/env python3
"""CI-only Core child of #93: per-GUID batched CREATE *send-attempt* observation.

A new source-attribution query is permanently default FALSE with no registered
provider. Only a future separately verified previously-F14-hidden pair may
be nominated as a CREATE attempt. The callback fires after SendDirectMessage
was CALLED, not after network delivery, client rendering or restore proof.

This adds no gameplay rights or feature enablement and never clears the ledger.
"""
from pathlib import Path
import sys

FILES = {
    "src/server/game/Entities/Object/Updates/UpdateData.h",
    "src/server/game/Entities/Object/Updates/UpdateData.cpp",
    "src/server/game/Entities/Player/PlayerUpdates.cpp",
    "src/server/game/Grids/Notifiers/GridNotifiers.cpp",
    "src/server/game/Scripting/ScriptMgr.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
}

def one(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise ValueError(f"{label}: expected ONE exact anchor; found {count}")
    return source.replace(old, new, 1)

def data_header(s: str) -> str:
    s = one(s, "_f14OutOfRangeGuids.size() >= 512",
            "_f14OutOfRangeGuids.size() + _f14CreateAttemptGuids.size() >= 512",
            "UpdateData shared 512 capacity")
    s = one(s, """            _f14OutOfRangeGuids.clear();
            _f14AttributionOverflow = true;""",
            """            _f14OutOfRangeGuids.clear();
            _f14CreateAttemptGuids.clear();
            _f14AttributionOverflow = true;""",
            "UpdateData overflow drops BOTH directions")
    anchor = "    [[nodiscard]] bool F14OutOfRangeAttributionOverflowed() const { return _f14AttributionOverflow; }"
    addition = """
    // Restored CREATE provenance is an opt-in *observation* ONLY.
    // A future positive script must prove this precise viewer/session and
    // creature incarnation were previously suppressed specifically by F14.
    // This is not a client ACK, a ledger completion or a request to create.
    void RememberF14CreateAttemptGUID(ObjectGuid guid)
    {
        if (_f14AttributionOverflow)
            return;
        if (_f14OutOfRangeGuids.size() + _f14CreateAttemptGuids.size() >= 512)
        {
            _f14OutOfRangeGuids.clear();
            _f14CreateAttemptGuids.clear();
            _f14AttributionOverflow = true;
            return;
        }
        _f14CreateAttemptGuids.push_back(guid);
    }
    [[nodiscard]] GuidVector const& F14CreateAttemptGUIDsForAttemptOnly() const
    {
        return _f14CreateAttemptGuids;
    }
"""
    s = one(s, anchor, anchor + "\n" + addition, "UpdateData create metadata API")
    return one(s, "    GuidVector _f14OutOfRangeGuids;",
               "    GuidVector _f14OutOfRangeGuids;\n    GuidVector _f14CreateAttemptGuids;",
               "UpdateData non-network create metadata")

def data_cpp(s: str) -> str:
    return one(s, """    _f14OutOfRangeGuids.clear();
    _f14AttributionOverflow = false;""",
               """    _f14OutOfRangeGuids.clear();
    _f14CreateAttemptGuids.clear();
    _f14AttributionOverflow = false;""",
               "UpdateData metadata Clear")

def player(s: str) -> str:
    anchor = """            target->BuildCreateUpdateBlockForPlayer(&data, this);
            UpdateVisibilityOf_helper(this, target, visibleNow);"""
    new = anchor + """
            // Only after the ordinary Core-visible CREATE block and link.
            // This optional query defaults FALSE; no F14 consumer registers.
            // An ordinary out-of-range/GM/phase relink is NOT F14 proof.
            if (target->IsCreature() && !target->ToCreature()->IsPet() &&
                sScriptMgr->ShouldF14AttributeCreatureCreateAttemptForViewer(this, target))
                data.RememberF14CreateAttemptGUID(target->GetGUID());"""
    return one(s, anchor, new, "PlayerUpdates batched CREATE only")

def grid(s: str) -> str:
    anchor = """    if (!i_data.F14OutOfRangeAttributionOverflowed())
        for (ObjectGuid const& f14Guid : i_data.F14OutOfRangeGUIDsForAttemptOnly())
            sScriptMgr->OnF14VisibilityBatchOutOfRangeAttempt(&i_player, f14Guid);"""
    new = anchor + """
    // Metadata is scoped to the same UpdateData and exact F14-specific
    // CREATE nomination, and is ignored after EITHER-direction overflow.
    // This callback cannot prove socket queue success or a 3.3.5a client ACK.
    if (!i_data.F14OutOfRangeAttributionOverflowed())
        for (ObjectGuid const& f14Guid : i_data.F14CreateAttemptGUIDsForAttemptOnly())
            sScriptMgr->OnF14VisibilityBatchCreateAttempt(&i_player, f14Guid);"""
    return one(s, anchor, new, "GridNotifiers per-GUID batched CREATE attempt")

def mgr(s: str) -> str:
    anchor = "    void OnF14VisibilityBatchOutOfRangeAttempt(Player const* viewer, ObjectGuid const& creatureGuid);"
    addition = """
    // Default-FALSE *attribution query*; NEVER asks Core to create an object.
    // No existing script returns true, and no ledger is completed by an event.
    [[nodiscard]] bool ShouldF14AttributeCreatureCreateAttemptForViewer(
        Player const* viewer, WorldObject const* creature);
    void OnF14VisibilityBatchCreateAttempt(Player const* viewer, ObjectGuid const& creatureGuid);"""
    return one(s, anchor, anchor + "\n" + addition, "ScriptMgr.h F14 paired attempts")

def world_header(s: str) -> str:
    s = one(s, "    WORLDOBJECTHOOK_ON_F14_BATCH_OUT_OF_RANGE_ATTEMPT,",
            "    WORLDOBJECTHOOK_ON_F14_BATCH_OUT_OF_RANGE_ATTEMPT,\n"
            "    WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTRIBUTION_QUERY,\n"
            "    WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTEMPT,",
            "WorldObjectScript.h hook enum")
    anchor = """    virtual void OnF14VisibilityBatchOutOfRangeAttempt(Player const* /*viewer*/,
        ObjectGuid const& /*creatureGuid*/) { }"""
    extra = """

    // Positive attribution requires future, external exact prior-F14-hidden
    // viewer-session + creature-incarnation proof. Default FALSE.
    [[nodiscard]] virtual bool OnF14ShouldAttributeCreatureCreateAttempt(
        Player const* /*viewer*/, WorldObject const* /*creature*/) { return false; }
    virtual void OnF14VisibilityBatchCreateAttempt(Player const* /*viewer*/,
        ObjectGuid const& /*creatureGuid*/) { }"""
    return one(s, anchor, anchor + extra, "WorldObjectScript.h default-false create")

def world_cpp(s: str) -> str:
    anchor = "WorldObjectScript::WorldObjectScript(char const* name, std::vector<uint16> enabledHooks)"
    code = """bool ScriptMgr::ShouldF14AttributeCreatureCreateAttemptForViewer(
    Player const* viewer, WorldObject const* creature)
{
    ASSERT(viewer && creature);
    // This OBSERVES a normal Core CREATE only; it does NOT force visibility.
    // A future consumer must verify the exact pair's prior F14 suppression.
    CALL_ENABLED_BOOLEAN_HOOKS_WITH_DEFAULT_FALSE(WorldObjectScript,
        WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTRIBUTION_QUERY,
        script->OnF14ShouldAttributeCreatureCreateAttempt(viewer, creature));
}

void ScriptMgr::OnF14VisibilityBatchCreateAttempt(Player const* viewer, ObjectGuid const& creatureGuid)
{
    ASSERT(viewer);
    // Per-GUID SendDirectMessage call ATTEMPT only. No client receipt/render.
    CALL_ENABLED_HOOKS(WorldObjectScript,
        WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTEMPT,
        script->OnF14VisibilityBatchCreateAttempt(viewer, creatureGuid));
}

"""
    return one(s, anchor, code + anchor, "WorldObjectScript.cpp observer-only callbacks")

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_batched_guid_create_attempt.py <disposable #93 Core>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("missing disposable Core checkout")
    originals = {path: (root / path).read_text(encoding="utf-8") for path in sorted(FILES)}
    if "F14CreateAttemptGUIDsForAttemptOnly" in originals["src/server/game/Entities/Object/Updates/UpdateData.h"]:
        raise SystemExit("already patched; refusing double application")
    if "OnF14VisibilityBatchOutOfRangeAttempt" not in originals["src/server/game/Scripting/ScriptMgr.h"]:
        raise SystemExit("missing exact #93 source-attributed batch precursor")
    if "f14PositiveSuppression" not in originals["src/server/game/Entities/Player/PlayerUpdates.cpp"]:
        raise SystemExit("missing default-false #86 visibility precursor")
    edits = {
        "src/server/game/Entities/Object/Updates/UpdateData.h": data_header,
        "src/server/game/Entities/Object/Updates/UpdateData.cpp": data_cpp,
        "src/server/game/Entities/Player/PlayerUpdates.cpp": player,
        "src/server/game/Grids/Notifiers/GridNotifiers.cpp": grid,
        "src/server/game/Scripting/ScriptMgr.h": mgr,
        "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h": world_header,
        "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp": world_cpp,
    }
    # Transaction-like source validation: EVERY anchor before ANY disk write.
    changed = {name: edits[name](old) for name, old in originals.items()}
    if any(changed[name] == originals[name] for name in FILES):
        raise ValueError("a required Core path did not change")
    for name, output in changed.items():
        (root / name).write_text(output, encoding="utf-8")
    print("PASS: seven disposable Core files, bounded F14 create send-attempt only, no gameplay hide")

if __name__ == "__main__":
    raise SystemExit(main())
