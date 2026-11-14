# F14 exact previously-hidden viewer → normal Core visibility recheck planner

**Owner:** CosmicCuddle. **Maintainer:** Server-Side-Maintenance CS01. **6 November 2026.**
**Status:** stacked DRAFT, PURE/unwired, source-model only, F14 and paid flags OFF. No host or player data changes.

## What this solves — and what it does not

The existing original [Claude #94 source-attributed per-viewer restore protocol](https://github.com/CosmicCuddle/mod-individual-progression/pull/94) knows an exact `ViewerKey(guidRaw, loginEpoch)`, `CreatureKey(mapId, instanceId, guidRaw)`, and incarnation for an actual positively attributed F14 hide. The previous [#115 source audit](https://github.com/CosmicCuddle/mod-individual-progression/pull/115) demonstrated that the existing corpse-ready default-FALSE refresh can reach `VisibleChangesNotifier` and conditional `Player::UpdateVisibilityOf` CREATE/link **before either removal mode**. But that generic creature-wide pass doesn't know *which viewer* the F14 hide actually affected. It is NOT enough to call it a completed restoration.

**This proposal defines a conservative, value-only decision to RECHECK exactly one previously F14-hidden viewer/creature pair.** The new `src/IndividualProgressionF14ExactViewerRestoreRecheckPlan.h` reads an **unchanged** `Protocol::Find` and `NeedsRestore` record from #94; it cannot enumerate the private records, send a packet, touch a Core object, or clear prior hidden debt.

| Decision | Conservative behaviour |
| --- | --- |
| No **exact** previously attributed F14 hide (or wrong creature incarnation) | No recheck; never invent evidence from an ordinary visibility OUT |
| A hide is recorded but no lifecycle/rights/feature-OFF revocation was recorded | Require revocation first, no accidental repeated CREATE |
| A matching server relink was already observed | Do not repeat server CREATE; **client rendering still unknown** and original restoration debt remains |
| Missing/headless/Altbot/unknown current session, stale viewer login epoch or viewer left the world | No unsafe recheck. Record remains owed; use separate future reconnect/reconciliation policy |
| Creature no longer in world, recycled GUID/incarnation, map/instance mismatch or owner-thread unknown | No stale pointer/foreign map dispatch; prior hidden debt never erased |
| Exact old pair, *already revoked*, current positively proven normal-human nonheadless session, same map/instance/incarnation and independently verified owner-map context | **Candidate for normal Core `Player::UpdateVisibilityOf`/CanSeeOrDetect re-evaluation only**; it is not a forced CREATE, a visibility override, a success callback or client ACK |

The current viewer's Vanilla/TBC status and whether experimental F14 has since been **disabled** are deliberately NOT required for rechecking an **already owed restoration**. An expansion upgrade or toggling OFF must not strand a previously hidden creature. Conversely, this candidate can NEVER authorise a new positive hide.

## Limits, important to a future implementer

- This PURE planner is intentionally **not wired** to `IndividualProgressionF14LifecycleScript.cpp`, `PlayerUpdates.cpp`, or a new hook. It never sends `SendUpdateToPlayer`, creates/destroys objects, clears Claude's `CompleteRestoreOnlyWithClientProof` ledger, or observes a client object ACK. Normal `CanSeeOrDetect`, phase/distance, pet/quest/loot and shared-vision rules still belong to Core.
- Input `currentOwningMapThreadProven` is an **external positive assertion**, **NOT proof** of exclusive map ownership. The #108 synchronous map-frame diagnostic is insufficient alone. Likewise `normalAccountHumanNonHeadlessSession` must be established from actual live authoritative session/account facts, not a raw GUID or headless flag alone. A future implementation must use an owning-map safely bounded queue and release any Claude protocol/mutex lock before Core visibility callbacks. Avoid holding old `Player*` or `Creature*` across callbacks.
- The original Claude protocol has **no public prior-hidden pair iterator**. This planner only accepts exact supplied keys; a future reviewed, bounded, non-evicting record enumeration/handoff must be designed. This is a major separate integration task, not implied by this header.
- A safe recheck does not guarantee that the old creature still exists or that the corresponding client actually has rendered it. The WoW 3.3.5a protocol does not offer a per-creature CREATE ACK. No hidden record can be cleared on a mere Core send/link/timeout. An owner-approved realistic reconnect/era-change/removed-creature reconciliation and actual client testing is required.
- For testing, synthetic original #94 F14-positive proof is used **only inside pure tests**. Current actual positive policy listener remains absent. Paid and F14 config defaults stay OFF, and original Claude [#85 ledger](https://github.com/CosmicCuddle/mod-individual-progression/pull/85)/[#94 protocol](https://github.com/CosmicCuddle/mod-individual-progression/pull/94)/[#103 send attempt adapter](https://github.com/CosmicCuddle/mod-individual-progression/pull/103) stay unchanged/unwired.

## Verification

`tests/f14_exact_viewer_restore_recheck_plan.cpp` compiles the original protocol with this new PURE header. It checks exact F14-attributed record creation, explicit retirement, feature OFF and global restore freeze, stale/relogged session, missing creature, already relinked **server-only** evidence, wrong key/incarnation and existing debt preserved. It then exhausts **65,536 live-fact combinations** (normal human session, viewer/creature present, map ownership, exact GUID/epoch/incarnation and all map/instance keys, plus F14 feature and era variations), in both C++17 and C++20 with GCC/Clang normal/NDEBUG = **eight** modes. Every allowed result remains a *candidate normal Core visibility pass*, never client confirmation.

The CI source-diff guard permits only five reviewable files (new PURE header, model, workflow, this note, Custom Modifications Register), rejects modifications to original Claude #85/#94/#103 or active IP runtime/config/SQL and verifies all paid/F14 defaults OFF. A successful source run is NOT a new full Worldserver compile. Earlier pinned Worldserver-native [IP #113 Actions run 42264907312](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/42264907312) already **SUCCESS** for its unchanged negative-only Core overlay; the new header is unreferenced/unwired so it cannot by itself prove integration.

## Before any future real concealment

First fully verify selected **current headless Blood Elf/Draenei RNDBOT** eligibility, normal-account human Vanilla viewing rights and complete independent group/quest/tap/loot/pet/DoT/combat/assist rights, exact creature lifespan, map thread, and robust previously-hidden record enumerator. Implement a concrete current-map per-pair restoration process through the existing early corpse-ready and combat-stop handoffs; then test **F01–F20** in a real 3.3.5a client using one owner-approved combined offline test candidate after installed source/config/DB/SQL/DBC/client inventory and recoverable backups. Owner decides any return-to-visibility/reconnect reconciliation policy; no automatic merge or feature enablement.

**Rollback:** reject/close the DRAFT and discard this unreferenced source header + model/docs. No live code, client, host, SQL or data change occurred.

**Cross-project records:** [F14 issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9) and [combined release readiness #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
