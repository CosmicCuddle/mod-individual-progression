# F14 bot logout: map-thread creature visibility-refresh handoff

**Owner:** CosmicCuddle. **Maintainer:** CS01 Maintenance.
**Status:** DRAFT, default-OFF experimental source; no source promotion, production deployment, SQL/DBC/client changes or gameplay proof.
**Parent:** [IP #82](https://github.com/CosmicCuddle/mod-individual-progression/pull/82) exact `2df1eadb1c3f3aa12f76eaa357e17487792f8077`, which implemented the previous one-shot post-event request on top of Claude #78. **Core requirement:** disposable [#77 event refresh hook](https://github.com/CosmicCuddle/mod-individual-progression/pull/77).

## What is implemented

Owner/logout identity invalidation in `ExpireF14LifecycleActor` previously invalidated participant rosters but did not cause `AttackStop` or a visibility update for each affected NPC. This new code carries **only copied `CreatureKey` (map ID, instance ID, raw GUID) and registry incarnation token** into a bounded queue.

- `IndividualProgressionF14LifecycleRegistry.h::ExpireActor` accepts an optional, synchronous value-only callback. Old 1-argument call sites remain valid. On a matching verified attacker or verified minion-owner epoch, the callback is called **before** the roster is invalidated. It does not touch map objects or query Playerbots.
- `IndividualProgressionF14LogoutRefreshQueue.h` is a pure, fixed-capacity queue (default 4,096). It coalesces duplicate keys, rejects mismatched incarnation tokens, and permanently poisons itself instead of evicting requests when full. Reset is explicit. The queue never holds `Creature*`, `Unit*`, `Map*`, `Player*` or any WorldPacket.
- Under **`COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT=1` only**, the lifecycle script records those value-only requests under its existing mutex on logout. It registers Core's `UNITHOOK_ON_UNIT_UPDATE`; only when an atomic pending flag is set does a creature on its own map thread match its own current key and stamp. If matched, it marks the existing one-shot `refreshPending` flag, releases the mutex, asks `ScriptMgr::ShouldF14RefreshCreatureVisibility`, then calls `creature->UpdateObjectVisibility(true)` only on positive response. **No visibility traversal happens under the registry mutex.**
- Removing a creature from world discards the queued key under the same lock; config/lifecycle registry reset clears all pending values. No pointer is kept across events. A stale GUID, replaced incarnation, out-of-map creature, absent callback or queue poison does not authorize anything.
- All historical F14 paid/group/progression flags stay OFF. `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION` (full-ready macro) is NOT defined. No actual creature denial/WHO hide is implemented.

## Safety and current limits

The code requests a server-side nearby-object visibility PASS, **not** a client acknowledgement. It is still impossible to enable concealment responsibly: a real per-observer **previously hidden** ledger and guaranteed global fail-visible restore on cache/queue overflow, config OFF, area unload, failed map update and target rights changes are missing. The queue's permanent poison avoids unbounded memory but **does not by itself restore already hidden client links**. Therefore NEVER promote this experiment to active concealment.

On logout, `OnPlayerLogout` may not be the creature's owning map thread. That is precisely why the logout hook captures value-only keys and the NPC itself consumes its request in `OnUnitUpdate`; no cross-thread Core map object or pointer is dereferenced. A creature that is unloaded or never ticks cannot deliver this update itself; client removal/visibility in those cases must be separately proven in the eventual integrated test.

**No gameplay acceptance:** human target/tap/quest/loot/DoT/heal/group rights; selected BE/Draenei bot and owned minion; two bots then logout; bot disappears without AttackStop; corpse/respawn; off-map unload; reused GUID/instance; queued update vs config OFF; client packet/redraw still require tests and owner approval. No special client patch/DBC data or SQL added.

## Files and test state

- Code: `src/IndividualProgressionF14LifecycleRegistry.h` (optional value callback), `src/IndividualProgressionF14LifecycleScript.cpp` (experimental queue producer and map-thread consumer), new pure `src/IndividualProgressionF14LogoutRefreshQueue.h`.
- Tests: `tests/f14_logout_refresh_queue.cpp`, `tests/test_ip_f14_logout_mapthread_source.py`, and one conservative assertion update to `tests/test_ip_f14_postevent_reveal_request_source.py`. Prior #82 consumer remains unchanged except the optional map-thread refresh call in the new additional method.
- Workflow: one isolated, exact-scope CI with pinned newest Grim `0d754d19` Core, #56/#67/#76/#77 disposable Core patch chain, IP source, unmodified Playerbots `e88cc409`, Naxx #292 `51d0611`, Services #16 `b5d372e`. Native GCC/Clang policy/unit check plus full Worldserver build/link. **Worldserver never launched.**
- **Build and test status:** pending original hosted CI results; do not claim any native pass until recorded.
- **Rollback:** close this DRAFT / remove experimental source in a reviewed future PR. No installed host changes, SQL migrations, DB edits, client changes or player state modifications to undo. Production would require the owner's signed-off integrated release plan and backups.
