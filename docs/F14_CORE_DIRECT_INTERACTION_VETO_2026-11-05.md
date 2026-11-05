# F14 pinned Grim Core — negative-only direct viewer interaction veto

**Owner:** CosmicCuddle
**Developer:** CS01 Maintenance, covering active development while Claude is on an owner-confirmed break
**Date:** 5 November 2026
**Status:** isolated development-only source prototype, default OFF, no host installation

## Exact target and implementation

This new Core safety check is carried as `tests/core_patches/apply_f14_direct_interaction_veto.py` on the IP source branch. In a **disposable GitHub Actions checkout only**, apply it **after** exact pinned Core F14 #86 / #93 / #95 prototypes on Grim Core `0d754d1921261dfd939f91060ebc022eae280f71`.

The patch changes **one pinned Core C++ file**, `src/server/game/Entities/Player/PlayerUpdates.cpp`, without changing packet bytes, configs, SQL, DBC, client files or any existing IP/Naxx/Services/PB runtime code. Both existing per-viewer visibility methods are changed from:

`sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target)`

to a **short-circuited conjunction** with a local `!F14ViewerHasDirectInteractionVeto(this, target->ToCreature())`. That local check runs **ONLY after the script callback has returned TRUE**. No callback currently returns TRUE, so there is **no active concealment** or extra pet lookup in the default-FALSE visibility path.

## Concrete Core interaction vetoes

The additive helper returns **TRUE (keep creature visible)** when:
- The viewer or creature is outside the active world / their map pointers differ; the viewer is a GM; or the creature is dead.
- The viewer currently selects the creature, attacks it, or is the creature's current victim.
- The creature has a group loot recipient (all groups fail visible), or the viewer is the current loot recipient.
- The viewer's current pet selects or attacks the creature, or the creature is attacking the pet.

It obtains these directly from already-present pinned Core `Player`, `Unit`, `Pet`, `Creature` APIs (`GetTarget`, `GetVictim`, `GetPet`, `GetLootRecipientGUID`, `GetLootRecipientGroupGUID`). The helper neither mutates those values nor changes creature ownership, rewards, threat, combat, script registration or packet construction.

## Strict limitations

**This is not an authoritative full human-rights source.** Missing evidence about observer group quest/loot/kill credit, DoT, queued spell, assists, guardians/totems/other controlled units, ongoing aura and invisibility/interaction range still **blocks any positive F14 policy**. All positive policies remain absent and all experimental F14 and paid options OFF.

A hide/reappear event has no 3.3.5a object-delivery ACK. Core #93/#95 per-GUID callback means **SendDirectMessage was called**, not that the client saw the object, was targetable or restored. No prior-hidden ledger is cleared.

## Tests / rollback

- Python source contract and pinned patched-Core tests prove exact two visibility-path insertions, short-circuit ordering, negative-only direct interests, unmodified client packet path and no positive IP listener.
- Native disposable Worldserver compile against original #92 IP source + exact #95 Core observers + #97/#98 pure model, historical Playerbots, Naxx #292, Character Services #16 is required before claiming compilation compatibility.
- **Do not claim exact-head CI PASS until GitHub Actions finishes.** A successful compile would not be a worldserver start or client game test.
- Rollback: close this DRAFT and discard the isolated branch. This does not affect live service because nothing is deployed.

CosmicCuddle approval, complete human pair-rights authority, genuine client restoration, host inventory, backups and supervised full combined testing remain mandatory before enabling F14.
