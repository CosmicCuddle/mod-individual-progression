# F14 refresh-queue saturation: visible diagnostics

**Owner:** CosmicCuddle. **Date:** 5 November 2026. **Source base:** exact IP #88 `c6cd97366f080b78a2712238b853025b20584d0a`. **Status:** DRAFT experimental source, NOT merged/deployed.

The pre-existing F14 #83 logout queue is reused by #88 for human damage/DoT and healer-assist invalidation. Queue saturation causes `IsPoisoned()` and prevents future refresh requests. However, #88 only emitted a queue poison log from the **bot-logout** code path; a queue overflow triggered entirely by human damage/healing could remain silent. `GetF14LifecycleStatus().poisoned` also checked only the combat registry, not the independent refresh queue.

This child adds one exact-once queue warning inside the existing lock-held `LogPoisonOnce()`, already called from both direct/assist and logout paths. It avoids duplicate logging on later events. `GetF14LifecycleStatus().poisoned` now returns the conservative OR of registry and queue poison when the experimental compile flag is enabled. The setter continues to default false with the experimental feature OFF; no positive NPC hide/restore is possible.

**Critical:** log and status are observability only. If a creature had previously been hidden, they do not restore any client links after queue saturation. Positive concealment, per-viewer restoration, packet/client proof and all human combat/loot/quest rights remain incomplete; full-ready macro is undefined. The queue can be reset by the existing lifecycle reset; a future positively enabled feature must not clear prior-client restoration obligations just because config reload succeeds.

**Tests:** new Python source contracts verify queue poison before registry-early-return, exactly one error log, both invalidation paths using the logger, status reporting, OFF config and no mutation. Existing #88 source tests/C++ queue model plus native Worldserver compilation required.

**Dependencies:** Grim Core `0d754d19`, F14 #83/#88, unchanged Playerbots `e88cc409`, Naxxramas Core #292 and Character Services #16. No SQL, DBC, DB schema, client patch, live server or configuration changes. **Rollback:** discard isolated DRAFT; no server or character data touched. Owner approval, backups and integrated 3.3.5a gameplay required before any deployment.
