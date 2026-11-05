#!/usr/bin/env python3
"""F14 Core CI-only child of #99: group/active-aura negative-only veto.

Layer after the exact #99 direct-interest veto in a DISPOSABLE pinned Grim Core.
This cannot supply full group/quest/loot/DoT rights or client reappearance proof.
Never registers a positive hide policy, clears a ledger, or sends a packet.

The inspected map-thread aura collection is bounded to 64 entries; >64,
null applications, unknown casters or player-cast auras fail VISIBLE.
All player-cast auras (including bot-cast) conservatively veto hiding.
"""
from pathlib import Path
import sys

PATH = "src/server/game/Entities/Player/PlayerUpdates.cpp"
HELPER_BEGIN = "static bool F14ViewerHasDirectInteractionVeto("
HELPER_END = "\ntemplate <class T>\nvoid Player::UpdateVisibilityOf(T* target, UpdateData& data,"

ANCHOR = "    return false;\n}\n"
EXTENSION = """    // Charmed units and guardians can be a real viewer's combat agents,
    // even when the named primary pet has no direct current interaction.
    // A missing reliable complete owner/assist census is NOT inferred safe.
    if (Unit const* controlled = viewer->GetCharm())
        if (controlled->GetTarget() == creatureGuid ||
            controlled->GetVictim() == creature ||
            creature->GetVictim() == controlled)
            return true;

    if (Guardian const* guardian = viewer->GetGuardianPet())
        if (guardian->GetTarget() == creatureGuid ||
            guardian->GetVictim() == creature ||
            creature->GetVictim() == guardian)
            return true;

    // Without a complete audited group quest/loot/kill-credit census,
    // grouped real viewers must always retain ordinary creature visibility.
    if (viewer->GetGroup())
        return true;

    // Auras can represent DoTs, healing threat or other owner interaction.
    // Scan ONLY a bounded live Core container on this map visibility pass.
    // Unknown provenance fails visible. Includes bot-cast player auras
    // intentionally, rather than guessing who has real-player rights.
    auto const& f14Auras = creature->GetAppliedAuras();
    if (f14Auras.size() > 64)
        return true;
    for (auto const& f14Entry : f14Auras)
    {
        AuraApplication const* const app = f14Entry.second;
        if (!app || !app->GetBase())
            return true;
        ObjectGuid const caster = app->GetBase()->GetCasterGUID();
        if (!caster || caster.IsPlayer())
            return true;
    }

    return false;
}
"""

def transform(source: str) -> str:
    if source.count(HELPER_BEGIN) != 1 or source.count(HELPER_END) != 1:
        raise ValueError("missing exact #99 Core helper/batched path")
    if "F14 group-and-aura veto" in source or "f14Auras" in source:
        raise ValueError("already patched")
    head, rest = source.split(HELPER_BEGIN, 1)
    body, tail = rest.split(HELPER_END, 1)
    if body.count(ANCHOR) != 1:
        raise ValueError(f"expected one exact #99 helper return anchor, found {body.count(ANCHOR)}")
    if "creature->GetLootRecipientGroupGUID()" not in body:
        raise ValueError("missing #99 original group tap veto")
    if "if (Pet const* pet = viewer->GetPet())" not in body:
        raise ValueError("missing #99 original direct pet veto")
    if '#include "SpellAuraEffects.h"' not in source:
        raise ValueError("pinned Core must include SpellAuraEffects.h (AuraApplication)")
    if "sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&" not in source:
        raise ValueError("missing default FALSE F14 short-circuit policy")
    updated = head + HELPER_BEGIN + body.replace(ANCHOR, EXTENSION, 1) + HELPER_END + tail
    # Only one helper changes. Both real Core per-viewer call sites remain
    # guarded behind the existing F14-only default FALSE query.
    assert updated.count("!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())") == 2
    assert updated.count("sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target) &&") == 2
    return updated

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_core_group_aura_veto.py <disposable #99 Core>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("missing disposable Core")
    path = root / PATH
    original = path.read_text(encoding="utf-8")
    changed = transform(original)  # Validate every anchor before ANY file write.
    if changed == original:
        raise ValueError("no change produced")
    path.write_text(changed, encoding="utf-8")
    print("PASS: bounded aura and group negative-only F14 Core veto, positive concealment OFF")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
