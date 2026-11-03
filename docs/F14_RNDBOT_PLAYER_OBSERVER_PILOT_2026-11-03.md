# F14 — Verified RandomBot player-observer hook (development, OFF)

**Owner:** CosmicCuddle. **Implementer:** Server-Side-Maintenance CS01. **Status:** unmerged DRAFT source, NO owner-host installation, no config enabling, no actual client/Worldserver gameplay. **Core source baseline:** `Grimfeather/azerothcore-wotlk@8daffdabe9202c228e45d833c9a1d399597a15cd`. **Parent:** IP DRAFT #50 `9b64bf1e1d624c3d02b02a668aa2f7207d6d097b`.

## Exactly what this new source contributes

1. Adds a compile contract to the *disposable Core patch stored under IP tests*. It **does not** modify an installed Core or any owner-controlled Core fork. When patched, Core can dispatch the optional WorldObjectScript hook, which still defaults to ALLOW.
2. Adds an **experimental OFF-by-default** IP config `IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0`. If the Core seam is missing or the Playerbots RandomBot identity header is unavailable, the configured value is forced OFF.
3. Reuses the existing login-verified, mutex-protected RandomBot identity cache. Only the established login thread calls the mutable Playerbots `IsRandomBot` API, and only after confirming a bot account, race 10/11 and a headless session. Cache is cleared on next login and logout. This lookup must **not** be moved into a visibility worker.
4. Registers a `WorldObjectScript` **only** when the optional Core macro is available. Its player-only callback defaults to ALLOW. With the IP flag deliberately set and positive existing identity, it would veto visibility of the specific headless Blood Elf/Draenei RandomBot **only for ordinary non-headless, non-excluded human observers whose effective personal progression remains below stage 8**. Human Blood Elves/Draenei, other unverified bots, Altbots, GM observers and all creatures are not hidden by this callback.
5. Adds source-level regression checks and rewires the *disposable* CI Worldserver to compile the changed IP source alongside pinned historical Core, Playerbots, Naxx #292 and Character Services #16. **A passing compile must not be represented as a game test.**

## Newly confirmed WHO bypass and hard-disable (Part 22 source audit)

Historical Grimfeather Core [`MiscHandler.cpp` lines 296–310](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Handlers/MiscHandler.cpp#L296-L310) processes `/who` using `sWhoListCacheMgr->GetWhoList()`, `target.IsVisible()` and security; it does **not** invoke the new `OnWorldObjectCanBeSeenByPlayer` callback or `Player::IsVisibleGloballyFor` for normally visible cached players. Therefore simply enabling the pilot could hide a bot in-world while still listing it in `/who` — **unacceptable under CosmicCuddle's F14 rule**.

**Safety correction now in source:** the option is forcibly reset to OFF unless **both** `COSMIC_F14_OBSERVER_HOOK_VERSION` and a future `COSMIC_F14_WHO_FILTER_HOOK_VERSION` are defined by reviewed compatible Core code. The current five-file Core patch advertises **only the observer hook**; consequently, this draft's runtime option **cannot activate**, even if the user sets it to 1. The new player-only callback remains available for compile/link verification and future integration, but it denies nothing while the second contract is absent. A future WHO hook must use an authentic positive cached RandomBot GUID identity, preserve GM/human exceptions, and avoid unsafe live-target dereferences from the cached WHO loop.

## Critical remaining safety gates

- **Global visibility and `/who`:** WHO code and all player-listing paths have not been independently proved to use the same new per-viewer veto in every case. A world-only hide with inconsistent `/who` would violate the owner’s F14 policy.
- **Immediate refresh:** Login, late join, target switch, logout, tier change, server reload and movement must trigger correct per-observer updates; this is not wired. A source callback alone cannot guarantee the client receives an immediate destroy/create update.
- **Creature rule:** The owner's active-attacker-only hide, immediate return to ordinary corpse on death, no invisible ghost hit, loot/quest rights and safe threat/AI handling remain **unimplemented**. Pure F14 combat model #47 is not a runtime callback.
- **Bot initialization:** This hook doesn't grant stage 8, protect a bot from Playerbots quest reset, change bot group rules or rewrite any quest, paid receipt or account entitlement. Playerbots keep-list #49 remains an independent default-empty proposal.
- **Performance:** The new callback checks the cache cheaply first; effective-stage computation for positively verified targets still needs profiling with many nearby bots and multiple viewers.
- **Native CI and source tests:** Require exact green run at this new head before claiming successful compile. Independently review header compatibility, script registry, complete affected call graph and GM/headless cases.
- **Release:** Verify installed source revisions, module set and custom config; obtain restorable character DB/config/binary backups; perform targeted multi-client gameplay/regression tests in staging; obtain explicit CosmicCuddle approval before any production installation or feature activation.

**Rollback now:** discard the unmerged development branch and its isolated disposable patch. No live server changes or character data to undo.

**Coordination:** [Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [Issue #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4), [Core/F14 patch #50](https://github.com/CosmicCuddle/mod-individual-progression/pull/50), [combat policy #47](https://github.com/CosmicCuddle/mod-individual-progression/pull/47).
