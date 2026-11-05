#!/usr/bin/env python3
"""CI-only exact Core prototype AFTER #86: F14-specific batched out-of-range send-attempt attribution.

No concealment consumer. Add per-GUID metadata alongside UpdateData, NEVER
inside packet bytes; suppress evidence rather than guessing after overflow.
Server SendDirectMessage call completion is NOT a socket receipt or client ACK.
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

def one(s: str, before: str, after: str, label: str) -> str:
    n = s.count(before)
    if n != 1:
        raise ValueError(f"{label}: expected 1 exact anchor, found {n}: {before[:90]!r}")
    return s.replace(before, after, 1)

def update_data_h(s: str) -> str:
    anchor = "    void AddOutOfRangeGUID(ObjectGuid guid);"
    addition = """    // CI-only, NON-NETWORK side channel: exact F14-attributed creature
    // out-of-range blocks. Ordinary phasing/distance out-of-range is excluded.
    // If the list overflows, discard ALL batch evidence; never assert send.
    void RememberF14OutOfRangeGUID(ObjectGuid guid)
    {
        if (_f14AttributionOverflow)
            return;
        if (_f14OutOfRangeGuids.size() >= 512)
        {
            _f14OutOfRangeGuids.clear();
            _f14AttributionOverflow = true;
            return;
        }
        _f14OutOfRangeGuids.push_back(guid);
    }
    [[nodiscard]] GuidVector const& F14OutOfRangeGUIDsForAttemptOnly() const { return _f14OutOfRangeGuids; }
    [[nodiscard]] bool F14OutOfRangeAttributionOverflowed() const { return _f14AttributionOverflow; }
"""
    s = one(s, anchor, anchor + "\n" + addition, "UpdateData.h methods")
    return one(s, "    GuidVector m_outOfRangeGUIDs;",
               "    GuidVector m_outOfRangeGUIDs;\n"
               "    GuidVector _f14OutOfRangeGuids;\n"
               "    bool _f14AttributionOverflow = false;", "UpdateData.h members")

def update_data_cpp(s: str) -> str:
    return one(s, "    m_outOfRangeGUIDs.clear();\n    m_blockCount = 0;",
               "    m_outOfRangeGUIDs.clear();\n"
               "    // Metadata must never survive a reused UpdateData batch.\n"
               "    _f14OutOfRangeGuids.clear();\n"
               "    _f14AttributionOverflow = false;\n"
               "    m_blockCount = 0;", "UpdateData.cpp Clear")

def player_updates(s: str) -> str:
    anchor = "            target->BuildOutOfRangeUpdateBlock(&data);\n            GetObjectVisibilityContainer().UnlinkWorldObjectVisibility(target);"
    replacement = """            target->BuildOutOfRangeUpdateBlock(&data);
            // The F14 decision was made on a stock-Core-visible nonpet NPC.
            // Preserve that exact source attribution with the same UpdateData
            // that already contains the out-of-range GUID; NOT packet receipt.
            if (f14PositiveSuppression)
                data.RememberF14OutOfRangeGUID(target->GetGUID());
            GetObjectVisibilityContainer().UnlinkWorldObjectVisibility(target);"""
    return one(s, anchor, replacement, "PlayerUpdates.cpp batched-only F14")

def grid(s: str) -> str:
    s = one(s, '#include "ObjectAccessor.h"', '#include "ObjectAccessor.h"\n#include "ScriptMgr.h"',
            "GridNotifiers.cpp include")
    anchor = "    i_data.BuildPacket(packet);\n    i_player.SendDirectMessage(&packet);"
    out = """    i_data.BuildPacket(packet);
    i_player.SendDirectMessage(&packet);
    // Exact F14-origin out-of-range GUIDs ONLY; normal distance/phase
    // removals have no metadata. This is a SendDirectMessage CALL ATTEMPT,
    // not socket send success, per-GUID packet receipt or client rendering.
    if (!i_data.F14OutOfRangeAttributionOverflowed())
        for (ObjectGuid const& f14Guid : i_data.F14OutOfRangeGUIDsForAttemptOnly())
            sScriptMgr->OnF14VisibilityBatchOutOfRangeAttempt(&i_player, f14Guid);"""
    return one(s, anchor, out, "GridNotifiers.cpp send attempt")

def mgr(s: str) -> str:
    old = "    void OnF14CreatureSuppressionDecision(Player const* viewer, WorldObject const* creature,\n                                         bool wasLinked, bool batched);"
    return one(s, old, old + """
    // Source-attributed batched out-of-range GUID reached the Core's
    // SendDirectMessage call. No callback may infer client confirmation.
    void OnF14VisibilityBatchOutOfRangeAttempt(Player const* viewer, ObjectGuid const& creatureGuid);""",
               "ScriptMgr.h previously patched #86")

def world_h(s: str) -> str:
    s = one(s, "    WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED,",
            "    WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED,\n"
            "    WORLDOBJECTHOOK_ON_F14_BATCH_OUT_OF_RANGE_ATTEMPT,", "WorldObjectScript.h hook enum")
    old = "    virtual void OnF14CreatureSuppressionDecision(Player const* /*viewer*/,\n        WorldObject const* /*creature*/, bool /*wasLinked*/, bool /*batched*/) { }"
    return one(s, old, old + """

    virtual void OnF14VisibilityBatchOutOfRangeAttempt(Player const* /*viewer*/,
        ObjectGuid const& /*creatureGuid*/) { }""", "WorldObjectScript.h opt-in observer")

def world_cpp(s: str) -> str:
    anchor = "WorldObjectScript::WorldObjectScript(char const* name, std::vector<uint16> enabledHooks)"
    event = """void ScriptMgr::OnF14VisibilityBatchOutOfRangeAttempt(Player const* viewer, ObjectGuid const& creatureGuid)
{
    ASSERT(viewer);
    // F14-only origin; no registered listener in the pinned combined module.
    // No assertion of socket transport/receipt/rendering is possible here.
    CALL_ENABLED_HOOKS(WorldObjectScript,
        WORLDOBJECTHOOK_ON_F14_BATCH_OUT_OF_RANGE_ATTEMPT,
        script->OnF14VisibilityBatchOutOfRangeAttempt(viewer, creatureGuid));
}

"""
    return one(s, anchor, event + anchor, "WorldObjectScript.cpp event")

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_batched_guid_send_attempt.py <disposable #86 Core>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("missing disposable Core checkout")
    sources = {name: (root/name).read_text(encoding="utf-8") for name in sorted(FILES)}
    if "ShouldF14SuppressCreatureForViewer" not in sources["src/server/game/Scripting/ScriptMgr.h"]:
        raise SystemExit("MISSING default-false #86 prerequisite; refusing unsafe patch")
    if "f14PositiveSuppression" not in sources["src/server/game/Entities/Player/PlayerUpdates.cpp"]:
        raise SystemExit("MISSING #86 source-attributed viewer decisions; refusing patch")
    edits = {
        "src/server/game/Entities/Object/Updates/UpdateData.h": update_data_h,
        "src/server/game/Entities/Object/Updates/UpdateData.cpp": update_data_cpp,
        "src/server/game/Entities/Player/PlayerUpdates.cpp": player_updates,
        "src/server/game/Grids/Notifiers/GridNotifiers.cpp": grid,
        "src/server/game/Scripting/ScriptMgr.h": mgr,
        "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h": world_h,
        "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp": world_cpp,
    }
    # Validate ALL exact source anchors before writing ANY file.
    changed = {name: edits[name](src) for name, src in sources.items()}
    for name, new_source in changed.items():
        if new_source == sources[name]:
            raise ValueError(f"missing expected code transformation in {name}")
    for name, new_source in changed.items():
        (root/name).write_text(new_source, encoding="utf-8")
    print("PASS: 7 exact Core C++ sources patched in ephemeral checkout; no active F14 hide")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
