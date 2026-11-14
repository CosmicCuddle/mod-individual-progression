# F14 existing AllMapScript::OnMapUpdate frame observer — no gameplay changes

**Owner:** CosmicCuddle. **Maintainer:** Server-Side-Maintenance CS01-IP. **6 November 2026.**
**Status:** isolated DRAFT, opt-in **compile-only observer**, DEFAULT ABSENT/OFF, no F14 positive concealment.

## Purpose and exact pinned Core source

The map-owner question from [#119](https://github.com/CosmicCuddle/mod-individual-progression/pull/119) cannot be answered by the old #108 `Map::Update` thread-local marker alone. A real map callback provides at least a **specific source of execution**. Original pinned Grim Core `0d754d1921261dfd939f91060ebc022eae280f71` already implements:

- `Map::Update` calls `sScriptMgr->OnMapUpdate(this,t_diff)` for each executing map.
- `AllMapScript.h` defines `ALLMAPHOOK_ON_MAP_UPDATE` and `virtual void OnMapUpdate(Map*,uint32)`.
- `AllMapScript.cpp` dispatches this hook with `CALL_ENABLED_HOOKS(AllMapScript,...)`; the same function separately calls selected world/dungeon/battleground map scripts.
- Original #108 disposable Core diagnostic exposes `Map::IsWithinF14MapUpdateInvocation()` (thread-local exact map pointer only during `Map::Update`). Original #110 prevents **new** hypothetical F14 concealment outside that frame.

This native hook is a better place to **observe update-frame provenance** than an arbitrary queued callback or a late `Map::DelayedUpdate`. **It still does not prove exclusive map ownership or safety to touch live Players/Creatures**. Other Core subsystems, logout/session code and delayed-removal coordinator may access map data outside this exact callback.

## New optional implementation

A single guarded addition to existing `src/IndividualProgressionF14LifecycleScript.cpp` defines `IndividualProgressionF14MapUpdateFrameObserver : AllMapScript` with **only** `ALLMAPHOOK_ON_MAP_UPDATE`, and registers it only when **`COSMIC_IP_F14_MAP_UPDATE_FRAME_OBSERVER >= 1`** is explicitly defined at compile time. Stock/unmodified module builds never register it. No distributed config option enables it.

Within `OnMapUpdate(Map*,uint32)` the observer checks the Core exact current `IsWithinF14MapUpdateInvocation()` signal; it counts accepted/rejected callback calls and logs **only the first aggregate occurrence** of either, without player GUIDs, names, IDs, map IDs, session keys or pointers retained. There are **NO** player/creature lookups, visibility calls, raw pointers beyond the synchronous parameter, queue work, database access, client packets, feature activation, positive hide, prior-hidden snapshot or ledger mutation. It is deliberately not a restoration executor.

The original #85/#94/#103 restoration components, #116 exact viewer recheck, #117 snapshot, #118 stale check and #119 map timing audit remain preserved. All F14 and paid feature defaults remain OFF and the original Claude #94 behavior is unchanged.

## Verification

The source regression tests pin exact Grim `AllMapScript.h/.cpp` + `Map.cpp` and assert this existing native callback is used with a **matching compile opt-in guard**. Mutation cases reject missing callback, guard, registration, frame predicate or a new visibility action. A native **disposable** Worldserver job will attempt to compile the observer with **`-DCOSMIC_IP_F14_MAP_UPDATE_FRAME_OBSERVER=1`** on top of the exact prior green #113 Core/IP/PB/Naxx/Services pins and Core overlay. Its symbol check must prove the method was actually emitted, not simply discarded by preprocessor, while also building a stock-code/off source variant where possible. **Native success means compiled C++ only, NOT runtime hook invocation, player display, exclusive map ownership or client ACK.**

The new observer is limited to build-time exploratory evidence. A proper next change must design a **single genuinely serialized map-context mechanism** and safe per-pair callback with live current session+incarnation proof and all human independent combat/group/quest/loot/pet/DoT rights. Nothing may use `OnMapUpdate` to finish an old hide debt just because it was invoked. The original #115 corpse-ready immediate visibility seam may still be necessary for short/zero-lifetime corpses.

**Release still BLOCKED:** installed source/config/DB/schema/DBC/client inventory and recovery-tested backups, selected positively verified headless BE/Draenei RandomBot-only actor, owner-approved no-object-ACK completion/reconnect policy and real WoW 3.3.5a F01–F20 client tests. No feature activation, merge, host, database or client changes.

**Rollback:** reject the DRAFT and remove the observer/CI/docs; default builds have no observer code registered and there is no live deployment.
