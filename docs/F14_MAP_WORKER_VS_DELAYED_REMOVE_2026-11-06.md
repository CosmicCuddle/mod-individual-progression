# F14 native map phase boundary: scheduled map update vs delayed corpse removal

**Owner:** CosmicCuddle. **Maintainer:** CS01-IP Server-Side-Maintenance. **Date:** 6 November 2026.
**Status:** isolated SOURCE-ONLY stacked DRAFT; F14 and paid expansion default OFF. No client/server execution.

## Confirmed source path at exact installed-candidate pin

Read the same **pinned** Grim Core `Grimfeather/azerothcore-wotlk@0d754d1921261dfd939f91060ebc022eae280f71` compiled in parent native-green #113. The following is source evidence, NOT proof of exclusive map ownership:

1. `World::Update` invokes `sMapMgr->Update(diff)`.
2. `MapMgr::Update` schedules one update for each OUTER map iterator with `m_updater.schedule_update` when worker threads are active, or directly calls `iter->second->Update` otherwise. With workers active, `m_updater.wait()` occurs BEFORE the subsequent, conditional **`iter->second->DelayedUpdate(...)` loop**. Thus `Map::Update` may run on a MapUpdater worker while `Map::DelayedUpdate` is invoked later on the `World::Update`/MapMgr coordinating call stack. A worker is **not** a persistent per-map owner.
3. `MapUpdater::WorkerThread` pops a task and invokes `request->call()`. `MapUpdateRequest::call` invokes `m_map.Update(m_diff,s_diff)` THEN marks the task finished. The worker pool is shared with other maps and LFG requests; there is no proof that any one worker is attached permanently to a given map.
4. `MapInstanced::Update` calls `Map::Update` on its own outer map then separately **schedules child instances** `sMapMgr->GetMapUpdater()->schedule_update(*i->second, t, s_diff)` (or direct update when workers are off). `MapInstanced::DelayedUpdate` separately invokes every child `DelayedUpdate(diff)`, followed by the parent's `Map::DelayedUpdate(diff)`, after the outer wait. Checking only outer map scheduling is insufficient.
5. `Map::Update` calls `UpdateNonPlayerObjects(t_diff)` and `sScriptMgr->OnMapUpdate(this,t_diff)`. `Map::DelayedUpdate` calls `RemoveAllObjectsInRemoveList`, which deletes queued Creature objects through `RemoveFromMap(...,true)`. These are distinct execution phases and original pointers may already have been cleaned up at the later stage.
6. The disposable #108 F14 diagnostic installs a `thread_local Map const*` RAII marker **at the start of `Map::Update` only** and `IsWithinF14MapUpdateInvocation` tests the exact current map pointer. #110 uses it only to fail-VISIBLE before a future positive F14 query. It does **NOT** establish safe access on a later `Map::DelayedUpdate` call, nor exclusivity from unrelated Core accesses.

## Impact on existing exact-pair recheck drafts

- [#115](https://github.com/CosmicCuddle/mod-individual-progression/pull/115) proves the original synchronous `Creature::setDeathState(Corpse)` → optional default-FALSE `ShouldF14RefreshCreatureVisibility` → `WorldObject::UpdateObjectVisibility(true)` path exists *before* immediate corpse removal. **But the creature death transition can also be invoked by code paths not yet covered by an owner-map proof.**
- [#117](https://github.com/CosmicCuddle/mod-individual-progression/pull/117) only copies known exact historical previous F14-hidden viewer/creature session+incarnation records. [#116](https://github.com/CosmicCuddle/mod-individual-progression/pull/116) decides if one matching current normal human/current creature *may be considered* for ordinary Core `CanSeeOrDetect` recheck. [#118](https://github.com/CosmicCuddle/mod-individual-progression/pull/118) rejects any stale snapshot after retirement/relink/config/epoch change; its map-ownership boolean remains caller supplied and **does not establish a real Core authority**.
- A future active handler must obtain an **authoritative, properly synchronized, single-map execution context** over the exact original viewer session and creature incarnation, with #94 protocol lock only for value-copy/validation and ALL locks released before Core's player visibility callbacks; it cannot retain raw `Player*`/`Creature*` across map queues or use #108's frame marker to authorise delayed removal. If an event happens outside that trusted context, **defer safely or keep the previous record owed**, never force a CREATE, clear the ledger, fabricate a client object ACK or start new hiding.
- A successful complete F14 client restoration is still **NOT proved**: source-level CREATE/send attempt from #115, even if a matching receiver is found, cannot establish targetability/loot sparkle or in-client rendering. There is no per-object ACK in WoW 3.3.5a, and a sustainable owner-approved reconciliation policy is still required.

## Review/test scope

New source-only `tests/test_ip_f14_map_worker_delayed_removal_phase_source.py`, CI `.github/workflows/ip-f14-map-worker-delayed-remove-source-20261106.yml`, this note and Custom Modifications Register append. Exactly **four** files relative to #118. It checks exact pinned Core `World.cpp`, `MapMgr.cpp`, `MapUpdater.cpp`, `MapInstanced.cpp`, `Map.cpp` and `Creature.cpp`, plus the existing exact #108 frame-marker and #110 negative-gate prototypes; mutation tests reject missing barrier, altered root/child scheduling, missing delayed cleanup, missing #108 marker or #110 pre-policy veto. It is not a native Worldserver compilation of this branch and does not run a WoW client, database or live host.

**Test outcomes must be verified from Actions before claiming PASS.** No active positive F14 hide provider, paid feature enablement, runtime IP/Core hooks, SQL, DBC or client patches are created.

## Release/rollback

Still BLOCKED: selected headless BE/Draenei RNDBOT-only attacker, complete independent normal-human Vanilla plus Altbot combat/group/quest/tap/loot/DoT/pet/assist rights, exact per-viewer prior-hidden restoration and owned-map dispatch, real 3.3.5a client tests F01–F20, installed custom sources/config/DB/schema/client version inventory, proven restorable backups and CosmicCuddle permission for one coherent complete offline test candidate.

Close/reject this stacked DRAFT to roll back all four review-only files. Nothing on the owner host was changed.

Shared tracking: [F14 issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [combined test readiness #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
