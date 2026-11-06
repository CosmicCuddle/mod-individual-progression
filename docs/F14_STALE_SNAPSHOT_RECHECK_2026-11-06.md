# F14 stale prior-hidden snapshot guard — pure fail-closed handoff

**Owner:** CosmicCuddle. **Workstream:** Server-Side-Maintenance CS01-IP. **Date:** 6 November 2026.
**Status:** stacked DRAFT / OFF / UNWIRED. No real Worldserver visibility callback, client packet, DB, config, SQL, DBC, host or production operation.

## Problem discovered in original plan

The additive read-only [#117 original #94 snapshot](https://github.com/CosmicCuddle/mod-individual-progression/pull/117) safely copies exact known historical F14-hidden viewer/creature records, and the source-green [#116](https://github.com/CosmicCuddle/mod-individual-progression/pull/116) verifies an individual old pair against supplied current normal-human, session, map and creature facts.

**Between copying the list and reaching a Core callback, the list can become stale.** An unrelated creature may be retired, F14 may turn OFF and revoke additional pairs, a viewer may log out, a creature can respawn/recycle its GUID, or a Core link may already have been restored. Reusing an old list and earlier captured session facts as if they were current is unsafe. The original #94 protocol does not expose a monotonic revision/generation counter, so merely comparing a caller's timestamp would not prove freshness.

## New pure value-only two-pass revalidation

The single NEW unused header `src/IndividualProgressionF14StaleSnapshotRecheckGuard.h` takes:

- An immutable exact historical snapshot from #117, including original snapshot status and whether **global restoration was required** when copied.
- A **different caller-owned fixed scratch array**, so it can call the original #94 `CopyExactPendingRestores` again for the CURRENT state. Using the same array for both input and output is explicitly refused.
- A fresh set of current viewer/creature facts to pass to the existing #116 normal-Core candidate planner, and a caller-supplied assertion of **independently verified serialized owner-map/protocol access**.

The entire list is compared again, not just the requested pair: **all pending pairs, count, original viewer login epochs, creature incarnations/maps, source-attributed suppression stage, server-only relink evidence, global restoration flag and `KnownPairsWithUnattributedRisk` status** must remain identical. Newly owed pairs, global freeze, a relink, capacity overflow or unattributed-risk change invalidate the old snapshot; no partial/unreviewed dispatch. Once the list still matches, the selected pair is rechecked against **fresh** normal-human/session epoch, creature GUID+incarnation, map+instance, and owning-map input. Feature OFF or a player advancing beyond Vanilla does NOT cancel an already owed restoration. All failed gates retain every historical pair.

The only possible favourable responses are `CandidateNormalCoreRecheckOnly` or `CandidateKnownPairWithUnattributedRisk`. The second deliberately **does not call the known list a global all-clear**; known exact pairs can still be eligible for a recovery attempt even if additional historical suppression is unattributed. Neither result authorises a CREATE or bypasses normal Core `CanSeeOrDetect`.

## Fundamental safety limits

**This code cannot prove atomic delivery.** Checking the original #94 state a second time does not by itself guarantee that no subsequent change occurs before Core visibility. A future real caller MUST be on the genuinely serialized owning Map event/thread, hold the original protocol mutex only through the snapshot/recheck read, then RELEASE ALL locks **before** Core/player visibility calls, while ensuring the map and exact player/creature lifetime cannot change during that operation. Do not carry raw player or creature pointers across queued jobs. The current #108 map-update frame marker does not prove ownership exclusivity and passing `sourceProtocolReadSerializedOnOwningMap=true` in this pure test is merely **synthetic input**, not actual trusted source.

There is still no authorising F14-positive listener, no completed Human/Altbot quest/tap/loot/assist/DoT/pet/group rights census, no verified selected headless Blood Elf/Draenei RNDBOT source, no completed exact previous hidden capture in runtime and no owner-approved removed-creature/relog/feature-OFF reconciliation. The original #85 ledger, #94 protocol and #103 send-attempt bridge are unchanged; none can be cleared because a client 3.3.5a per-object CREATE/OUT ACK does not exist. Core server relink, attempted SendDirectMessage, a complete copied list, or a successful build are not actual client restoration.

## Exact scope and tests

Child of [#117](https://github.com/CosmicCuddle/mod-individual-progression/pull/117) exact native-independent source-green head `03332da2fbc3e0968a6fe490b67d80a9514bb8f1`. Exactly **five** changed paths: this new PURE header, `tests/f14_stale_snapshot_recheck_model.cpp`, source-only GitHub workflow, this document and Custom Modifications Register entry. **No edits** to protected original #85/#94/#103 files, active IP Core Playerbots Naxx Character Services runtime, SQL/DB/DBC/client or distributed paid/F14 config. The workflow guards exact paths/defaults and compiles/runs the pure model under eight GCC/Clang C++17/20 normal/NDEBUG modes.

Test cases simulate separate old F14-positive pairs **only in test memory**, revoke one NPC and snapshot, then mutate the original protocol through another creature retirement, feature OFF global revocation, a late Core relink, and an unattributed hide risk; old lists MUST fail. The model also rejects an alias output buffer, missing serialized ownership assertion, wrong current normal human/session, stale login, wrong map/instance/incarnation, and too-small snapshot. It enumerates **65,536 current live-fact combinations per compiler configuration** while verifying original restoration debt never decreases or clears. The single candidate result cannot send Core packets and cannot assert an object ACK.

**Caution:** source/CI GREEN would only validate a pure mathematical contract; **the real map-thread and client proofs are still missing**. [Client acceptance F01–F20](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/blob/main/release-gates/2026-11-06-f14-client-acceptance-matrix.md) remain NOT RUN. Owner installed source/module/config/schema/client inventory, recovery-tested backups and explicit approval for one complete offline test are still required.

**Rollback:** reject/close the isolated DRAFT. No host or gameplay files have changed. Details coordinated in [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9) and [combined readiness #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
