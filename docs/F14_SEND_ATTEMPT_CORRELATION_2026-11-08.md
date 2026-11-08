# F14 send-attempt correlation (E1) — viewer login epoch + creature incarnation

**Owner:** CosmicCuddle. **Developer:** CS03-IP. **Lead/review:** CS01-IP.
**Date:** 8 November 2026.
**Assignment and ACK:** Coordination #13 [6722253767](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6722253767), claim [6723215377](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6723215377), ACK [6723413637](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6723413637).
**Frozen parent:** IP #104 `3c273c8060eb9f1f90c01f76669b9bec0d1d8935`.
**Status:** DRAFT. A pure, unwired header and tests. There is no runtime listener, and nothing hides or restores anything. All F14 and paid flags stay OFF. `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION` and `COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT` are not defined in source.

## The gap

The #93 OUT and #95 CREATE Core callbacks report only `(Player const* viewer, ObjectGuid creatureGuid)`, after `SendDirectMessage`.

After an OUT unlink, that event can't recover two facts: which **login session** the viewer is on, and which **incarnation** of the creature it was. A creature GUID can be reused after a respawn. #104 therefore correctly refuses to call #103, and #103 can't feed #94 from a GUID-only event.

## Why the facts exist earlier in the same call chain

At pinned Grim Core `0d754d1921261dfd939f91060ebc022eae280f71` (lines as recorded in #94's design note):
- Batched `Player::UpdateVisibilityOf(T*, UpdateData&, ...)` (`PlayerUpdates.cpp` 1702) is where #86 makes its F14 decision, and where #76 reports a re-link. It only **appends** to `UpdateData`.
- That `UpdateData` is flushed in `VisibleNotifier::SendToSelf` (`GridNotifiers.cpp` 36, `BuildPacket` at 85, `SendDirectMessage` at 86). #93/#95 fire after that send call.
- Every batched caller reaches `SendToSelf()` in the same synchronous chain (`PlayerUpdates.cpp` 1633; `Unit.cpp` 16030, 16067). `SendToSelf` also visits zone-wide objects itself (lines 39–58) before the flush.

So the decision (or re-link) and the tagged send attempt happen on the same viewer's map thread, in one call chain. At the decision point the caller has the live viewer's login epoch and the live creature's incarnation. The table records them there, and gives them back to the later GUID-only event.

## Model: `IndividualProgressionF14SendAttemptCorrelation::Table`

### `Nominate`
Runs at the #86 decision (OUT) or the #76 re-link of an owed restore (CREATE). It records one candidate: viewer (GUID + login epoch), creature (map, instance, GUID) + incarnation, direction and pass token.

It is refused unless all of these hold:
- the keys are valid and the token is nonzero;
- the epoch and incarnation are the live ones, and the event is on the creature's map/instance;
- the protocol isn't frozen;
- #94 holds the matching prior-F14 suppression;
- for OUT, **no** restore is owed; for CREATE, a restore **is** owed.

`ReadProtocolFacts` reads the #94 protocol through `const&` (`Find`, `GlobalRestoreRequired`, `NeedsRestore`) only.

### `Correlate`
Runs at a tagged #93/#95 send attempt. It returns the recorded `(ViewerKey, CreatureKey, incarnation)` **only** when exactly one unconsumed candidate matches the same viewer session, creature GUID, direction and current pass token, on the same map/instance.

A matched candidate is consumed: one send attempt, one use. The result is only an **input** a future caller could give #103. The model never calls #103 or any mutating #94 API.

### Pass token: an UNMET Core dependency
The pass token is caller-supplied, opaque, nonzero and strictly increasing per viewer session.
- **No Core producer exists** at the pin, and none is in #93 or #95.
- It is never inferred from GUIDs, maps, clocks, callback order or the packet itself.
- A synthetic accepted token in the tests proves **no** live provenance.
- A future Core owning-map implementation must prove the token is unique for a viewer-login/visibility-pass lifetime, and propagate it through both the nomination and the tagged send attempt.

### Fail-closed rules (each has a native test)

| Case | Result |
|---|---|
| Default facts, unattributed send, zero token, invalid key or direction | refused |
| Stale or relogged epoch; relog without an observed session end | `StaleEpoch` / `UnobservedSessionChange` |
| Recycled GUID or changed incarnation | `RecycledIncarnation` (nominate) or `Ambiguous` (competing in one pass) |
| Map/instance change | `CrossMap` (the candidate is consumed) |
| Wrong direction | `WrongDirection` (not consumed) |
| Two candidates for the same GUID in one pass, e.g. two instances | `Ambiguous`; every competitor is dropped and an identical retry can't rescue the slot |
| Old send after a successor pass | `StaleToken` |
| Reused, reordered or wrapped token | `TokenReorderedOrReused`; poisons **that viewer session only** until its end is observed |
| Candidate or viewer capacity overflow | `Capacity`; the whole table is poisoned and drops all candidates, with no eviction or partial guess |
| Restore owed for OUT, or not owed for CREATE | refused |
| Global freeze, at nomination or send | `GlobalFreeze`; nothing consumed or upgraded |
| Creature retired, map unloaded, viewer rights or era changed, session ended, pass ended or cancelled | candidates dropped, so a later send finds `NoCandidate` |

**Expiry is explicit only.** A candidate expires on an observed newer token for the same viewer session (via `ObserveSuccessorPass`, or a nomination carrying it), or on the explicit events above. Pass end and cancellation are externally supplied hypothetical events, because no Core producer exists. A send call is never treated as pass completion or delivery. A candidate can only match the viewer's current token, so it can't be matched indefinitely across passes.

**Dropping a candidate never touches #94.** The table holds no restore obligations. Those live only in #94's ledger, which this model can't change, and they survive a global freeze.

A repeated identical visit in the same pass (for example, a grid visit and then the zone-wide loop) is idempotent (`DuplicateIgnored`).

## B4 analysis note: information #103 intentionally loses during a global freeze

Per CS01's decision, #103 checks `GlobalRestoreRequired()` before **both** directions, and this model refuses nominations and correlations while frozen. Neither is changed here.

- **What is lost.** After a global restore request (config OFF, overflow or unattributed hide risk), tagged CREATE send attempts for owed restores aren't recorded. Restore-side packet evidence for those pairs stays at `ImpliedPendingBatchFlush` even when Core did call `SendDirectMessage` with that GUID. During the period when restores matter most, the server keeps less evidence about how far each restore got.
- **Why it's acceptable for now.** A frozen protocol means the origin or prior state of some hides may be ambiguous. The evidence couldn't clear anything anyway; clearing still needs separately verified client proof, which doesn't exist. The safety rule ("every obligation stays owed") holds either way. Only diagnostic detail is lost.
- **What would be needed to recover it later.** A separately reviewed, owner-approved design that records CREATE send attempts during a freeze in a **diagnostic-only** channel, kept apart from evidence and never upgrading any protocol stage. That is not proposed or implemented here.

## Server-observable vs client-test-only

- **Server-observable (once wired with a real token):** that a tagged send **call** for a uniquely correlated (viewer session, creature incarnation) happened.
- **Never server-observable:** socket transmission, client receipt, rendering, targetability or restoration. 3.3.5a has no object create/destroy ACK opcode (#94 trace T9). These need a CosmicCuddle-approved real 3.3.5a client test.

## Tests

- **Native:** `tests/f14_send_attempt_correlation.cpp`, 3,706 checks on GCC and Clang, C++17 and C++20, with and without NDEBUG.
  - every case in the table above
  - an end-to-end OUT and CREATE run through the **unchanged** #103 adapter and #94 protocol, where the ledger never shrinks and the restore stays owed
  - a 3,000-step randomised sequence checking that a match is only ever the nominated identity, used once, and never from an expired pass
- **Source contracts:** `tests/test_ip_f14_send_attempt_correlation_source.py`, 15 contracts.
  - pure includes, with no pointers, Core types or threads, and nothing includes the header
  - protocol access only through `const&` and three read accessors, with no adapter, ledger or clearing call
  - fail-closed defaults; checks precede any state change; strict tokens; ambiguity and freeze ordering; poison on overflow
  - the original #94 blobs (`2250bae5`, `f520356a`, `9bce90ed`)
  - **every parent file outside the six paths byte-identical to `3c273c80`**
  - both macros undefined, F14 and paid flags OFF
- **Local mutation checks:**
  - model: 31 of 32 caught. The survivor removes a defensive check: a never-nominated newer token already matches nothing, because older candidates are dropped when a newer token is observed.
  - contracts: 14 of 14 caught.

This is model and source evidence only. It isn't a Worldserver build (the header isn't compiled into it), a packet capture or a client test.

## Owner obligations kept visible

Before any future integration, the owner still needs:
- an installed source/config/SQL/DBC/client inventory
- tested backups and rollback
- complete human combat, group, quest, loot, pet and DoT rights
- verified selected headless BE/Draenei RandomBot identity
- a real Core pass-token producer with proven provenance
- client-visible hide, re-create and targetability tests across stop, death, logout, map change and overflow
- CosmicCuddle's explicit approval

Any future owner-approved offline test must preserve tracked, untracked **and ignored** local custom SQL cleanup and migration files.

## Rollback

Close the DRAFT or delete the branch. Nothing is compiled into the Worldserver. No host, database, config or client was touched.
