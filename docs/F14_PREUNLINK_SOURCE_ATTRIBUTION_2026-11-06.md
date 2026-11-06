# F14 source-attributed, pre-viewer-link intent event (DRAFT/OFF)

**Owner:** CosmicCuddle
**Maintainer:** CS01 Server-Side-Maintenance
**Date:** 6 November 2026
**State:** source-only DRAFT / disposable pinned Grim Core patch, **not merged, enabled or deployed**

## Specific gap fixed in development

The prior [#106](https://github.com/CosmicCuddle/mod-individual-progression/pull/106) opt-in `OnF14CreatureSuppressionDecision` observer is **after** the visibility-link branch. Although it can inspect current viewer/creature objects, it cannot honestly claim it inspected the pair before the Core unlinked it. The later #93/#95 per-GUID send-call events lack the historical creature incarnation. Without authoritative pre-link evidence, neither Claude's original [#94](https://github.com/CosmicCuddle/mod-individual-progression/pull/94) ledger nor the [#103](https://github.com/CosmicCuddle/mod-individual-progression/pull/103) pure restoration-evidence adapter should receive a fabricated previous-hide record.

This draft adds a distinct, earlier F14-specific event:

1. A **disposable four-file pinned Core overlay** `tests/core_patches/apply_f14_preunlink_suppression_decision.py`, applied **after existing #86/#93/#95/#99–#102** overlays, inserts `ScriptMgr::OnF14BeforeCreatureSuppression(Player const*,WorldObject const*,bool,bool)` and `WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_BEFORE_SUPPRESSION`. It is called in **both** `Player::UpdateVisibilityOf` forms directly after the existing `f14PositiveSuppression` decision and **before** the viewer's `HaveAtClient` branch, `UnlinkWorldObjectVisibility`, and creation/out-of-range update construction.
2. The call is **only when `f14PositiveSuppression` is TRUE**. The original default-FALSE F14 policy query is unchanged and the module registers **NO positive F14 suppression callback**. Thus the new hook is dormant in the test build too; it cannot make an NPC disappear.
3. The existing opt-in `WorldObjectScript` in compiled IP lifecycle subscribes to this new pre-link event, and shares a **value-only, fail-closed identity inspector** with the older post-decision callback. It reads the current #105 human viewer full GUID/login epoch and an already-stamped #92 creature incarnation in `Creature::CustomData`; it matches the current unpoisoned registry by full creature GUID/map/instance/token under a **try-lock**. There is no new creature stamp or prior-hidden per-viewer record.
4. Diagnostics distinguish **pre-viewer-link** and **post-decision** source observations with separate anonymous atomic counters and first-event generic logs. No GUID/Player pointer/session epoch/creature token is retained, and no packet, ledger, phase, combat, loot or client-visible state is changed. The source remains guarded by the **test-only** `COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT=1` build flag; normal builds register neither subscriber.

## Proof boundaries

The **Core call site is before the two viewer-link mutation branches**, not merely an after-send attempt. This is real source-order provenance useful for a future reviewed source-attributed prior-hidden design. However, it is **not yet** an independent Core owning-map-thread assertion, an approved human combat/quest/loot/group rights audit, a verified selected headless BE/Draenei RandomBot-only positive hiding rule, a prior-hidden ledger entry, or proof of any 3.3.5a client's receipt/rendering/targetability.

The emitted callback is after a *positive decision* but before executing the two link branches; even though source placement proves local ordering, the callback **cannot guarantee a client actually received an OUT-OF-RANGE or rendered a CREATE**. In particular, an initially unlinked object might have no OUT event to send. The original Claude #94 prior-hidden ledger and #103 pure send-attempt protocol remain **unchanged and UNWIRED**. The observer never tries to retire those records.

## Testing, dependencies and rollback

Six review paths: one Core-only overlay, `src/IndividualProgressionF14LifecycleScript.cpp` guarded subscriber/refactor, dedicated Python fixture and source tests, a full CI workflow, this design note and Custom Modifications Register append.

A synthetic four-file Core fixture validates **both batch and direct pre-link ordering**, preservation of the old post-decision callback, exact source tagging, all-or-nothing validation on mismatch and refusal to apply the overlay twice. Actual native CI applies pinned Grim Core `0d754d1921261dfd939f91060ebc022eae280f71`, original #86/#93/#95 and #99–#102 overlays plus the new pre-link patch in a **disposable checkout**; only that CI build compiles the IP observer with `COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT=1`. It then attempts a full actual Worldserver compile/link with historical selected-only Playerbots, Naxx #292 and Character Services #16 and checks the new Core and IP callback symbols. **Do not claim native success until exact-head runner jobs finish.**

**No protected Core/IP merge, host config, DB, SQL, paid feature, Playerbots source, Naxx, Character Services, DBC or 3.3.5a client change.** The old #106 callback is preserved. Backups/rollback of actual installed systems remain unverified. The owner must approve a complete safe combined candidate and an actual game/client test before any deployment. To revert this experiment, reject/close the DRAFT; the server is untouched.
