# F14 human damage, DoT and assist -> owning-map refresh request

Owner: CosmicCuddle. Component: Individual Progression. Date: 5 November 2026.
Base: F14 DRAFT #83 at 1f71b99c2cfff79b2ad42019769da539c89ec61b. Status: DEV-only; default OFF; no live server.

## Real gap
Earlier F14 #83 queued a refresh when a verified bot logged out. But a previously tracked creature can acquire new human threat or loot/assist rights through direct damage, DoT, heal or a human healer on a tracked RandomBot, without AttackStop or logout. The registry already marks evidence uncertain (fail-visible), but did not request any map-thread visibility pass. Future concealment could therefore leave a creature incorrectly hidden even after a human interacted.

## Narrow implementation
- IndividualProgressionF14LifecycleScript.cpp: when existing OnParticipantEvidence invalidates the tracked creature or its stale conservative snapshot is recomputed, enqueue only key+incarnation into the existing value-only logout queue, with the shared LifecycleLock. Do not call visibility, packets, playerbot manager or core inside the synchronous damage/heal callback.
- IndividualProgressionF14LifecycleRegistry.h: add a backward-compatible optional onAffected(CreatureKey, stamp) callback in OnAssistOfActor, copying affected NPC identities before applying conservative invalidation. The Heal/HoT on positively tracked RandomBot path places those values in the same bounded queue, even if the NPC never receives a direct hook.
- #83 OnUnitUpdate remains the ONLY map-thread visibility call after releasing LifecycleLock. It discards stale stamp / map / incarnation and honors poison on capacity overflow. Queue name is historical; it now holds owner logout AND changed-rights requests.
- Only enabled when COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT=1 at compile time in ephemeral CI. Existing live config keys OFF and no full-ready macro defined.

## Test and safety boundaries
New C++17/C++20 and Clang/GCC pure tests check direct human evidence, assist fanout to correct NPCs, self-assist exclusions, one-shot queue, instance mismatch, recycled incarnation and poison. Python source tests check lock order, map-thread-only UpdateObjectVisibility and no stage/DB/packet mutations. Full pinned Grim Core + original Core #56/#67/#76/#77, unchanged PB, Naxx Core and Character Services native compile is a separate CI gate, not gameplay proof.

Still missing: complete human target/quest/loot/tap/group/DoT/vehicle/pet rights, WHO, positive F14-specific creature-hide policy and per-viewer #85 ledger (#86 Core seam still separately experimental). This patch only schedules a future visibility PASS, not concealment or verified 3.3.5a client restore. Overflow does not make hidden clients visible; F14 must remain OFF.

Rollback: discard unmerged DRAFT; no host, database, SQL, config, DBC, client, scripts on production or player data touched. Full installed baseline and restorable backups plus CosmicCuddle approval required before any future production change.