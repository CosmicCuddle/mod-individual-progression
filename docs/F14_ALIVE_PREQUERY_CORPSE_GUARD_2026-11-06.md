# F14 Alive-only NEW suppression negative gate — fail visible, OFF

**Owner:** CosmicCuddle. **Maintenance lead:** CS01-IP.
**Date:** 6 November 2026.
**Status:** isolated stacked DRAFT; CI-only disposable pinned Core, not installed, not a positive hiding policy.

## Why this is necessary

The exact Grim Core `Unit.h` at `0d754d1921261dfd939f91060ebc022eae280f71` defines `IsAlive()` as **`m_deathState == DeathState::Alive`**. This excludes `JustDied`, `Corpse`, `Dead` and `JustRespawned`. The previously native-green [IP #112](https://github.com/CosmicCuddle/mod-individual-progression/pull/112) stack already enforces negative-only same-map/current `Map::Update` and nonheadless non-GM viewer vetoes, but without this additional gate a future accidentally permissive positive policy could **begin new F14 suppression against an already dead creature**, possibly leaving its corpse unselectable or invisible.

**This child prevents starting NEW concealment when the NPC is not Alive. It does NOT itself make an already hidden corpse reappear.** Existing per-viewer hidden-pair restore remains required and is currently UNWIRED. The exact source trace shows pinned `Creature::Update` dispatches `Unit::Update` for Corpse but not Dead; a queued `OnUnitUpdate` refresh may have no next tick for a short corpse. The `IsAlive` gate must not be presented as fixing that existing debt or client observation.

## Exact reviewable proposed change

Stacks on [IP #112](https://github.com/CosmicCuddle/mod-individual-progression/pull/112) exact `221d7b65c9d1721be9ec936fe16ae38afb9e8f9b`, whose full pinned native Worldserver [Actions run 42252623462](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/42252623462) is verified SUCCESS. New script `tests/core_patches/apply_f14_alive_prequery_corpse_veto.py` modifies **only the disposable pinned Core** `src/server/game/Entities/Player/PlayerUpdates.cpp` and only inside the inherited F14-specific `f14PositiveSuppression` predicate in BOTH direct and batched `Player::UpdateVisibilityOf`.

The added `target->ToCreature()->IsAlive() &&` short-circuits **after** #110 map-frame and #111 nonheadless non-GM viewer checks, **before** `ShouldF14SuppressCreatureForViewer`, original #99–#102 human rights negative veto, #107 pre-link callback or any link change. Full exact two-path source contracts and exact original Unit.h `IsAlive()` implementation are checked *before* one-file mutation; unknown/changed Core source fails without writing. No Core `CanSeeOrDetect` or ordinary visibility behaviour, no attacker ID, quest/tap/loot/auras/pet rights, hook wiring, SQL, config, DB, DBC, client or IP runtime code is changed.

## Test requirements and confidence boundary

1. Python fail-atomic tests apply the original #111 source overlay to a disposable fixture, then this patch; verify exactly two IsAlive() checks and placement **before** future positive callback, rights and link mutation. Missing Unit.h exact death semantics, broken direct path, duplicate application must leave Core file byte-identical.
2. A pure C++ decision model enumerates **20,480** direct/batched boolean × death-state cases per compiler mode. With the existing absent/default-FALSE provider, all decisions remain FALSE; a hypothetical TRUE policy must not even be queried on any of four non-Alive states. The model **preserves** an already-existing restore debt, rather than falsely clearing it.
3. Source-only CI runs the fixture contracts and model in eight GCC/Clang/C++17/C++20 normal/NDEBUG variants; checks stay active with NDEBUG. A separate native pinned full Worldserver compile/link is required before any claim of compatibility with the full Core/IP/Playerbots/Naxx/Services tuple. Native PASS still does **not** prove a running server or 3.3.5a client's corpse sparkle/targetability/restore.
4. The [F14 client F01–F20 matrix](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/blob/main/release-gates/2026-11-06-f14-client-acceptance-matrix.md) F07/F08/F09/F17/F18/F19 remain **NOT RUN**, including short/zero corpse lifetime.

## Critical remaining safety gates

Selected **positively verified** current headless Blood Elf/Draenei RandomBot-only source, never a human or Altbot player; complete per-viewer normal-human Vanilla era, quest/tap/loot/pet/guardian/DoT/assist/combat rights; actual same owning-map safety vs a synchronous map-frame diagnostic; **prior-hidden exact per-viewer session+creature-incarnation pair** and timely restore with 3.3.5a client rendering and targeting. Claude's original [#94](https://github.com/CosmicCuddle/mod-individual-progression/pull/94) and follow-up [#103](https://github.com/CosmicCuddle/mod-individual-progression/pull/103) remain preserved, PURE, and unwired. No F14 positive listener is registered; paid and F14 feature flags default OFF.

**Release:** this proposal alone does not solve the hide/restore system and does not justify installation. The single combined owner-approved offline test still needs real host source/config/SQL/DBC/client inventory, proven restorable backups, all safe integrations and explicit CosmicCuddle approval.

**Rollback:** close this isolated DRAFT and discard its disposable CI-only Core overlay; there are **no owner-host files or character data** to roll back.

**Coordination:** [F14 #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [combined readiness #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
