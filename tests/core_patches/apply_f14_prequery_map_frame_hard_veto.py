#!/usr/bin/env python3
"""DISPOSABLE pinned-Core-only F14 pre-query map-frame negative veto.

Must apply after #107 F14 pre-link and #108 Map::Update frame overlays.
In both Player visibility paths, ensure the proposed F14-only positive
suppression callback is NEVER queried unless viewer/target are in the
same current Map::Update invocation. The baseline Core visibility decision
is unchanged. The positive F14 provider remains absent / default FALSE.

This is a real negative gate, NOT exclusive-map ownership proof, complete
human rights, prior hidden-pair evidence or 3.3.5a client restoration.
"""
from pathlib import Path
import sys

TARGET = "src/server/game/Entities/Player/PlayerUpdates.cpp"
MAP_H = "src/server/game/Maps/Map.h"
MAP_CPP = "src/server/game/Maps/Map.cpp"

ORIGINAL = """        !target->ToCreature()->IsPet() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&
"""
GUARDED = """        !target->ToCreature()->IsPet() &&
        // F14-only mandatory negative authorization; outside this exact
        // synchronous Map::Update call, fail VISIBLE before policy query.
        // This is NOT exclusive map ownership or all-player-rights proof.
        IsInWorld() && target->IsInWorld() && GetMap() &&
        GetMap() == target->GetMap() &&
        GetMap()->IsWithinF14MapUpdateInvocation() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&
"""
INCLUDE_OLD = '#include "Language.h"\n'
INCLUDE_NEW = '#include "Language.h"\n#include "Map.h" // F14 isolated disposable pre-query map-frame veto\n'

PATHS = (
    ("void Player::UpdateVisibilityOf(T* target, UpdateData& data,",
     "\nvoid Player::GetInitialVisiblePackets(Unit* target)", "batched"),
    ("void Player::UpdateVisibilityOf(WorldObject* target)",
     "\nvoid Player::UpdateTriggerVisibility()", "direct"),
)

def one(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise ValueError(f"{label}: expected one exact anchor, got {count}")
    return text.replace(old, new, 1)

def function_bodies(source):
    bodies = []
    for begin, end, kind in PATHS:
        before, found, tail = source.partition(begin)
        if not found or before.count(begin):
            raise ValueError(f"{kind}: missing or duplicate function start")
        body, found_end, after = tail.partition(end)
        if not found_end:
            raise ValueError(f"{kind}: missing function end")
        bodies.append((kind, body))
    return bodies

def transform(source):
    if "IsWithinF14MapUpdateInvocation()" in source:
        raise ValueError("F14 frame hard veto already present; reject repeat")
    if INCLUDE_NEW in source or source.count(INCLUDE_OLD) != 1:
        raise ValueError("unexpected Map include or pinned source")
    if source.count(ORIGINAL) != 2:
        raise ValueError("expected exactly two #99-#107 positive F14 queries")
    for kind, body in function_bodies(source):
        if body.count(ORIGINAL) != 1:
            raise ValueError(f"{kind}: F14 query is missing/duplicated")
        for key in ("bool const f14BaselineVisible", "bool const f14PositiveSuppression",
                    "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())",
                    "OnF14BeforeCreatureSuppression(this, target",
                    "OnF14CreatureSuppressionDecision(this, target",
                    "if (HaveAtClient(target))"):
            if body.count(key) != 1:
                raise ValueError(f"{kind}: missing/duplicated source contract {key}")
        if body.index(ORIGINAL) > body.index("OnF14BeforeCreatureSuppression(this, target"):
            raise ValueError(f"{kind}: query not before F14 pre-link callback")
    changed = source.replace(ORIGINAL, GUARDED)
    changed = one(changed, INCLUDE_OLD, INCLUDE_NEW, "pinned Map.h include")
    for kind, body in function_bodies(changed):
        a = body.index("IsWithinF14MapUpdateInvocation()")
        b = body.index("ShouldF14SuppressCreatureForViewer(this, target)")
        c = body.index("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())")
        d = body.index("OnF14BeforeCreatureSuppression(this, target")
        e = body.index("if (HaveAtClient(target))")
        if not a < b < c < d < e:
            raise ValueError(f"{kind}: F14 gate must precede policy query, rights veto and link mutation")
        if body.count("IsWithinF14MapUpdateInvocation()") != 1:
            raise ValueError(f"{kind}: duplicate frame gate")
    if changed.count("GetMap()->IsWithinF14MapUpdateInvocation() &&") != 2:
        raise ValueError("both F14 paths must carry the real pre-query veto")
    return changed

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_prequery_map_frame_hard_veto.py <disposable pinned Core root>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise ValueError("missing disposable Core root")
    map_header = (root / MAP_H).read_text(encoding="utf-8")
    map_cpp = (root / MAP_CPP).read_text(encoding="utf-8")
    if map_header.count("[[nodiscard]] bool IsWithinF14MapUpdateInvocation() const;") != 1:
        raise ValueError("missing exactly one #108 Core public frame API")
    if map_cpp.count("F14ScopedMapUpdateInvocation f14ScopedInvocation(this);") != 1:
        raise ValueError("missing exactly one #108 real Map::Update RAII frame")
    if map_cpp.count("return f14CurrentMapUpdateInvocation == this;") != 1:
        raise ValueError("missing #108 exact-Map comparison")
    path = root / TARGET
    source = path.read_text(encoding="utf-8")
    changed = transform(source)
    if changed == source:
        raise ValueError("refuse empty overlay")
    # SINGLE target path, entire validated replacement, no other Core files.
    path.write_text(changed, encoding="utf-8")
    print("PASS: F14-only Core positive policy query fails VISIBLE off-map-frame in BOTH visibility paths; F14 remains OFF")

if __name__ == "__main__":
    main()
