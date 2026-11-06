# F14 original #94 protocol — bounded, read-only enumeration of exact owed restorations

**Owner:** CosmicCuddle. **Prepared:** 6 November 2026 by Server-Side-Maintenance CS01.
**Status:** STACKED DRAFT, source/test-only; unused by live IP/Worldserver. All F14 and paid feature defaults OFF.

## Why this particular change is necessary

Previous [#115](https://github.com/CosmicCuddle/mod-individual-progression/pull/115) established the already-present original corpse-ready → optional default-FALSE immediate refresh → `VisibleChangesNotifier` → conditional `Player::UpdateVisibilityOf` server route. [#116](https://github.com/CosmicCuddle/mod-individual-progression/pull/116) adds a pure **one-pair-at-a-time** conservative selector for a verified normal-human current viewer and exact previously suppressed creature incarnation. However, the original Claude [#94 per-viewer protocol](https://github.com/CosmicCuddle/mod-individual-progression/pull/94) stores its source-attributed suppression records in **private fixed arrays** with **no public enumeration**. A Core callback cannot safely reconstruct those records from generic OUT/CREATE or viewer proximity; that would invent suppression evidence.

The smallest auditable way forward is to add **one new const, value-only, bounded snapshot method** to the original #94 protocol, with its existing suppress/restore logic preserved byte-for-byte. This proposal DOES add an API to an original custom file; it deliberately does **not** alter or silently overwrite any earlier behaviour, entry, ledger capacity, packet/visibility hook, proof rule or callback.

## Scope and exact new API

**One protected original file receives an additive, read-only API:** `src/IndividualProgressionF14ViewerRestoreProtocol.h`. No existing #94 method, constructor, record state, private field, history, #85 ledger or #103 tagged send-attempt bridge is changed.

`Protocol<Capacity>::CopyExactPendingRestores(std::array<ExactPendingRestore, OutputCapacity>& destination, std::size_t& count, std::size_t& requiredCount) const`:
- Scans the existing fixed `_records` array; includes **only** exact previously attributed F14 suppressions with real `UnlinkedByF14` or `CreateWithheldByF14` server stage **and** a genuinely pending restore in the original private `_ledger`. It deliberately queries `_ledger.NeedsRestore` instead of the protocol's global-risk-expanded `NeedsRestore` so an unattributed risk cannot fabricate exact missing pairs.
- Copies each value-only record's **original viewer GUID/login epoch**, creature map/instance/full GUID/incarnation, original suppression stage and latest **server-only** relink/packet-attempt stage. No Core pointers, session pointers, callbacks or client state are stored or generated.
- **Two-pass, no silent truncation:** first counts required entries. If the caller's fixed output array is too small, returns `OutputTooSmallNoCopy`, sets `count=0`, provides `requiredCount`, and does **not change any destination element or the original ledger**. A future caller must treat this as a BLOCKED global restoration / capacity issue, never discard an earlier item.
- On a successful copy returns `KnownPairsCopied`, or `KnownPairsWithUnattributedRisk` if prior protocol risk/ledger poisoning or a ledger/record count discrepancy shows that known exact pairs might be only a subset. The latter still provides recoverable *known* pairs, but **never proves that all hidden clients were found or that hiding is safe**.
- The snapshot cannot remove, reset, unfreeze or acknowledge a single entry, never calls `CompleteRestoreOnlyWithClientProof` or `ClearFrozenAfterGlobalProof`, never sends/receives a packet, never grants positive concealment, never fabricates a 3.3.5a per-object client ACK.

**Composed handoff regression:** `tests/f14_snapshot_to_exact_recheck_no_packet.cpp` links this new original #94 read-only snapshot with the previously tested #116 exact old-human recheck planner. It tests multiple previously F14-hidden human viewers on one NPC, an unrelated NPC not yet revoked, fail-atomic undersized output, wrong current human viewer/old login epoch, headless/Altbot ineligible session, uncertain map owner, wrong instance, an already-server-relinked but client-unacknowledged pair, F14 disabled and later entitlement change, and global unattributed risk that cannot be called an all-clear. Its only possible positive outcome is a **candidate for ordinary Core visibility reevaluation**, never a force CREATE or a packet/client ACK. Both C++ executables run in eight compiler modes.

## Ownership, locking and safety boundaries

The original #94 protocol is not thread-safe. A future caller must obtain one **verified** owning-map or mutex ownership before calling this const method, copy its **bounded value-only** result and release the lock **before** invoking any Core visibility/ScriptMgr or player session callback. Do not assume the #108 synchronous `Map::Update` frame marker itself proves owner-thread exclusivity. An external caller must supply authoritative exact current normal-human nonheadless session and current creature incarnation/instance; the new snapshot only exposes historical data.

The destination should be preallocated to an approved maximum and must never auto-grow or silently evict old pairs. A future partial walk across maps, missing viewer, expired login epoch, recycled creature GUID or `OutputTooSmallNoCopy` is not an excuse to clear debt. Recheck each exact known pair with the separate #116 value-only plan; use Core's ordinary `CanSeeOrDetect` and rights checks only after independent map-thread proof. A creature-wide refresh or stage-2/3 Core link/send attempt is **NOT** the client's rendering/loot/targetability and cannot clear the original record. 3.3.5a does not provide a per-creature object-display ACK. The owner still needs to approve a realistic no-ACK completion/reconnect policy.

## Verification and protected original behaviour

Six review paths only: the additive original #94 API, two pure C++ regressions `tests/f14_original_protocol_readonly_owed_snapshot.cpp`, the strict source workflow `.github/workflows/ip-f14-original-protocol-readonly-snapshot-20261106.yml`, this note, and the Custom Modifications Register entry.

The workflow **compares the new original #94 file with the exact pinned parent and rejects any difference outside the one new additive block**. It asserts no modification to the original #85 ledger/#103 bridge/IP runtime/config/SQL/DBC/client, and that F14/paid flags remain OFF.

The executable constructs synthetic F14-positive original-protocol records *only in test memory* and checks direct/batched source attribution, multiple viewer GUIDs/login epochs, multiple creature keys and incarnations, untouched history, zero-output, undersized output and required counts, no-copy on overflow, server relink still being only server proof, all-record revocation after feature OFF, sticky unattributed risk, and full original ledger capacity freeze with **no eviction**. Runs in eight GCC/Clang C++17/20 regular/NDEBUG modes, with active CHECKs even under NDEBUG. Tests do not launch Worldserver or touch a real client. CI status must be inspected before marking PASS.

## Still blocking a working F14 system

This is a **snapshot of known exact previous hides**, not a runtime implementation. To safely enable the future NPC hiding:
1. A source-authorized selected current headless Blood Elf/Draenei RandomBot-only attacker (never humans, Altbots or pets as an authorising actor), complete normal-human Vanilla quest/tap/loot/pet/DoT/group/assist/combat rights, and verified exclusive map owner.
2. A bounded lock-safe handoff from this read-only snapshot to the #116 exact old-pair recheck, using the existing #56/#77 immediate corpse-ready, combat-stop, relog/map/respawn/feature-OFF pathways without callbacks under locks; includes over-capacity and no-next-tick handling.
3. A separately owner-approved F14 historical debt **completion/reconciliation** design for logout/removed incarnation/client no-ACK reality; no caller may clear a record on simple Core link/send evidence.
4. ONE complete owner-approved offline test with the [F01–F20 WoW 3.3.5a client acceptance cases](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/blob/main/release-gates/2026-11-06-f14-client-acceptance-matrix.md), actual installed source/module/custom-edit/config/SQL/DB/DBC/client inventory and verified restorable backups.

**No implementation merge, deployment, live database/client modifications, positive hide, paid toggle or player-facing release approved.** If review raises a conflict, discard/reject this DRAFT. As no live changes occurred, rollback is closing the PR.

[Coordination F14 #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9) · [combined test gate #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
