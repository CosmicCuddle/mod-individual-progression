#!/usr/bin/env python3
"""F14-only negative alive/death-state gate BEFORE any positive policy query.

Disposable pinned Grim Core only, after exact #110 map-frame and #111
human-viewer negative gates. This does NOT restore previously hidden pairs,
reschedule Corpse/Dead ticks, prove client presentation or enable hiding.
"""
from pathlib import Path
import sys

PLAYER = "src/server/game/Entities/Player/PlayerUpdates.cpp"
UNIT_H = "src/server/game/Entities/Unit/Unit.h"

OLD = """        GetSession() && !GetSession()->IsHeadless() && !IsGameMaster() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&"""
NEW = """        GetSession() && !GetSession()->IsHeadless() && !IsGameMaster() &&
        // F14-only hard negative: never begin NEW concealment for JustDied,
        // Corpse, Dead or JustRespawned. This does not clear a prior hide.
        target->ToCreature()->IsAlive() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&"""

PATHS = (
    ("void Player::UpdateVisibilityOf(T* target, UpdateData& data,",
     "\nvoid Player::GetInitialVisiblePackets(Unit* target)", "batched"),
    ("void Player::UpdateVisibilityOf(WorldObject* target)",
     "\nvoid Player::UpdateTriggerVisibility()", "direct"),
)

def function_body(source, begin, end, kind):
    if source.count(begin) != 1 or source.count(end) != 1:
        raise ValueError(f"{kind}: pinned Core function anchors not unique")
    return source.split(begin, 1)[1].split(end, 1)[0]

def transform(source):
    if "F14-only hard negative: never begin NEW concealment" in source:
        raise ValueError("already applied; refuse reapplication")
    if source.count(OLD) != 2:
        raise ValueError("expected two exactly verified #111 viewer policy queries")
    if source.count("target->ToCreature()->IsAlive() &&") != 0:
        raise ValueError("unrecognised prior F14 life check")
    for begin, end, kind in PATHS:
        body = function_body(source, begin, end, kind)
        for token in (OLD, "bool const f14BaselineVisible",
                      "bool const f14PositiveSuppression",
                      "GetMap()->IsWithinF14MapUpdateInvocation()",
                      "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())",
                      "OnF14BeforeCreatureSuppression(this, target",
                      "OnF14CreatureSuppressionDecision(this, target",
                      "if (HaveAtClient(target))"):
            if body.count(token) != 1:
                raise ValueError(f"{kind}: missing/duplicate original safety anchor {token}")
        if not (body.index("GetMap()->IsWithinF14MapUpdateInvocation()") <
                body.index("GetSession() && !GetSession()->IsHeadless()") <
                body.index("ShouldF14SuppressCreatureForViewer(this, target)") <
                body.index("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())") <
                body.index("OnF14BeforeCreatureSuppression(this, target") <
                body.index("if (HaveAtClient(target))")):
            raise ValueError(f"{kind}: inherited F14 query or callback ordering changed")
    changed = source.replace(OLD, NEW)
    for begin, end, kind in PATHS:
        body = function_body(changed, begin, end, kind)
        order = (
            "GetMap()->IsWithinF14MapUpdateInvocation()",
            "GetSession() && !GetSession()->IsHeadless()",
            "target->ToCreature()->IsAlive()",
            "ShouldF14SuppressCreatureForViewer(this, target)",
            "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())",
            "OnF14BeforeCreatureSuppression(this, target",
            "if (HaveAtClient(target))",
        )
        if any(body.count(token) != 1 for token in order):
            raise ValueError(f"{kind}: duplicated or missing F14 corpse guard")
        indexes = [body.index(token) for token in order]
        if indexes != sorted(indexes) or len(set(indexes)) != len(indexes):
            raise ValueError(f"{kind}: life check must precede positive policy and link")
    if changed.count("target->ToCreature()->IsAlive() &&") != 2:
        raise ValueError("one required corpse-negative veto per visibility path")
    return changed

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_alive_prequery_corpse_veto.py <disposable Core root>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise ValueError("disposable root missing")
    unit = (root / UNIT_H).read_text(encoding="utf-8")
    required = "[[nodiscard]] bool IsAlive() const { return (m_deathState == DeathState::Alive); };"
    if unit.count(required) != 1:
        raise ValueError("pinned Unit::IsAlive() is not the exact Alive-only check")
    path = root / PLAYER
    original = path.read_text(encoding="utf-8")
    changed = transform(original)  # validate the whole target before touching disk
    if changed == original:
        raise ValueError("refuse no-op")
    path.write_text(changed, encoding="utf-8")
    print("PASS: F14 policy query cannot run on JustDied/Corpse/Dead/JustRespawned in either visibility path, positive policy remains OFF")

if __name__ == "__main__":
    main()
