# F14 per-viewer source-attributed restore protocol — pure evidence model

**Owner:** CosmicCuddle. **Developer:** CS03-IP. **Lead/review:** CS01-IP. **Date:** 5 November 2026.
**Base:** IP #85 `6bbd4542e8b8b992c51d21955a341c1f988658c6` (prior-hidden ledger, unchanged).
**Read-only reference:** IP #86 `2933d939bea7ebf9abca224c585ddc3c08744eda` (Core decision seam, unchanged).
**Status:** DRAFT, UNWIRED pure header and tests. No runtime source, Core, config, SQL, DB, DBC, client or host change. All F14 and paid flags stay OFF; `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION` stays undefined.

## Why

#85's ledger can hold a restore obligation, but it accepts a single caller-supplied "proven F14 suppression" boolean. Nothing yet says *which* Core facts justify that boolean, or how far a later restore got. This model maps the #86 decision and #76 link transitions onto four separate evidence stages, feeds the ledger only with stage-2 attribution, and fails visible when a positive F14 decision can't be recorded exactly.

## Source trace — pinned Grim Core `0d754d1921261dfd939f91060ebc022eae280f71`

Line numbers are exact at the pin and re-checked by the workflow's `pinned-core-trace` job.

| # | Fact | Where (pinned Core) |
|---|---|---|
| T1 | Direct path: linked and `!CanSeeOrDetect` → `DestroyForPlayer(this)` then `UnlinkWorldObjectVisibility` | `src/server/game/Entities/Player/PlayerUpdates.cpp` 1738, 1742, 1747, 1748 |
| T2 | Direct path: unlinked and visible → `SendUpdateToPlayer(this)` then `LinkWorldObjectVisibility` | `PlayerUpdates.cpp` 1753, 1755, 1756 |
| T3 | `DestroyForPlayer` builds `SMSG_DESTROY_OBJECT` and calls `SendDirectMessage` synchronously | `src/server/game/Entities/Object/Object.cpp` 272, 289, 294 |
| T4 | `SendUpdateToPlayer` builds its own `UpdateData` packet and calls `SendDirectMessage` synchronously | `Object.cpp` 244, 252 |
| T5 | Batched path only **appends**: `BuildOutOfRangeUpdateBlock` + immediate unlink, or `BuildCreateUpdateBlockForPlayer` + link via helper | `PlayerUpdates.cpp` 1702, 1709, 1713, 1714, 1719, 1721 |
| T6 | Batched data is built and handed to the session later, in `VisibleNotifier::SendToSelf` (`i_data.BuildPacket`, `SendDirectMessage`), and only if `i_data.HasData()` | `src/server/game/Grids/Notifiers/GridNotifiers.cpp` 36, 85, 86 |
| T7 | Every batched caller ends in `SendToSelf()` in the same call chain | `PlayerUpdates.cpp` 1633; `src/server/game/Entities/Unit/Unit.cpp` 16030, 16067 |
| T8 | Ordinary (non-F14) unlinks exist: the `SendToSelf` out-of-sight sweep, judged by plain `CanSeeOrDetect` | `GridNotifiers.cpp` 66, 78 |
| T9 | No client acknowledgement opcode for object create/destroy in 3.3.5a; the only `*UPDATE_FAILED` opcodes are quest ones | `src/server/game/Server/Protocol/Opcodes.h` 199, 200, 436, 437 |

#86 (read-only) narrows the first `CanSeeOrDetect` in each `UpdateVisibilityOf` body to `!f14BaselineVisible || f14PositiveSuppression`, and calls `OnF14CreatureSuppressionDecision(viewer, creature, wasLinked, batched)` only when `f14PositiveSuppression` is true. So that decision is a trustworthy **F14-specific** server fact: `wasLinked=true` means F14's branch removed the link; `wasLinked=false` means F14 withheld a create that baseline Core would have sent.

## The four stages

| Stage | Suppression | Restore | Server-observable? |
|---|---|---|---|
| 1. Policy intent | #86 query answered true (implied by the decision) | n/a | Yes |
| 2. Server link change | `UnlinkedByF14` or `CreateWithheldByF14` from the #86 decision | `RelinkedAfterF14`: #76 `nowLinked=true` for the **same** viewer GUID + session epoch, creature map/instance/GUID + incarnation, on the owning map | Yes |
| 3. Packet handed to session | Direct: `HandedToSessionDirect` (T1/T3, synchronous). Batched: `ImpliedPendingBatchFlush` only (T5–T7) | Same split (T2/T4 vs T5–T7) | Direct: yes. Batched: **only implied** until a flush hook exists |
| 4. Client presentation | `ClientStage::NotServerObservable` (the only value; no setter) | Same | **Never** (T9) |

"Handed to session" means `SendDirectMessage` returned. It doesn't prove the bytes reached the socket, or that the client drew or removed anything.

## Rules the model enforces

- **Ledger input:** only `ObserveF14SuppressionDecision` calls `RecordConfirmedF14Suppression`, and only after every fact check. The protocol owns the ledger privately, exposes it read-only, and never calls `CompleteRestoreOnlyWithClientProof` or `ClearFrozenAfterGlobalProof`. So every restore obligation stays owed until a separately approved client-proof path exists.
- **Fail visible:** a positive #86 decision already changed Core state. If it can't be recorded exactly, the model sets a **sticky** unattributed-hide risk and asks the ledger for a global restore. Reasons: config OFF, actor not a positively verified RandomBot (humans, Altbots), no entitlement, pair rights not proven, stale viewer session, recycled creature incarnation, creature not alive, event not on the creature's map/instance, invalid key, ledger refusal (frozen, pending restore, full, recycled), record capacity. Every default fact value is the failing one.
- **Ordinary link changes are never suppression evidence.** Phase, distance, GM, despawn, map change and the out-of-sight sweep reach the model only as #76 generic transitions. For an unrecorded pair, or any unlink, they're ignored.
- **Restore evidence is strict.** A relogged viewer, an event captured for an old epoch, a reused creature GUID or another map's thread is ignored and the obligation is kept. A re-link while F14 still believed it suppressed first turns into an explicit obligation.
- **Lifecycle only adds obligations:** creature retirement (stop, death, corpse, respawn), viewer logout or relog, viewer era or rights change, map unload (only that map/instance) and config OFF.
- **Pair rights untouched.** The protocol takes `pairRightsProven` as an input and never reads or answers `F14CombatVisibilityPolicy`, which stays default false (`plainUnscriptedNonEliteHostileNpc` is never set true).
- **Not thread-safe.** A future caller needs one owner or one mutex, and must not hold it across a Core visibility call.

## Proposed next Core hook (CS01's Core lane, not built here)

`OnF14VisibilityBatchFlushed(Player* viewer, outOfRangeGuids, createdGuids)`, called immediately after `i_player.SendDirectMessage(&packet)` in `VisibleNotifier::SendToSelf` (`GridNotifiers.cpp` 86). It would let a caller move batched evidence from `ImpliedPendingBatchFlush` to `HandedToSessionBatched` via `ObserveProposedBatchFlush`. Even then, stage 4 stays client-test-only.

## Server-observable vs client-test-only

- **Server-observable now:** stage 1 and 2 (via #86), direct-path stage 3, all lifecycle invalidations.
- **Server-observable only with the proposed hook:** batched-path stage 3.
- **Needs a CosmicCuddle-approved real 3.3.5a client test:** stage 4 for both suppression and restore — that a hidden NPC actually disappears, and that it actually reappears (with correct auras, melee state and target frames) after target switch, death, logout, relog, map change and config OFF.

## Tests

- `tests/f14_viewer_restore_protocol.cpp`: 1,434 native checks on GCC and Clang, C++17 and C++20, with and without `NDEBUG`. Every missing fact fails visible and stays sticky. Stages stay separate on direct, batched and withheld-create paths. Ordinary transitions are ignored. Restore evidence never shrinks the ledger. Batched evidence never reaches "handed" without the proposed hook. Map unload, relog, era change, config OFF, recycled incarnation and capacity all keep obligations. A 400-step mixed sequence checks that the ledger never shrinks.
- `tests/test_ip_f14_viewer_restore_protocol_source.py`: 14 contracts. Pure includes, no pointers or Core types, nothing includes the header. No ledger completion or reset. Ledger private, read-only view. Single sticky assignment. Client stage has one value. Batched "handed" appears only in the flush method. Default facts all fail. #85's parent files, the runtime source and config are byte-identical to `6bbd4542`. Pair rights stay default false. Refresh macro undefined, F14 and paid flags OFF.
- Mutation checks were run locally: 16 of 17 model mutations and 11 of 11 contract mutations fail the suites. The survivor removes a redundant check (a fail-visible event also freezes the ledger, which already blocks new concealment).

**This is model and source evidence only.** It isn't a compiled Worldserver change, a packet capture or a client test.

## Rollback

Close the DRAFT PR or delete the branch. Nothing is compiled into the Worldserver, and no host, database, config or client was touched.
