# F14 two pinned corpse removal modes — original restore-obligation regression

**Owner:** CosmicCuddle. **Maintainer:** Server-Side-Maintenance CS01-IP. **6 November 2026.**
**Status:** isolated SOURCE/PURE-model DRAFT; no Core/IP runtime change, positive hide, installation or real 3.3.5a client test.

## Confirmed pinned Core lifecycle boundaries

The exact source in [`Grimfeather/azerothcore-wotlk@0d754d.../Creature.cpp`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Entities/Creature/Creature.cpp) contains two different branches within `Creature::RemoveCorpse(bool setSpawnTime, bool skipVisibility)`:

1. **Respawn compatibility mode**: `setDeathState(DeathState::Dead)`, `DestroyForVisiblePlayers()` unless `skipVisibility`, clear loot, relocate; after reaching Dead the regular `Creature::Update` does NOT call `Unit::Update`.
2. **Dynamic respawn mode**: clear loot, save respawn time and `AddObjectToRemoveList()`; removal goes through the object-removal queue, not a guaranteed later corpse `Unit::Update`.

In both modes an *already* hidden corpse could be lost to a delayed `OnUnitUpdate` restore attempt. This is a **source-derived risk**, NOT an observed live bug: F14's positive hide listener is still ABSENT, feature OFF. Being unable to send a CREATE because the old creature object is gone is not the same as a confirmed restored client object.

## What is tested without wiring any positive policy

The new **single test source file** `tests/f14_two_corpse_modes_restore_debt.cpp` compiles the ORIGINAL unchanged Claude [#94 restore protocol](https://github.com/CosmicCuddle/mod-individual-progression/pull/94), [#85 prior-hidden ledger](https://github.com/CosmicCuddle/mod-individual-progression/pull/85) and [#103 GUID-send attempt bridge](https://github.com/CosmicCuddle/mod-individual-progression/pull/103). It models 16 combinations (compatibility/dynamic × was-linked/was-not-linked × direct/batched × server relink observed/missed) using **hypothetical synthetic F14 prior-hide evidence**. The simulation asserts:

- A prior verified F14 suppression creates an exact viewer GUID/login-epoch, creature map/instance/full-GUID/incarnation record; generic Core link loss is not substituted for positive source attribution.
- Death/attack stop makes restore owed; old viewer session and recycled creature incarnation never discharge it. A delayed F14-tagged OUT after death cannot be promoted to a new hide.
- Even if the old creature is relinked and a matching tagged Core CREATE send-call is reported for the batched path, the server does **not** have evidence of actual 3.3.5a client display. The original ledger remains owing.
- In both compatibility Dead and dynamic removal, a missing later Unit update leaves restoration debt intact. Map-unload, logout and F14 config OFF request further restoration, never clear earlier hidden pairs. Global freeze prevents fresh concealment.
- Each test compiles and runs under GCC/Clang × C++17/C++20 × regular/NDEBUG (eight modes) with the project's original active CHECK macro. The source workflow separately pins and audits the exact public Core `Creature.cpp` modes, proves exactly four changed paths, distributed paid/F14 default OFF, and unchanged source runtime and original #94/#103 artifacts.

**This is NOT a production restoration implementation** and is NOT proof that a Core packet was actually sent or that a human 3.3.5a client saw/targeted a corpse. No new positive concealment provider, runtime `OnF14...` listener, listener registration or ledger clear is added. No SQL/DB/config/DBC/client, Playerbots, Naxx or Services changes.

## Release-blocking design before any future F14 enablement

- Identify and test a **guaranteed, same-current-Map lifecycle restore handoff before both corpse-removal branches**, or demonstrate a different owner-approved provably safe client restore. A queued `OnUnitUpdate` alone is insufficient once Dead/removed. Must preserve all observer/quest/tap/loot interests and original hidden-pair record.
- The original #94 protocol intentionally never exposes a client ACK and never clears its private ledger from a mere Core send-call. A sustainable production completion/reconciliation policy is **not yet designed or owner-approved**. Do not invent a synthetic client ACK, clear the ledger on an OUT/CREATE call, or enable repeat hiding by destroying historical debt.
- Retain existing direct and batched prehide map/frame/session/GM/Alive negative guards (#110–#113), selected positively verified headless BE/Draenei RandomBot-only source, complete real-human/Altbot pet/guardian/aura/group/quest/loot/tap/combat rights, and exclusive owner-map proof. Missing fact => ordinary **VISIBLE**.
- [F01–F20 real WoW 3.3.5a client acceptance matrix](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/blob/main/release-gates/2026-11-06-f14-client-acceptance-matrix.md), especially F07–F09 and F17–F19, remains **NOT RUN**. Owner installed source/custom dirty/config/schema/DBC/client inventory, restorable backups and explicit approval required for one complete combined test.

**Rollback:** close this isolated DRAFT; it adds only a source test, its source workflow, this design note and a Custom Modifications Register entry. No release candidate, live host action or mutable runtime change exists to roll back.

**Coordination:** [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [combined readiness Issue #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
