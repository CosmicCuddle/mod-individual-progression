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
| Creature retired, map unloaded, viewer rights or era changed, session ended | candidates dropped, so a later send finds `NoCandidate` |
| Explicit pass end/cancel of the active token | candidate dropped; a late same-token send returns `PassClosed`, a same-token re-nomination poisons that viewer session |
| Stale end of older pass | `StaleToken` with no mutation to the newer active pass |
| Strictly newer end, including an empty or never-nominated pass | `PassAccepted`. `lastToken` and `closedThrough` move to that token. That viewer's candidates are dropped. The viewer is **not** poisoned, so a later token can still nominate. Replaying the ended token still poisons that viewer session |
| Newer login epoch vs a fully closed, candidate-free, unpoisoned tombstone | the tombstone is replaced. Live evidence (open pass, any candidate, or a poison flag) is not |
| Older-epoch end against a newer epoch's record | `UnobservedSessionChange`. No poison and no deletion |

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


## 10 November 2026 — CS01 Maintenance temporary ownership: closed pass replay repair (DRAFT, OFF)

**Context:** CosmicCuddle temporarily reassigned unfinished CS03-IP E1 work to CS01 while Claude is on break. Preserve Claude's original #131 and branch. This section documents a separate proposed six-path child, not production activation. Coordination: [Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13).

**Old confirmed source defect:** previously, `ObservePassEnded` and `CancelPass` dropped candidates but left `lastToken` open. Later `Nominate` accepted the same closed token and a matching `Correlate` reported a successful *send-call* correlation, violating pass lifetime. An event replay is not proof of real packet delivery.

**New pure-model rule:** each `ViewerPass` records `closedThrough`. A normal active `ObservePassEnded` or `CancelPass` records that token as closed and discards only that pass's candidates. A late send of the closed active token returns `PassClosed` with no match. Re-nominating or explicitly re-opening that closed token returns `TokenReorderedOrReused` and poisons **that viewer session only**. A strictly newer token remains valid if the viewer was not poisoned. Ends older than the current active token return `StaleToken` without touching the newer pass. An end for an unknown viewer stores its closed token without inventing an active pass, subject to bounded capacity. Duplicate active end returns `PassClosed`. Invalid epoch, zero token and table-overflow rules remain fail closed. Multiple different creatures can be nominated under one **still open** token.

The 10 November CS02 child below changes one part of that rule: a strictly newer end closes through the new token instead of poisoning the viewer. Same-token replay of an ended pass is unchanged.

**Old-RED/new-GREEN protection:** the workflow compiles exactly the same old-API C++ behavior reproducer against the pristine #131 header and the proposed header, using GCC/Clang C++17/C++20. The old build MUST compile but exit with a reproducible unsafe replay match; the new build MUST compile and exit successfully with no match. A compile failure solely from a changed enum or return signature is never accepted as old-RED. Native suite also checks unknown/stale/future end, duplicate end, cancellation, multi-creature pass and #94 restore-debt invariance. **At authoring, hosted Actions must still be independently checked for terminal success before calling these results PASS.**

**Core/source limitation:** newer owner-Core draft #8/#9 proposes default-OFF notifier pass and exact batch GUID send-CALL producers, but this pure table is **unwired** and knows no verified present Core listener/refresh ABI, delivery acknowledgement, current creature incarnation or observer epoch. In particular the immediate `Player::UpdateVisibilityOf(WorldObject*)` direct `DestroyForPlayer`/`SendUpdateToPlayer` path is separate from the batch notifier and **cannot inherit its send evidence**. Never clear any #94 `PriorHiddenLedger` owed restore on Begin/SendCall/End, cancellation or this correlation. Worldserver/native, 3.3.5a two-human gameplay, owner S5 installed source/DB/SQL/DBC/client and backup restore remain release blockers. F14 and paid features OFF.

**Rollback:** close this isolated DRAFT and abandon its child branch. Original #131 and #94/#103/#104 stay unchanged; no owner host, account data, SQL, config, packet format or client touched.

## 10 November 2026 — CS02 pass liveness (DRAFT child of #141, OFF, not wired)

This section is a pure-model correction on top of the closed-pass replay rule above. It does not register a listener, does not define a Core macro, and does not turn any F14 or paid flag on. `HasCorePassTokenProducer()` stays false.

### Strictly newer end

`ObservePassEnded` / `CancelPass` of a token strictly above `lastToken` now sets both `lastToken` and `closedThrough` to that token, drops only that viewer's candidates, and returns `PassAccepted`. The viewer is not poisoned. A later higher token can nominate. A same-token nominate of the ended token still returns `TokenReorderedOrReused` and poisons **that viewer session only**. A late send of the ended token returns `PassClosed` and does not match. An older end still returns `StaleToken` and does not change the newer pass.

Backwards, reused or wrapped tokens on `Nominate` / `ObserveSuccessorPass`, and re-opening a closed token, still poison that viewer only. Another viewer's candidates are left alone. Several creatures may still be nominated under one open token; ending that token, or a newer one, drops them together.

### Logged-out epoch and tombstones

Viewer slots are keyed by raw GUID. A different `loginEpoch` is a different session. Replacement is allowed only when all of these hold: the incoming epoch is strictly greater, the stored pass is fully closed (`closedThrough >= lastToken` and `lastToken != 0`), the record is not poisoned, and no candidate remains for that GUID. That record is a tombstone. The same slot is overwritten. An open pass, any candidate, or a poison flag is live evidence and still returns `UnobservedSessionChange` until `ObserveViewerSessionEnded`.

A late end from an older epoch, including one that arrives after logout recreated a tombstone and the new epoch has since stored live evidence or its own tombstone, does not poison and does not delete the newer record. Logout itself still forgets the slot. The hole this closes is the end that arrives **after** that forget and installs a closed tombstone for the old epoch, which previously made every later epoch return `UnobservedSessionChange` for the rest of the table's life.

### Unknown-viewer capacity (default 256)

The 256-slot unknown-viewer table is `DefaultMaxViewers` (256). Each distinct viewer GUID occupies one slot, whether it got there by nomination or by an end for a GUID the table had not seen. A new login epoch of a GUID that already has a slot does not take a second slot.

When all 256 slots are full, the next end or nomination for a GUID that is not already stored calls `PoisonTable()`:

- the table's poison flag is set and is never cleared (there is no reset);
- every candidate is dropped, including candidates that belonged to viewers already in the table;
- no slot is evicted and no closed tombstone is reused to make room;
- every later `Nominate`, `Correlate`, `ObserveSuccessorPass`, `ObservePassEnded` and `CancelPass` returns `TablePoisoned`;
- `ObserveViewerSessionEnded` can still forget a GUID, but that does not unpoison the table or free a usable slot for new evidence.

Filling the 256 slots with unknown-viewer closed tombstones is enough. The 257th distinct GUID's end poisons **every** viewer, not only the new one. This does not touch the #94 prior-hidden ledger. That ledger is a different object. This table never holds restore debt and never calls a ledger clear.

Candidate overflow (default 1024) is a separate counter and poisons the same way.

### What Core8 and Core9 actually do (not consumed here)

Read from Michael's Core fork, not from this header:

- Core draft [#8](https://github.com/CosmicCuddle/azerothcore-wotlk/pull/8) head `2e2a21ae2224a9738cd92abf00bab957dbfb7604` is default **OFF**. The new calls exist only when `COSMIC_F14_NOTIFIER_PASS_LIFECYCLE_EXPERIMENT>=1`. Nothing in that PR registers a script listener.
- `VisibleNotifier`'s constructor calls `BeginF14NotifierPass()` after `m_newVisible.clear()` and before the near/far scans, and only for an in-world player with a non-headless session. `VisibilityPassTokenSource::Issue()` hands out one process-global nonzero token via atomic CAS, starting at 1. It saturates: after `UINT64_MAX` the next issue returns 0 and the constructor records no pass. Tokens are not per viewer and do not wrap.
- `SendToSelf` emits `OnF14ViewerVisibilityPassBatchSendAttempt` only after `SendDirectMessage`, and only when the batch had data and a token was born. An empty batch returns before that call and emits no send event.
- The destructor always emits `OnF14ViewerVisibilityPassEnd` for a born token, including an empty batch, an early return, and stack unwinding. The notifier is non-copyable under the macro, so one stack object ends its own token once.
- Those three callbacks carry `(Player const*, token)` only. They do not carry a login epoch, a creature GUID, an incarnation, or a client ACK. `Player::UpdateVisibilityOf(WorldObject*)` is not on this path and is not given a token.
- Core draft [#9](https://github.com/CosmicCuddle/azerothcore-wotlk/pull/9) head `865724cee8d0fb94d7496854c0f426f60bac2232` adds per-creature CREATE/OUT GUID candidates only when **both** its macro and the Core8 macro are set. The GUID event is dispatched after the same `SendDirectMessage`, and only with the real Core8 token. It still has no registered consumer, no login epoch, and no incarnation. Pass end remains the Core8 destructor, including for passes that nominated nothing.

So a real Core8 pass is begin, then an optional batch send, then end, including when the pass is empty. A later pass for the same viewer has a strictly greater token, not necessarily the previous token plus one. This table does not receive those callbacks. The liveness rule above is only so that a future consumer can forward an empty end without killing the viewer session. It is not a claim that Core8 already calls `ObservePassEnded`, and it is not evidence of packet delivery or of a cleared #94 debt.

### #94 debt check

The native composition test snapshots ledger size, pending restores, poison, freeze, `NeedsRestore`, and the found evidence's incarnation and restore stage **before** end, cancel, same-token replay and a strictly newer empty end, then requires the same values afterwards. Pending is required to be at least one, so the comparison is not an empty ledger matching itself. A correlate of the ended token must not be `Matched`. The #94 ledger and its blobs are unchanged.


### IP142 compatibility composition — 11 November 2026 (separate DRAFT)
This exact original E1 pure header and native model is copied unchanged into a new child of
IP142 `f259c859`, which retains later raw-stage/cache-only human observer safeguards
and an additive read-only pending-pairs snapshot in the original #94 protocol.
This import does NOT wire the header, a listener or a human session epoch.
Core14's new compile-opt positive-query OUT callback is separate, emits no
authenticated IP facts, and does not authorize E1 nomination or clearing #94 debt.
All positive hiding and paid/F14 feature flags remain OFF.
