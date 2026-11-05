# F14 tagged GUID Core callback observer — real IP integration, default OFF

**Owner:** CosmicCuddle
**Developer:** CS01 Maintenance, while Claude CS03-IP is on an owner-confirmed break
**Date:** 5 November 2026
**Status:** isolated **DRAFT**, no live server, client, config, DB, DBC, SQL or deployment change

## Why this exists

The original [#93 Core out-of-range hook](https://github.com/CosmicCuddle/mod-individual-progression/pull/93) and [#95 Core CREATE attempt hook](https://github.com/CosmicCuddle/mod-individual-progression/pull/95) offer post-`SendDirectMessage` **server-call** metadata for a source-tagged creature GUID. [#103's pure adapter](https://github.com/CosmicCuddle/mod-individual-progression/pull/103) validates a matching original Claude #94 prior-hidden pair, session epoch, map/instance and incarnation, but previously there was **no actual C++ IP script subscribed to the Core hooks**.

This change wires a **real `WorldObjectScript` subclass** in the existing compiled IP lifecycle source `src/IndividualProgressionF14LifecycleScript.cpp`. It subscribes to:
- `WORLDOBJECTHOOK_ON_F14_BATCH_OUT_OF_RANGE_ATTEMPT` — tagged per-GUID OUT after a Core session send call.
- `WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTEMPT` — tagged per-GUID CREATE after a Core send call.
- `WORLDOBJECTHOOK_ON_F14_BATCH_CREATE_ATTRIBUTION_QUERY` — always returns **FALSE**; this script never claims a normal Core CREATE belongs to an earlier F14 concealment.

It never registers or implements a positive F14 creature hide callback.

### Strict runtime boundaries

This entire observer implementation, header include and registration compile **only when** the development build explicitly sets `COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT=1` AND the pinned F14 Core lifecycle hooks are available. The macro is **not present in default source config or production build flags**. Original lifecycle actor/creature logic remains byte-for-byte intact, and `src/IndividualProgression.cpp` remains untouched.

The callbacks reject missing viewer, out-of-world viewer, missing map, empty GUID or non-creature GUID. Otherwise they increment **aggregate counters only** (one for tagged OUT, one for tagged CREATE). Invalid reports have a separate counter. The first valid report in each direction (and first invalid report) can produce a single generic diagnostic log entry, without player names, GUIDs, IP addresses or character IDs. No per-event logging or stored Player/Map pointers.

**Important:** an OUT event often occurs after the creature is unlinked, so the callback cannot independently recover its former incarnation or the exact viewer login epoch. We **do not invent those details**, and therefore we deliberately **do not call** the pure #103 protocol adapter, change its `prior-hidden` ledger, mark a restore or grant concealment. In addition, #95 CREATE nomination remains default FALSE, meaning our observer does **not** make any normally visible creature an F14-tagged CREATE.

This is the smallest genuinely compiled connection between the two Core send-attempt hooks and an IP runtime observer without inventing absent rights or lifecycle data. Its new counter/log diagnostics are **not client ACK, receipt, rendering, targetability, actual creature restoration or a positive hide**. It is not a functioning completed F14 feature.

## Test plan and evidence limits

- Source test enforces **exactly five review paths**, reconstructs the original IP lifecycle file after removing this single macro guard and checks byte-for-byte unchanged historical logic, confirms the exact Core hook registrations, no positive CREATE attribution or hide and zero protocol/ledger mutations.
- Native disposable GitHub workflow layers the pinned **Grim Core + #86/#93/#95 observer Core prototypes + #99/#100/#101/#102 rights veto** and the actual IP stage-floor/rights source, selected historical Playerbots, Naxx #292 and Character Services #16. It explicitly sets `COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT=1` *for that temporary build only*, then compiles and links the actual Worldserver without starting it. Symbol tests confirm this particular observer was compiled, not optimized/preprocessed away.
- A second **default-OFF static compile** or preprocessor source check must confirm no default registration or positive script hook. **Do not claim native PASS until exact-head GitHub Actions confirms it.** A full compile is never an in-game gameplay proof.

## Release block and rollback

No live server or production merge is authorized. A future runtime adapter must establish an authoritative per-viewer login epoch, original creature incarnation, owning map-thread synchronization, missing/poisoned source rights and already-hidden obligations, followed by actual **client-visible** hide/re-create/targetability tests. There is no 3.3.5a individual-object ACK. Full human player/group/quest/loot/pet/charm/guardian/DoT/assist/kill-credit evidence, selected headless BE/Draenei RandomBot identity, installed host/source/SQL/config/DBC/client inventory, real backups and explicit CosmicCuddle approval remain mandatory.

Reject this DRAFT to roll back. No live files need restoring because none were changed.
