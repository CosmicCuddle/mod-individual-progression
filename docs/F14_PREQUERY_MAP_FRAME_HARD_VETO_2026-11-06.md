# F14 fail-visible pre-query Map::Update veto — stacked DRAFT, positive hide OFF

**Owner:** CosmicCuddle
**Maintainer:** Server-Side-Maintenance CS01-IP
**Date:** 6 November 2026
**Status:** isolated unmerged source / native-check development; **no live server changes, no positive hide policy, no deployment approval**.

## Motivation: evidence is not enforcement

[IP #108](https://github.com/CosmicCuddle/mod-individual-progression/pull/108) and [#109](https://github.com/CosmicCuddle/mod-individual-progression/pull/109) established a real compiled observer and disposable pinned-Core function `Map::IsWithinF14MapUpdateInvocation()`. The observer rejects missing-frame snapshots **after** an explicitly F14-positive concealment decision. Because its callback is `void`, rejecting a diagnostic does **NOT** undo or veto that concealment. A future positive hiding provider could otherwise silently create an inconsistent viewer link.

This child adds a narrowly-scoped **actual negative short-circuit** in the existing F14-specific `f14PositiveSuppression` expression **before** consulting any future positive policy callback, before `OnF14BeforeCreatureSuppression`, and before the viewer link/OUT packet path.

## Proposed exact behaviour (both direct and batched Player visibility paths)

- Retain unchanged baseline `CanSeeOrDetect(target, false, true)`, existing pet/other exclusion and all #99–#102 Core human-rights negative vetoes.
- In the **F14-only expression**, require current viewer and creature to both be in world, a non-null viewer map, equal viewer and creature Map pointers, and `viewer->GetMap()->IsWithinF14MapUpdateInvocation()`. C++ short-circuit `&&` skips `ShouldF14SuppressCreatureForViewer` completely when any prerequisite is missing.
- An absent/off-frame map update, player map entry, cross-map or stale object therefore defaults to normal **VISIBLE** Core behaviour, even if a future provider would otherwise return TRUE. This is not a general visibility override, a forced show, or a packet-sending method.
- A frame-present result allows the (currently absent) future F14 policy to be consulted, but **does not authorise hiding by itself**. The existing human-rights veto runs as before after any hypothetically positive callback.
- This guard **does not prove exclusive map ownership**, all real-player/Altbot rights, verified headless BE/Draenei RandomBot-only selection, a historical per-viewer hidden pair, or client reappearance. The policy must remain absent/default FALSE until those other independent gates are proved and owner approves.

## Source and change control

This child stacks on [#109](https://github.com/CosmicCuddle/mod-individual-progression/pull/109) native-green development source. Its exact parent at branch creation is `566c9524f81ca537623a95789568bd8a708a082f`, combining #107 pre-link attribution + #108 Map::Update diagnostics + #109 CI integration. The new patcher `tests/core_patches/apply_f14_prequery_map_frame_hard_veto.py` changes **only the disposable Core** `src/server/game/Entities/Player/PlayerUpdates.cpp` after ensuring the #108 exact Map RAII API exists. It adds the pinned `Map.h` include and the identical short-circuit to direct and batched F14-specific query expressions; refuses malformed, partially patched and repeated sources *before* writing.

The IP runtime source, original Claude #94 restore ledger/protocol, pure #103 send-attempt adapter, previously tested #107 pre-link event, #108 map marker, protected owner Core fork, Playerbots, Naxx, Character Services, all SQL/config/DB/DBC/client/host are **unchanged**.

## Evidence and tests

- Dedicated Python synthetic fixture contracts enforce exact two-path code ordering, **no policy query off-frame**, fail-no-write on a missing #108 Map marker or incomplete second visibility path, and repeated-patch refusal.
- Exhaustive pure C++ diagnostic model enumerates 1,024 boolean scenarios *per visibility path* (2,048 cases), including a hypothetical future TRUE policy. Assert F14 remains default FALSE when no provider is registered, and even a hypothetical TRUE cannot run/approve outside the same-current-frame, same-map, live NPC prerequisite.
- Source-only CI and an independent disposable pinned combined Worldserver native workflow must confirm exact paths, original #99–#102 source vetoes, #107/#108 Core and IP symbols, precise direct/batched query ordering and real Worldserver link.
- The source model is **not actual gameplay**; the pinned native workflow **must not launch** worldserver. Check exact completed CI and final link before claiming pass.

## Remaining blockers and rollback

An exclusive owning-map-thread guarantee, independent full human quest/tap/loot/group/assist/spell/DoT/pet/guardian rights, positively verified selected headless BE/Draenei RNDBOT eligibility (never humans or Altbots), original Claude #94 historical hidden-pair/restore handoff and real 3.3.5a client OUT/CREATE/targetability remain unverified. Need installed Core/IP/Playerbots/Naxx/Services and DB/config/client inventory, backups with tested restore, approved paid-expansion rules, and CosmicCuddle approval for **one coherent combined game test**.

**Rollback:** close/reject this isolated stacked draft; none of its source has been installed or run in a live server. F14 experimental and paid flags remain OFF.

**Coordination:** [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9) and [combined readiness Issue #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
