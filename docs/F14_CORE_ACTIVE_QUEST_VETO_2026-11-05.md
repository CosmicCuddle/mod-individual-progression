# F14 active human NPC / kill-credit quest objective veto

**Owner:** CosmicCuddle  
**Developer:** CS01 Maintenance, covering active development while Claude is on owner-confirmed break  
**Date:** 5 November 2026  
**Status:** isolated DRAFT, negative-only, default F14 OFF, no host/DB/client change

## What is actually implemented

On top of [#100](https://github.com/CosmicCuddle/mod-individual-progression/pull/100) at exact `8d9bb660116d196e1ea8b9d20e72f7bfb549825b`, a new exact-anchor patcher changes **one disposable pinned Grim Core C++ file** (`PlayerUpdates.cpp`), extending the already-reviewed #99/#100 `F14ViewerHasDirectInteractionVeto` helper. Both per-viewer visibility call sites remain **unchanged** and continue to require an explicit prior `ShouldF14SuppressCreatureForViewer` TRUE result. All real callbacks remain default FALSE with no positive F14 consumer.

The additive **real Core** checks:

1. Iterate at most `MAX_QUEST_LOG_SIZE` current quest-log slots for the exact viewer; ignore empty or non-INCOMPLETE slots. This does NOT scan every quest in the global DB.
2. Fetch each active incomplete quest's **actual** `sObjectMgr->GetQuestTemplate`. If missing, **fail visible**; missing status/templates are not permission to hide.
3. For each `RequiredNpcOrGo[f14Objective]` with positive creature entry and nonzero required count, **fail visible** when it equals this creature's `GetEntry()`, **or** one of its `CreatureTemplate::KillCredit[0..MAX_KILL_CREDIT)` aliases.
4. Negative GO objective entries are not treated as a creature-kill objective; a completed quest is not an *active incomplete* requirement. Even if this narrow check does not veto, **no positive concealment is authorized**.

Previous #99 direct target/pet/loot/GMs, and #100 group/charm/guardian/bounded caster aura checks remain intact. New `ObjectMgr.h` and `QuestDef.h` includes resolve the pinned quest API. No packet mutation, quest status changes, reward grants, SQL, DBC, client patch or prior-hidden ledger clearance.

## Cautions and limits

- This is **only an extra negative veto**. A quest requiring an item dropped by the creature, gossip/spell interaction, shared group quest credit, special scripted kill-credit, assist/DoT, pending cast, pet, skinning or generic kill rewards may still require the creature to remain visible. The missing **complete independent rights provider** remains a release blocker.
- This check runs **only behind** a future verified positive F14 per-viewer callback; currently no callback returns TRUE. It is bounded at stock quest log slots times at most the four objective slots and `MAX_KILL_CREDIT` entries.
- Quest/caster/client server-call evidence does **not** prove actual hide/reappearance or client ACK. #94 prior-hidden ledger remains unwired and unchanged, and no stale per-viewer state is cleared.
- This exact patch targets a **disposable CI Core checkout** pinned to Grim `0d754d1921261dfd939f91060ebc022eae280f71`. It is **not installed** into CosmicCuddle's protected Core fork or live host. No bot/account or production data are touched.

## Exact tests and rollback

A source gate pins the previous #99/#100 original patcher blobs, immutable IP source, all F14 and paid flags OFF and six changed assets; the C++ model compiles the **actual #99 + #100 + this extension's transformed Core helper** against fake interface objects for vanilla quest log, missing template, incomplete vs completed quests, direct NPC objective, negative GO objective, no objective count, both kill credit aliases and the upper quest log slot, while preserving the previous 35 direct/player/pet/group/aura/charm/guardian cases.

A **full native pinned Worldserver compile/link** with IP, Playerbots, Naxx #292 and Character Services #16 is required to validate actual Core ABI. The workflow may run this on a disposable GitHub runner. **Do not claim CI success until its exact-head jobs finish**; a native build still is not gameplay proof.

The final test candidate also requires actual installed version/config/DB/DBC/client inventory, recoverable backup and rollback, approved positive RandomBot-only identity, full human rights, safe client reappearance across kill/corpse/combat/logout and CosmicCuddle's explicit authorization. Revert by closing this separate DRAFT branch; nothing on the live server to undo.
