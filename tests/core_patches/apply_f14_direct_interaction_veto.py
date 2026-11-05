#!/usr/bin/env python3
"""F14 pinned-Core safety prototype, AFTER exact #86/#93/#95 visibility patches.

Adds a *negative* direct-interaction veto to BOTH player visibility paths.
No script is added, no visibility policy becomes positive, no packet changes.
Requires a FUTURE positive F14-specific callback before checking these facts.
The helper intentionally cannot claim complete human quest/group/DoT rights.
Applies only to a disposable GitHub Actions Core checkout.
"""
from pathlib import Path
import sys

TARGET = "src/server/game/Entities/Player/PlayerUpdates.cpp"

HELPER = """
// F14 negative-only viewer veto. This is NOT the full human-rights proof.
// Consulted only AFTER a future explicit F14 concealment provider returns
// TRUE; the provider is currently absent and the default is always FALSE.
// Read only synchronous Core values on this player's owning visibility pass.
static bool F14ViewerHasDirectInteractionVeto(Player const* viewer, Creature const* creature)
{
    if (!viewer || !creature ||
        !viewer->IsInWorld() || !creature->IsInWorld() ||
        viewer->GetMap() != creature->GetMap() ||
        viewer->IsGameMaster() || !creature->IsAlive())
        return true;

    ObjectGuid const creatureGuid = creature->GetGUID();
    if (viewer->GetTarget() == creatureGuid ||
        viewer->GetVictim() == creature ||
        creature->GetVictim() == viewer)
        return true;

    // Group tap can carry shared loot/quest rights; never hide under a
    // merely local inspection. Direct viewer loot ownership also vetoes.
    if (creature->GetLootRecipientGroupGUID() != 0 ||
        creature->GetLootRecipientGUID() == viewer->GetGUID())
        return true;

    // Player-owned pet selection, damage and victim state are independent
    // of the player's own selected target. Missing/other controlled units,
    // pending spells, DoTs and group rights still require OTHER vetoes.
    if (Pet const* pet = viewer->GetPet())
        if (pet->GetTarget() == creatureGuid ||
            pet->GetVictim() == creature ||
            creature->GetVictim() == pet)
            return true;

    return false;
}

"""

OLD = "        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target);"
NEW = """        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&
        !F14ViewerHasDirectInteractionVeto(this, target->ToCreature());"""

INSERT_BEFORE = "template <class T>\nvoid Player::UpdateVisibilityOf(T* target, UpdateData& data,"

def transform(s: str) -> str:
    if '#include "Pet.h"' not in s:
        raise ValueError("Pinned Core must already include Pet.h")
    if "static bool F14ViewerHasDirectInteractionVeto(" in s:
        raise ValueError("Refusing repeat application")
    if s.count(INSERT_BEFORE) != 1:
        raise ValueError("Unexpected pinned Core visibility template")
    if s.count(OLD) != 2:
        raise ValueError(f"Expected exactly two #86 explicit F14 query calls, got {s.count(OLD)}")

    # All anchors are validated before constructing/writing any change.
    changed = s.replace(INSERT_BEFORE, HELPER + INSERT_BEFORE, 1).replace(OLD, NEW)
    direct = changed.split("void Player::UpdateVisibilityOf(WorldObject* target)", 1)[1].split(
        "void Player::UpdateTriggerVisibility()", 1)[0]
    batch = changed.split("void Player::UpdateVisibilityOf(T* target, UpdateData& data,", 1)[1].split(
        "void Player::GetInitialVisiblePackets(Unit* target)", 1)[0]
    for name, part in (("direct", direct), ("batched", batch)):
        if part.count("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())") != 1:
            raise ValueError(f"{name}: missing or duplicate direct-interaction veto")
        if part.count("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target)") != 1:
            raise ValueError(f"{name}: F14 source hook was lost")
        if part.index("ShouldF14SuppressCreatureForViewer(this, target)") > part.index(
                "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())"):
            raise ValueError(f"{name}: veto must execute only following positive F14")
    return changed

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_direct_interaction_veto.py <disposable #95 Core>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("missing disposable pinned Core")
    source = (root / TARGET).read_text(encoding="utf-8")
    if "OnF14VisibilityBatchCreateAttempt" not in (root / "src/server/game/Scripting/ScriptMgr.h").read_text(encoding="utf-8"):
        raise SystemExit("missing exact #95 Core CREATE provenance prerequisite")
    if "RememberF14OutOfRangeGUID" not in source:
        raise SystemExit("missing exact #93 Core OUT provenance prerequisite")
    changed = transform(source)
    if changed == source:
        raise ValueError("No source transformation made")
    (root / TARGET).write_text(changed, encoding="utf-8")
    print("PASS: exact two Core Player visibility paths have negative-only direct viewer interaction veto, F14 OFF")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
