#!/usr/bin/env python3
"""F14 CI-only fail-visible HUMAN VIEWER session/GM guard before positive query.

Runs ONLY after #110's verified Map::Update negative F14 Core overlay and
#107's pre-link callbacks, on a DISPOSABLE exact Grim Core checkout.
A real non-headless session and non-GM viewer are NECESSARY, not sufficient,
conditions for hypothetical future concealment. Never a positive provider.
"""
from pathlib import Path
import sys

PLAYER = "src/server/game/Entities/Player/PlayerUpdates.cpp"
PLAYER_H = "src/server/game/Entities/Player/Player.h"
SESSION_H = "src/server/game/Server/WorldSession.h"
MAP_H = "src/server/game/Maps/Map.h"

OLD_QUERY = """        GetMap()->IsWithinF14MapUpdateInvocation() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&"""
NEW_QUERY = """        GetMap()->IsWithinF14MapUpdateInvocation() &&
        // F14-specific viewer negative gate: MUST have an actual non-headless
        // session and must NOT be an active GM. Not a complete human-account
        // or Vanilla expansion entitlement proof; the final provider stays OFF.
        GetSession() && !GetSession()->IsHeadless() && !IsGameMaster() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&"""
OLD_INCLUDE = '#include "WorldStatePackets.h"\n'
NEW_INCLUDE = '#include "WorldStatePackets.h"\n#include "WorldSession.h" // F14 disposable viewer-session prequery veto\n'

PATHS = (
    ("void Player::UpdateVisibilityOf(T* target, UpdateData& data,",
     "\nvoid Player::GetInitialVisiblePackets(Unit* target)", "batched"),
    ("void Player::UpdateVisibilityOf(WorldObject* target)",
     "\nvoid Player::UpdateTriggerVisibility()", "direct"),
)

def body_of(source, begin, end, kind):
    if source.count(begin) != 1 or source.count(end) != 1:
        raise ValueError(f"{kind}: pinned visibility bounds not unique")
    return source.split(begin, 1)[1].split(end, 1)[0]

def transform(source):
    if "F14 disposable viewer-session prequery veto" in source:
        raise ValueError("already applied: refuse double patch")
    if source.count(OLD_INCLUDE) != 1:
        raise ValueError("expected exactly one pinned WorldStatePackets include")
    if source.count(OLD_QUERY) != 2:
        raise ValueError("missing exact two #110 frame-guarded F14 queries")
    if "IsHeadless()" in source or '#include "WorldSession.h"' in source:
        raise ValueError("unreviewed prior session guard or WorldSession include")

    for begin, end, kind in PATHS:
        body = body_of(source, begin, end, kind)
        if body.count(OLD_QUERY) != 1:
            raise ValueError(f"{kind}: missing F14-only framed decision")
        for token in ("bool const f14BaselineVisible", "bool const f14PositiveSuppression",
                      "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())",
                      "OnF14BeforeCreatureSuppression(this, target",
                      "if (HaveAtClient(target))"):
            if body.count(token) != 1:
                raise ValueError(f"{kind}: missing/doubled prior safety anchor {token}")

    changed = source.replace(OLD_QUERY, NEW_QUERY)
    changed = changed.replace(OLD_INCLUDE, NEW_INCLUDE, 1)
    for begin, end, kind in PATHS:
        body = body_of(changed, begin, end, kind)
        tokens = (
            "GetMap()->IsWithinF14MapUpdateInvocation()",
            "GetSession() && !GetSession()->IsHeadless() && !IsGameMaster() &&",
            "ShouldF14SuppressCreatureForViewer(this, target)",
            "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())",
            "OnF14BeforeCreatureSuppression(this, target",
            "if (HaveAtClient(target))",
        )
        if any(body.count(token) != 1 for token in tokens):
            raise ValueError(f"{kind}: duplicate/missing viewer safety gate")
        positions = [body.index(token) for token in tokens]
        if positions != sorted(positions) or len(set(positions)) != len(positions):
            raise ValueError(f"{kind}: mandatory viewer guard not before policy/link")
    if changed.count("GetSession() && !GetSession()->IsHeadless() && !IsGameMaster() &&") != 2:
        raise ValueError("viewer gate must be present in both paths")
    return changed

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_human_viewer_prequery_veto.py <disposable pinned Core root>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise ValueError("missing disposable pinned Core")
    check = (
        (PLAYER_H, "[[nodiscard]] WorldSession* GetSession() const"),
        (PLAYER_H, "[[nodiscard]] bool IsGameMaster() const"),
        (SESSION_H, "[[nodiscard]] bool IsHeadless() const"),
        (MAP_H, "[[nodiscard]] bool IsWithinF14MapUpdateInvocation() const;"),
    )
    for path, token in check:
        if (root / path).read_text(encoding="utf-8").count(token) != 1:
            raise ValueError(f"missing exact pinned declaration {path}: {token}")
    path = root / PLAYER
    original = path.read_text(encoding="utf-8")
    proposed = transform(original)  # fully validate BEFORE any write
    if original == proposed:
        raise ValueError("refuse no-op")
    path.write_text(proposed, encoding="utf-8")
    print("PASS: both F14 visibility paths reject headless/missing-session/active-GM viewers before positive F14 query; hide OFF")

if __name__ == "__main__":
    main()
