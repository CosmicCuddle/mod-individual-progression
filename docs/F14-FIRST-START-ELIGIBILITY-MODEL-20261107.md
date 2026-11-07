# F14 selected Blood Elf/Draenei RandomBot first start — test-only decision boundary

**Date:** 7 November 2026. **Owner:** CosmicCuddle. **Stage:** pure independent model / S2–S3 candidate; no gameplay implementation.

## Why

The real IP #46 `IndividualProgressionPlayer.cpp` bot/excluded-account login branch uses level: **≤60→stage0**, 61–70→stage8, 71+→stage13. Its optional F19 guard only **preserves previously earned stage8+** and does not make a new eligible level-60 Blood Elf/Draenei RandomBot enter TBC. Existing Playerbots creates synthetic headless sessions with raw Wrath expansion value regardless of IP tier, and `currentBots` membership may precede the actual Player login; neither is proof of owner-approved starting entitlement. A blanket `ForceUpdateProgressionState(stage8)` can remove previously earned quests, override GM commands or write a tier above the server's era limit.

## Test-only deterministic decision

Five new files in this child change **zero production code**. The `tests/f14_first_start_eligibility_model.h` pure function returns only a verdict — **never** modifies quests. It can name an eligible **new** stage8 start only with all caller-supplied positive flags: explicit pilot ON; verified headless genuine RandomBot; race10/11; independently proved new character and absence of preexisting data/quests; stage0; explicit world TBC availability; cap=0 uncapped or nonzero>=8; owner-approved starting-tier exception; and if paid mode is active, a **separately approved paid Bot exception**. Otherwise it preserves an existing higher earned tier as-is, signals owner review for ambiguous/stage0–7 existing progress, or does nothing for real humans/nonbots/OFF. It is not a trusted identity oracle: synthetic flags are **assumed facts**, not evidence that the server can currently supply them.

The source/negative matrix checks races1..12, stages0..19, caps{-1,0,7,8,12,13,18,19}, existing/first state combinations, missing positives and paid-mode exceptions. It deliberately **does not** define/approve how an old RandomBot's saved0..7 progresses or how the installed `ProgressionLimit=0` interacts with actual Vanilla host configuration.

## Missing to turn into implementation

1. Verify the original user-approved exact eligibility rule from coordination #9/#11 and how installed cap/world/client compatibility is intended to work. All owner-final human paid 10259 and Wrath rule remains separate.
2. Identify a **proven real first-created, fully loaded bot event** from Playerbots that isn't mere `currentBots.insert`, database rehydration or a raw Wrath socketless WorldSession.
3. Provide a trusted, owner-thread classification with restart-safe actor incarnation, and an IP callback guaranteed to occur before the destructive login rewrite. Source #2 snapshot remains model-only and is not authority.
4. Define and get approval for handling existing saved0–7, cap7, world expansion TBC enabled but IP capped Vanilla, `.ip setbot` GM downgrade semantics, mixed groups and reboot.
5. Test exact rewarded quest markers 66001–66018, progression limit and paid exception, human and Altbot negative cases in isolated restorable DB. A successful full Worldserver native link is still not gameplay.

**No live code, active config, source defaults, paid purchase, SQL, character DB or server startup.** Rollback is discard this child DRAFT. Related: [F14 #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [IP #46](https://github.com/CosmicCuddle/mod-individual-progression/pull/46), [PB #1](https://github.com/CosmicCuddle/mod-playerbots/pull/1), [PB #2](https://github.com/CosmicCuddle/mod-playerbots/pull/2), [five-repo task routing #40](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/pull/40).
