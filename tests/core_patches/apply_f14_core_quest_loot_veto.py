#!/usr/bin/env python3
"""F14 CI-only negative Core quest-loot interest veto layered after #101.

Checks actual pinned LootStore::HaveQuestLootForPlayer on creature lootid.
Never infers that a FALSE result proves all player loot, skinning, quest,
corpse, tap, group, assist or client-visibility rights. No positive F14
policy registered; original viewer callback stays default FALSE.
"""
from pathlib import Path
import sys

TARGET = "src/server/game/Entities/Player/PlayerUpdates.cpp"
BEGIN = "static bool F14ViewerHasDirectInteractionVeto("
END = "\ntemplate <class T>\nvoid Player::UpdateVisibilityOf(T* target, UpdateData& data,"
ANCHOR = """    CreatureTemplate const* const questCreature = creature->GetCreatureTemplate();
    if (!questCreature)
        return true;
"""
ADD = """    CreatureTemplate const* const questCreature = creature->GetCreatureTemplate();
    if (!questCreature)
        return true;

    // Core's own per-viewer quest-loot table, including quest references
    // and ordinary loot conditions. Do not guess item-to-mob ownership.
    // This is a veto ONLY. FALSE cannot establish complete loot rights.
    if (questCreature->lootid &&
        LootTemplates_Creature.HaveQuestLootForPlayer(questCreature->lootid, viewer))
        return true;
"""

def transform(source: str) -> str:
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("expected one exact #101 PlayerUpdates helper")
    if "HaveQuestLootForPlayer(" in source:
        raise ValueError("refusing duplicate quest-loot veto")
    prefix, rest = source.split(BEGIN, 1)
    body, suffix = rest.split(END, 1)
    if body.count(ANCHOR) != 1:
        raise ValueError("exact #101 template-guard anchor missing")
    for prior in ("viewer->GetQuestSlotQuestId(f14Slot)",
                  "questCreature->KillCredit[f14Credit]",
                  "viewer->GetGroup()", "creature->GetAppliedAuras()",
                  "viewer->GetPet()", "viewer->GetCharm()",
                  "viewer->GetGuardianPet()",
                  "creature->GetLootRecipientGroupGUID()"):
        if prior not in body:
            raise ValueError("missing immutable #99/#100/#101 veto: " + prior)
    for core in ("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&",
                 "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())"):
        if source.count(core) != 2:
            raise ValueError("Core direct/batched default-FALSE call sites changed")

    updated = prefix + BEGIN + body.replace(ANCHOR, ADD, 1) + END + suffix
    if '#include "LootMgr.h"' not in updated:
        header = '#include "ObjectMgr.h"\n'
        if updated.count(header) != 1:
            raise ValueError("missing pinned #101 ObjectMgr include")
        updated = updated.replace(header, '#include "LootMgr.h"\n' + header, 1)
    if updated.count("HaveQuestLootForPlayer(") != 1:
        raise ValueError("expected exactly one negative quest-loot API call")
    return updated

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_core_quest_loot_veto.py <disposable #101 Core>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("disposable pinned Core checkout missing")
    path = root / TARGET
    before = path.read_text(encoding="utf-8")
    after = transform(before)  # no writes until EVERY source assertion passes
    if before == after:
        raise ValueError("no change produced")
    path.write_text(after, encoding="utf-8")
    print("PASS: pinned Core quest loot per-player negative-only veto, default FALSE F14")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
