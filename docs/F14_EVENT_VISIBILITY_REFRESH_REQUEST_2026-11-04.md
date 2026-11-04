# F14 event-driven creature visibility request — source-only Core prototype

**Owner:** CosmicCuddle. **Maintenance:** CS01-IP Core integration lane.
**Branch:** `research/cs01-f14-event-driven-visibility-request-20261104` (child of [PR #76](https://github.com/CosmicCuddle/mod-individual-progression/pull/76)).
**Status:** DRAFT; no host, SQL, configuration, executable-fork or player data modified. **No creature hiding** is enabled.

## What actual code this adds

The new disposable Core patch `tests/core_patches/f14_event_visibility_refresh_request_prototype.patch` applies only **after** the original pinned F14 #56 attack/corpse/respawn event bridge, #67 Core after-pass, and #76 per-viewer link observer. It adds a concrete hook into the **actual Core event paths**, while protecting the F14 registry mutex:

- A `UnitScript::OnF14ShouldRefreshCreatureVisibility(Creature const*)` virtual method returns **default FALSE**. `ScriptMgr::ShouldF14RefreshCreatureVisibility` aggregates opt-in positive requests with the existing Core `CALL_ENABLED_BOOLEAN_HOOKS_WITH_DEFAULT_FALSE` API. **No existing IP module overrides this method.** As delivered it never requests a visibility grid walk or changes client visibility.
- After the original `OnF14AttackTargetTransition` bookkeeping callback **returns**, the Core can revisit the **previous** creature target (reappearing after target switch) and the **new** creature target, if and ONLY if a future separately reviewed script affirmatively requests refresh. Both target GUIDs are captured **before** calling the lifecycle script; the Core re-resolves them by GUID through the current map before using live pointers. This prevents dereferencing a potentially stale victim pointer after a script callback.
- After original `OnF14CreatureCorpseReady` and `OnF14CreatureRespawned` callbacks return, the Core checks the same default-false request hook and, only if explicitly requested and still in-world, runs `Creature::UpdateObjectVisibility(true)` on that map thread. This gives a tested future route for restoring visibility on death/corpse and reevaluating it on respawn.
- The request path rejects a missing map, noncreature GUID, removed/off-world creature and **pet**. It uses the existing pinned WorldObject/Unit visibility notifier, which checks nearby viewer links. No target or Player pointers are retained across frames, and the F14 lifecycle mutex is **never held during visibility** by this Core route.

## What this does NOT do

No viewer-specific concealment decision is implemented. No observer or creature rights proof is granted; threat/tap/loot/quest/pet/DoT/spell and group-rights conditions from the F14 model are still missing or conservatively false. There is no automatic same-event request from the IP module: the new hook is **default FALSE** and has no current positive consumer. It cannot be presented as a completed visibility refresh integration.

Even a future triggered `UpdateObjectVisibility(true)` means the server **visited nearby viewers** and may have sent object updates. It is NOT a client packet acknowledgement or proof a WoW 3.3.5a client has rendered the hide/restore transition. For batched visibility it may still queue packet data. The `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION` macro is deliberately **not defined** by this patch; current experimental F14 and paid expansion flags remain OFF.

The source code cannot by itself prove stable event ordering across all GameObjects, creature removal, teleport, map/unload transitions, a player's spell/aura impact, or all observer quest loot rights. A validated source consumer must first provide a complete current creature/viewer authorization proof, only then return true for a transition, and confirm the client-facing behavior in an owner-approved test environment.

## Tests / preservation / rollback

The stored patch encodes four intentional empty diff-context lines as `@F14_BLANK_CONTEXT@`. CI checks that exact count, then restores them to real unified-diff context before `git apply --check`; this preserves strict repository whitespace validation without changing the resulting Core C++ code.\n\nThe workflow checks exact source ancestry, exact four-file child scope, applies this patch to a **disposable newest pinned Grimfeather Core** already patched by #56/#67/#76, uses Core source tests for the per-viewer path, compiles and links actual Worldserver with unchanged Playerbots, Naxx Core and Character Services, and **never starts the server**. Its source tests separately assert default-false behavior, prior capture of target GUIDs, map re-resolution and post-callback order for target switches, attack stop, corpse and respawn.

Rollback is to discard this isolated CI-only PR and the disposable test patch. No release source/config/SQL/DBC/characters were modified. CosmicCuddle's **single combined game test** remains blocked until all actual F14 hiding, safety, paid, Playerbots and other module features are completed and approved.
