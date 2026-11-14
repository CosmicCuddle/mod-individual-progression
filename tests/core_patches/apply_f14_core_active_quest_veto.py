#!/usr/bin/env python3
"""CI-only F14 negative veto for active human NPC/kill-credit quest objectives.

Apply on disposable Grim Core after #99 and #100 Core prototypes. Does NOT
claim all quest-drop, gossip, assistance, corpse or group rights. It is only a
negative-only keep-visible check, behind the original default-FALSE F14 hook.
No positive listener, packet change or prior-hidden ledger completion.
"""
from pathlib import Path
import sys

TARGET = "src/server/game/Entities/Player/PlayerUpdates.cpp"
BEGIN = "static bool F14ViewerHasDirectInteractionVeto("
END = "\ntemplate <class T>\nvoid Player::UpdateVisibilityOf(T* target, UpdateData& data,"
TAIL = "    return false;\n}\n"
ADD = """    // Real Core quest log, not a guessed eligibility counter:
    // only current, incomplete player quests, at most MAX_QUEST_LOG_SIZE.
    // An unfinished quest requiring this NPC OR its additional KillCredit
    // must keep that creature visible even if a bot attacks it.
    // Missing template/candidate provenance must fail visible.
    CreatureTemplate const* const questCreature = creature->GetCreatureTemplate();
    if (!questCreature)
        return true;
    for (uint16 f14Slot = 0; f14Slot < MAX_QUEST_LOG_SIZE; ++f14Slot)
    {
        uint32 const f14QuestId = viewer->GetQuestSlotQuestId(f14Slot);
        if (!f14QuestId || viewer->GetQuestStatus(f14QuestId) != QUEST_STATUS_INCOMPLETE)
            continue;

        Quest const* const f14Quest = sObjectMgr->GetQuestTemplate(f14QuestId);
        if (!f14Quest)
            return true; // Cannot prove missing quest data safe to hide.

        for (uint8 f14Objective = 0; f14Objective < QUEST_OBJECTIVES_COUNT; ++f14Objective)
        {
            int32 const required = f14Quest->RequiredNpcOrGo[f14Objective];
            if (required <= 0 || !f14Quest->RequiredNpcOrGoCount[f14Objective])
                continue;
            uint32 const requiredCreature = uint32(required);
            if (requiredCreature == creature->GetEntry())
                return true;
            for (uint8 f14Credit = 0; f14Credit < MAX_KILL_CREDIT; ++f14Credit)
                if (questCreature->KillCredit[f14Credit] &&
                    requiredCreature == questCreature->KillCredit[f14Credit])
                    return true;
        }
    }

    return false;
}
"""

def single(text: str, anchor: str, value: str, label: str) -> str:
    if text.count(anchor) != 1:
        raise ValueError(f"{label}: expected one anchor, found {text.count(anchor)}")
    return text.replace(anchor, value, 1)

def transform(source: str) -> str:
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("expected exactly one prior #100 Core helper")
    if "f14QuestId" in source:
        raise ValueError("refusing to reapply quest overlay")
    if "#include \"Pet.h\"" not in source:
        raise ValueError("missing pinned Pet.h include")
    prefix, rest = source.split(BEGIN, 1)
    helper, suffix = rest.split(END, 1)
    if helper.count(TAIL) != 1:
        raise ValueError("missing exact previous helper tail")
    for token in ("viewer->GetGroup()", "creature->GetAppliedAuras()",
                  "viewer->GetCharm()", "viewer->GetGuardianPet()",
                  "creature->GetLootRecipientGroupGUID()"):
        if token not in helper:
            raise ValueError("missing prior #99/#100 negative guard " + token)
    for token in ("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&",
                  "!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())"):
        if source.count(token) != 2:
            raise ValueError("missing both original default-FALSE F14 call sites")

    # Construct fully in memory; write only once every expected anchor passed.
    updated = prefix + BEGIN + helper.replace(TAIL, ADD, 1) + END + suffix
    if '#include "ObjectMgr.h"' not in updated:
        updated = single(updated, '#include "Pet.h"\n',
                         '#include "ObjectMgr.h"\n#include "Pet.h"\n', "ObjectMgr include")
    if '#include "QuestDef.h"' not in updated:
        updated = single(updated, '#include "Player.h"\n',
                         '#include "Player.h"\n#include "QuestDef.h"\n', "QuestDef include")
    if updated.count("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&") != 2:
        raise ValueError("F14 source-specific default FALSE hook changed")
    if updated.count("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())") != 2:
        raise ValueError("negative guard removed from a per-viewer path")
    return updated

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_core_active_quest_veto.py <disposable #100 Core>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("missing disposable pinned Core tree")
    header = root / "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h"
    if "OnF14ShouldSuppressCreatureForViewer" not in header.read_text(encoding="utf-8"):
        raise SystemExit("missing original default-FALSE F14 query patch")
    target = root / TARGET
    before = target.read_text(encoding="utf-8")
    after = transform(before)
    if after == before:
        raise ValueError("no source changed")
    target.write_text(after, encoding="utf-8")
    print("PASS: active NPC/kill-credit quest negative-only veto layered after #100 in disposable Core")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
