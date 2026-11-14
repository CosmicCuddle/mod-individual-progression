# F14 pure viewer restoration protocol — exact-blob source integration into actual #92 IP (DRAFT)

**Owner:** CosmicCuddle. **Maintainer:** CS01-IP, covering the existing development lane while Claude CS03-IP is on an owner-confirmed break. **Date:** 5 November 2026. **NOT deployed; no feature ON.**

## Source ancestry and preservation

- Base: [DRAFT #96](https://github.com/CosmicCuddle/mod-individual-progression/pull/96) exact `869325e7d78391b6924901699093c58487aec2d8`, whose unchanged parent is actual IP source [#92](https://github.com/CosmicCuddle/mod-individual-progression/pull/92) `621673d66a0cd02a9a4667b0a8f62b1cf6b8ffde`.
- **Exact unmodified blobs copied from Claude's [#94](https://github.com/CosmicCuddle/mod-individual-progression/pull/94) `61302474b2c0e2acfcc6b5c32036dde1517e43c5`:**
  - `src/IndividualProgressionF14PriorHiddenLedger.h`: `2250bae56bb09a810d337927f738bd3cbcf5fddd`.
  - `src/IndividualProgressionF14ViewerRestoreProtocol.h`: `f520356a8152d98580751604f4649fe1b564f072`.
  - `tests/f14_viewer_restore_protocol.cpp`: `9bce90ed2c3e0f05dc41583d8e96b4f53f661422`.
- **Protected**: Claude's #94 branch and all original #92 source/header/config/tests; newer #92 F14 lifecycle-registry blob `2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8` is **not overwritten** by #94's older registry `e0965b40...`. Existing Naxx, Services and Playerbots source, Grim upstream and protected fork master remain untouched.

## Actual integration achieved (and not achieved)

The two unmodified pure C++ headers now exist in an isolated proposed **IP source tree** that also contains the actual #92 stage-floor and combat/rights work. Their include resolves the existing newer #92 lifecycle registry. This is reviewable, non-invasive source consolidation rather than a simulated copy inside an external compiler include directory.

The test invokes the **original unchanged** #94 pure C++ protocol-model test against the newer real #92 registry using GCC and Clang, C++17/20 and NDEBUG/non-NDEBUG. The new source contracts require exact imported blob hashes, untouched gameplay source/config, all flags OFF, no normal compiled module include/wiring, no client proof claim, and no completed prior-hidden ledger from ordinary Core link transitions.

**Absolutely NOT implemented:** the #94 protocol is not constructed by the Worldserver; it is not attached to #86/#95 Core callbacks or #92 gameplay rights, not a visibility deny, not a client packet receipt or restored-object ACK. No client create/destroy ACK exists for 3.3.5a. The prior-hidden ledger is never cleared based on default-false server send-attempt observations. A functional hide+restore consumer must be separately designed, owner-reviewed and tested.

## Dependencies, tests and release/rollback

- Parent #96 disposable CI combines real #92 source with #95 default-false per-GUID OUT-OF-RANGE and CREATE send attempts plus selected RNDBOT-only PB / Naxx / Services; status must be checked at its exact GitHub Actions head.
- This branch's own **source/pure model** CI must pass before describing the imported files as source-compatible; the full live Worldserver/client remains untested.
- No config, SQL, DBC, client, existing source, DB, server executable or production operation changes. Rollback is to reject/close this independent DRAFT; nothing to restore on the live machine.
- Release blockers: complete human quest/group/tap/threat/target/damage/DoT/assist/pet/loot/kill-credit safety; selected verified BE/Draenei RandomBot identity, WHO, safe positive viewer-specific concealment, client-visible restoration across death/corpse/map/logout/relog/overflow/disable, installed source/DB/config/DBC/client inventory, backups/rollback and owner approval.

**Status:** Unwired, source-only, all F14 and paid switches OFF.
