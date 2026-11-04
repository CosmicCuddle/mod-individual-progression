# F14 — pinned Core viewer refresh and creature removal routing

- **Owner:** CosmicCuddle. **Maintainer:** Server-Side-Maintenance / CS01-IP.
- **Scope:** SOURCE CONTRACTS ONLY; no new executable feature, no Core fork writes, no production installation.
- **Pinned reference:** `Grimfeather/azerothcore-wotlk@0d754d1921261dfd939f91060ebc022eae280f71`.
- **Parent draft:** [F14 Core after-pass observer #67](https://github.com/CosmicCuddle/mod-individual-progression/pull/67).
- **Test:** `tests/test_ip_f14_core_refresh_delivery_path.py` in a read-only pinned checkout.

## Engine behaviour verified by source

1. [`WorldObject::UpdateObjectVisibility`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Entities/Object/Object.cpp#L3073-L3081) constructs `Acore::VisibleChangesNotifier` and calls `Cell::VisitObjects`. This is a synchronous nearby-viewer **server reevaluation**, not a client ACK.

2. [`VisibleChangesNotifier`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Grids/Notifiers/GridNotifiers.cpp#L95-L124) walks nearby human/bot player objects, shared-vision viewers and dynamic-object farsight. [`Player::UpdateVisibilityOf(WorldObject*)`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Entities/Player/PlayerUpdates.cpp#L1735-L1763) uses live `CanSeeOrDetect` and either destroys/unlinks or sends an update/links the target.

3. **Player-target refresh is distinct.** [`Player::UpdateObjectVisibility(true,false)`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Entities/Player/PlayerUpdates.cpp#L1641-L1659) sets `bRequestForcedVisibilityUpdate` and returns; the actual [Player::Update queued processing](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Entities/Player/PlayerUpdates.cpp#L421-L430) only runs when the player is **not teleporting** and then performs both `UpdateObjectVisibility(true,true)` and `UpdateVisibilityForPlayer()`. Calling the request method is not proof that the map has already processed it.

4. The **existing** [`Creature::RemoveFromWorld`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Entities/Creature/Creature.cpp#L344-L360) calls `sScriptMgr->OnCreatureRemoveWorld(this)` **before** `Unit::RemoveFromWorld()` and before map-store removal; [`AllCreatureScript`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Scripting/ScriptDefines/AllCreatureScript.h#L43-L55) can subscribe without a new Core patch. Normal [`Map::RemoveFromMap`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Maps/Map.cpp#L747-L755) invokes `RemoveFromWorld` ahead of map reset.

## F14 integration guardrails

- **Never call a target visibility reevaluation while holding the F14 lifecycle registry mutex.** The notifier can re-enter a per-viewer F14 `CanSeeOrDetect` consumer that reads the registry. If that reader needs the same nonrecursive mutex, the map worker deadlocks. Commit a bounded value-only state transition, release the registry lock, revalidate on the owning map thread, *then* request reevaluation. No raw `Player*`/`Creature*` should be queued across map worker boundaries.
- A per-viewer `CanSeeOrDetect` callback must **fail visible** if actor/creature incarnation, generation, stage, session, map, group rights, source of threat, tap/loot/quest credit, pet/minion ownership, or packet state is unknown. A mere positive actor attack or historical threat is insufficient. In particular, a creature corpse must be visible and lootable after death.
- The existing notifier may emit create/destroy updates but does **not** guarantee a 3.3.5a client has processed them; it is not a source of client acknowledgements. Validate actual nearby clients, shared vision, corpse restoration and teleport/relog in a later owner-approved offline/gameplay test.
- **Do not define `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION`** from this source audit or #67's after-pass callback alone. All experimental F14 concealment, TBC racial RandomBot floor, paid entitlements and unrelated toggles remain OFF.

## Verification / rollback

The added workflow checks the immutable Core SHA, exact parent ancestry, an exact **three-file test/docs/CI allowlist**, and performs 12 source contracts. It does **not** compile, start, install, patch, query or modify a Core, Playerbots, SQL database or live server. If the pinned Core changes, the tests should be re-evaluated and retargeted explicitly rather than silently passed.

**Rollback:** close/discard this draft branch; no runtime changes exist. **Approval gate:** CosmicCuddle separately approves a reviewed combined native build and later gameplay tests, after Full F14 participant/rights proofs and both worker/client visibility are genuinely implemented.
