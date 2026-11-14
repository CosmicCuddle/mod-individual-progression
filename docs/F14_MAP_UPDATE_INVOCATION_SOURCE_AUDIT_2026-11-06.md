# F14 strict current Map::Update invocation evidence — isolated diagnostic, OFF
**Owner:** CosmicCuddle
**Maintainer:** Server-Side-Maintenance CS01-IP
**Date:** 6 November 2026
**Status:** stacked DRAFT source experiment only; no protected merge, deployment, real build or 3.3.5a client acceptance.
## Why this matters
In the exact disposable Grim Core used by parent [IP #107](https://github.com/CosmicCuddle/mod-individual-progression/pull/107),
`Grimfeather/azerothcore-wotlk@0d754d1921261dfd939f91060ebc022eae280f71`:
- `Map::Update` processes map players, objects and delayed visibility. However, `Map::AddPlayerToMap` also calls `player->UpdateObjectVisibility(false)` outside the shown `Map::Update` body. Do not assume all paths to `Player::UpdateVisibilityOf` are a map tick.
- `MapUpdater::WorkerThread` executes queued `MapUpdateRequest` objects; a worker may execute multiple different map requests. A remembered `std::thread::id` is not a stable map identity.
- Parent #107's source-specific F14 callback runs before *direct and batched per-viewer link mutation*, and its IP observer checks the same live map pointer plus current human epoch and creature incarnation. That still does not demonstrate a current `Map::Update` call stack, exclusive map ownership, full real-player rights, previous hidden pair, or client ACK.
## Isolated changes
1. `tests/core_patches/apply_f14_map_update_invocation_diagnostic.py` applies an exact-anchor, pre-validated two-file overlay **ONLY in a disposable pinned Core checkout**: `Map.h` exposes `IsWithinF14MapUpdateInvocation()`; `Map.cpp` uses a thread-local current Map pointer and a non-copyable RAII guard at `Map::Update` entry. Nested calls restore the previous pointer on exit/unwind. This is synchronous *invocation* evidence, **not exclusive map ownership** or a general thread identity API. Failure to match pinned source anchors leaves both files unchanged.
2. The existing opt-in-only `src/IndividualProgressionF14LifecycleScript.cpp` observer rejects current-source snapshots if there is a matching map pointer but no current `Map::Update` frame. It records a first generic rejection, stores no personal identifiers or per-viewer records, performs no force-visible or hide operation, and leaves previous source identity checks intact. All ordinary builds lack the `COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT=1` subscriber.
3. `tests/test_ip_f14_map_update_invocation_source.py` checks RAII entry/exit, exact two-file patch behavior, double-apply refusal, mismatched-second-anchor pre-validation, the unchanged OFF/unwired observer, and (when supplied) exact pinned Core source.
4. The scoped source-only GitHub workflow checks the isolated diff, pins exact Grim Core, runs the Python tests and existing pure human viewer/creature identity models. **It does NOT compile/link Worldserver or run it**, and says so explicitly.
This draft is a **diagnostic narrowing only**. The original Claude #94 restoration ledger/protocol, #103 send-attempt adapter and parent #107 pre-link Core patch are unchanged. The new marker does not feed the ledger or register a positive F14 policy. It must not be used as a standalone release gate.
## What remains unproven
- The map frame method establishes the **same thread is currently executing this map's Update function**, not exclusivity if two map workers could update the same Map concurrently; investigate scheduling and MapUpdater synchronization before ever using it as authority.
- Source references to a Map within an entry/logout/teleport callback may be valid without being in a `Map::Update` frame; those observations simply fail this diagnostic rather than permitting hide.
- Complete player and group combat/quest/kill/loot/tap/assist/pet/guardian/DoT rights, selected headless Blood Elf/Draenei RandomBot-only eligibility, prior-hidden ledger linkage, server-to-client OUT/CREATE delivery and actual client render/targetability/reappearance on stop, death/corpse, respawn, logout, map change, overflow and feature OFF remain absent.
- Actual installed Core/IP/Playerbots/Naxx/Character Services versions, custom edits/config/DB/DBC/client, restorable backups, paid receipt/expansion rules, owner's combined-test approval and gameplay acceptance remain unknown/pending.
## Safe next action and rollback
1. Check **parent #107 exact native build** before claiming successful Core hook compilation.
2. Check this source-only workflow. A source PASS does not validate native C++ interoperability.
3. If retained, add a **separate full pinned native integration** to test both the disposable Map overlay and macro-enabled IP observer with the exact Core/IP/Playerbots/Naxx/Services tuple. Then investigate exclusive-map scheduling separately; no positive hide until complete human rights and 3.3.5a restoration.
4. Reject/close this isolated DRAFT to discard the experiment. **No live code, DB, SQL, effective config, client/DBC, player data or host has been changed.** Do not merge or deploy without CosmicCuddle's separate approval.
**Coordination:** [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9#issuecomment-6683293567), [combined release gate #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).