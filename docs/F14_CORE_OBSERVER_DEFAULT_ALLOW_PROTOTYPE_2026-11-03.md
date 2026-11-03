# F14 Core per-observer visibility hook — default-ALLOW prototype (NO GAMEPLAY)

**Owner:** CosmicCuddle. **Maintenance:** CS01. **Decision:** Source-only and disposable CI investigation/implementation candidate, NOT an approved Core fork, production patch, feature activation or in-game visibility test. Grok on owner-confirmed break; any review by CS01 is not independent sign-off.

**Branch:** `research/cs01-f14-core-observer-hook-prototype-20261103` stacked on [IP DRAFT #47](https://github.com/CosmicCuddle/mod-individual-progression/pull/47) exact parent `ce69a6891470693a471896b999c4fc97424f8676`. **Historical Core:** `Grimfeather/azerothcore-wotlk@8daffdabe9202c228e45d833c9a1d399597a15cd`. These are developer/CI sources, not proof of the installed host.

## The confirmed missing seam

Core [`WorldObject::CanSeeOrDetect`](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Entities/Object/Object.cpp#L1812-L1860) tests `CanNeverSee`, then **early success** for `IsAlwaysVisibleFor` / `CanAlwaysSee`, and later handles individual creature AI `CanBeSeen`. There is no generic scripted **viewer-specific veto** for all ordinary visible creatures and players. Overriding NPC AI globally would risk custom scripts and is NOT proposed.

Core [`Player::IsVisibleGloballyFor`](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Entities/Player/Player.cpp#L11668-L11692) returns TRUE for normally visible players **before** its existing `OnPlayerNotVisibleGloballyFor` module hook, so relying on that hook alone cannot hide verified Blood Elf/Draenei rndbots from ordinary `/who`/global visibility. Existing normal-player self/GM logic must be preserved.

## Minimal five-file candidate patch — not installed

**Source artifact:** `tests/core_patches/f14_observer_visibility_default_allow_prototype.patch` applies only to the *disposable test runner's* exact historical Core (five files, eight hunks).

1. `src/server/game/Scripting/ScriptDefines/WorldObjectScript.h`: add `WORLDOBJECTHOOK_ON_WORLD_OBJECT_CAN_BE_SEEN_BY_PLAYER` *after* all existing numeric hooks and before `END`, and default method `virtual bool OnWorldObjectCanBeSeenByPlayer(Player const*,WorldObject const*) { return true; }`.
2. `src/server/game/Scripting/ScriptMgr.h` and `.../WorldObjectScript.cpp`: dispatch via existing `CALL_ENABLED_BOOLEAN_HOOKS` and deny only on explicit future module veto. **With no vetoing hook, return TRUE.** Existing hook ordinals remain unchanged.
3. `src/server/game/Entities/Object/Object.cpp`: call the observer hook only when a Player observer has a **non-null, ordinary-player WorldSession**, and only for Player/Creature targets, after `CanNeverSee` but BEFORE the early always-visible fast path. Headless bots and GM observers bypass this new hook. The patch explicitly includes `AccountMgr.h` and `WorldSession.h`. This is only an observation gate, not a threat, attack, damage, loot or phase modification.
4. `src/server/game/Entities/Player/Player.cpp`: apply the same hook only when the observer has a **non-null ordinary-player WorldSession** in `IsVisibleGloballyFor`, **after self exemption but BEFORE `IsVisible()`**. Preserve the original fast true path for headless bots (whose session may be absent), GM observer bypass and existing GM visibility behavior. Neither path identifies a RandomBot/race on its own.

**Crucial limitation:** there is NO override in the IP module for this new hook; no creature/player is concealed. This patch does not create any F14 configuration key, perform WorldObject visibility refresh or alter saved stages. Therefore compiling it is a **Core seam compatibility proof**, NOT a playable F14 visibility system. F14 remains **BLOCKED_CORE_PB_HOOK**.

## Mandatory follow-on development before F14 can be used

1. Implement an explicit **default-OFF** IP-owned observer predicate, with **positively verified** rndbot identity from the owning Playerbots context, Blood Elf/Draenei race, observer effective raw/paid progression, exclusions for human players and Altbots, and no cache guesses. This module must not call Playerbots data across unsafe worker threads.
2. For creature targets, use already-authored [IP #47 pure active-attack policy](https://github.com/CosmicCuddle/mod-individual-progression/pull/47) with a per-living-instance identity token and current generation. Hide only a **living** creature actively attacked by a qualifying already-hidden rndbot; if a real player is threatening, attacking, looting, assisting, nearby with unsafe aggro or otherwise at risk, **show the creature**. `inCombat` or a stale threat flag alone is insufficient.
3. Build the **invalidation and republish** path on the correct map thread for attack start, attack stop, target switch, bot logout/reset and **creature death**. On death **show the corpse immediately** with normal sparkle, quest loot, group and skinning rights. A `CanSeeOrDetect` hook alone **does not send an object-visibility update**, so a pure compiled policy never proves client display.
4. Audit other visibility surfaces (`/who`, friends, guild, inspect, targeting, combat packet visibility), confirm no ghost-hit damage and do NOT affect human BE/Draenei, Altbots or TBC+ observers. Demonstrate the creature un-hides on attack stop even if still in combat, and on death even if attacker flags are stale.
5. Because `WorldObjectScript` constructor enables all hooks for scripts with no explicit hook list, a new hook can have a **per-player-object performance cost** even when all scripts default allow. Measure 5/10/40-player/bot combat, and benchmark empty/new hook dispatch before any production decision; an explicit fast-path subscription mechanism may be needed.
6. Keep first in-game release flags OFF and separately resolve [IP #49](https://github.com/CosmicCuddle/mod-individual-progression/pull/49) PB `ResetQuests` earned stage-marker loss. A working visibility hook does not protect stages, and a PB keep-list does not refresh world visibility.

## Automated checks and deployment gate

- `tests/test_ip_f14_core_observer_seam_prototype.py`: source/provenance guard for five exact Core paths, default ALLOW, two bypass-sensitive call sites and **no IP runtime executable changes**.
- `.github/workflows/ip-f14-core-observer-seam-source-prototype-20261103.yml`: historical SHA checkout, five exact original Core file blob checks, `git apply --check`, only five allowed file edits and Python source contracts. Source-only.
- `.github/workflows/ip-f14-core-observer-seam-joint-native-prototype-20261103.yml`: disposable Core + **this no-op hook** + existing two-file Core paid row safety patch + historical Playerbots compatibility edits + pinned IP #47, Naxx #292 and Character Services #16; **actual Worldserver compile/link** to catch API/ABI failures, never launch. Both paid/IP F14 options OFF. A green link cannot establish gameplay, responsiveness, safety or owner-installed compatibility.

**Rollback:** delete/discard this isolated IP draft research PR and its runner-only Core patch. No installed Core or database operation has occurred. Any future deployment would need owner approval, verified actual server source and custom modifications, restorable full backups and a tested revert path. Never merge this proposal merely because CI passes.
