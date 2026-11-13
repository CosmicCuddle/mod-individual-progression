# F14 backlog v3 integration handback — source candidates only

Owner CosmicCuddle. Coordination [#51](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51). This dossier and the [candidate graph](CODEX_F14_CANDIDATE_GRAPH_2026-11-13.md) cover the ordered source/test/design backlog. The common IP parent is `aed05a92f262362608f5d643c8570b504189aae0`. No merge, hosted Actions, deployment, DB/SQL/config/DBC/client/host change or F14/Paid activation was performed. The installed tuple, full Worldserver link, gameplay and actual client restore remain unknown.

## Exact branch and changed-path inventory

Paths in each row are the **candidate delta against its own parent**. Child trees additionally inherit their parent, as noted.

| Candidate / current HEAD | Changed paths |
| --- | --- |
| [#174](https://github.com/CosmicCuddle/mod-individual-progression/pull/174) `feature/coding-support-f14-pass-identity-lifecycle-20261113` `3bf97b4d6edc746cb0ff7e7e02a55d6ab9fc5b32` | `docs/CUSTOM_MODIFICATIONS_REGISTER.md`; `docs/F14_PASS_IDENTITY_LIFECYCLE_HARDENING_2026-11-13.md`; `src/IndividualProgressionF14CurrentPassIdentity.h`; `tests/f14_current_pass_identity_test.cpp`; `tests/test_f14_pass_identity_lifecycle_source.js` |
| [#175](https://github.com/CosmicCuddle/mod-individual-progression/pull/175) `feature/codex-f14-min-tbc-group-floor-20261113` `005d8949334b11c7c985225d6aa9a1cc120157a1` | `docs/CUSTOM_MODIFICATIONS_REGISTER.md`; `docs/F14_GROUP_SYNC_ERA_FLOOR_2026-11-13.md`; `src/IndividualProgressionF14GroupSyncEraFloor.h`; `tests/f14_group_sync_era_floor_test.cpp`; `tests/test_f14_group_sync_era_floor_source.js` |
| [#176](https://github.com/CosmicCuddle/mod-individual-progression/pull/176) `feature/codex-f14-group-era-consumer-20261113` `971ad2ce160e29a8c426d2650e1d665c56e2ee01`, child of #175 | `docs/CUSTOM_MODIFICATIONS_REGISTER.md`; `docs/F14_GROUP_ERA_CONSUMER_2026-11-13.md`; `src/IndividualProgressionF14GroupEraConsumer.h`; `tests/f14_group_era_consumer_test.cpp`; `tests/test_f14_group_era_consumer_source.js` |
| [#177](https://github.com/CosmicCuddle/mod-individual-progression/pull/177) `feature/codex-f14-negative-integration-witness-20261113` `49af2465898e318481e9ad9de17699aea6f9338d`, child of #174 | `docs/CUSTOM_MODIFICATIONS_REGISTER.md`; `docs/F14_NEGATIVE_INTEGRATION_WITNESS_2026-11-13.md`; `tests/f14_negative_integration_witness.cpp`; `tests/test_f14_negative_integration_source.js` |
| [#178](https://github.com/CosmicCuddle/mod-individual-progression/pull/178) `feature/codex-module-sync-preflight-20261113` `e75e2fe7033ccee35fd476fb008f6d231d4f6d5e` | `docs/CUSTOM_MODIFICATIONS_REGISTER.md`; `docs/MODULE_SYNC_PREFLIGHT_2026-11-13.md`; `docs/MODULE_SYNC_PREFLIGHT_EXAMPLE.json`; `tests/module_sync_preflight.js`; `tests/test_module_sync_preflight.js` |
| Claude parked social `d8a43a77f012d3e5aca1912a91eed2b6cbbffbbf` (reference only) | `.github/workflows/f14-social-isolation.yml`; `conf/individualProgression.conf.dist`; `docs/CUSTOM_MODIFICATIONS_REGISTER.md`; `docs/F14_SOCIAL_ISOLATION_2026-11-13.md`; `src/IndividualProgression.cpp`; `src/IndividualProgression.h`; `src/IndividualProgressionF14SocialIsolationPolicy.h`; `src/IndividualProgressionF14SocialIsolationScript.cpp`; `src/IndividualProgressionPlayer.cpp`; `tests/f14_social_isolation_policy_test.cpp`; `tests/test_f14_social_isolation_source.py` |
| This source-only handback branch `docs/codex-f14-integration-readiness-20261113` | `docs/CODEX_F14_CANDIDATE_GRAPH_2026-11-13.md`; `docs/AUCTION_VISIBILITY_WORKER_SEAM_TEST_SPEC_2026-11-13.md`; `docs/CODEX_F14_INTEGRATION_DOSSIER_2026-11-13.md`; `docs/F14_GROUP_GENERATION_PRODUCER_AUDIT_2026-11-13.md`; `docs/F14_REGISTER_RECONCILIATION_AUDIT_2026-11-13.md`; `docs/F14_REGISTER_RECONCILIATION_MANIFEST_2026-11-13.json`; `tests/f14_register_reconcile.js`; `tests/test_f14_register_reconcile.js` |

## Evidence and status

| Item | Evidence at exact head | Limit |
| --- | --- | --- |
| #174 | CS01 independent reconstructed-source GCC/Clang C++17/20 normal/NDEBUG 8/8 PASS | Not full checkout, link, runtime or client |
| #175 | CS01 independent native 8/8 PASS | Pure floor; no producer/wiring/gameplay |
| #176 | Claude independent 8/8 native PASS at `b40ba6f` and #175 floor 8/8 in that composed context; source contract PASS at test/documentation-only `971ad2ce` | New C++ fixture not compiled here; no proven generation producer or tombstone retirement |
| #177 | Claude independent 8/8 native PASS at `b75cabf`; `node tests/test_f14_negative_integration_source.js` PASS at test-only `49af2465`, including G2 | New C++ witness changes not compiled here; no client ACK or Core token composition proof |
| #178 | `node --test tests/test_module_sync_preflight.js` 7/7 PASS at code head `21b3e73`; Register-only correction `e75e2fe` preserves earlier row | Offline synthetic Git fixtures; does not inventory installed components |
| Claude social | Source branch inspected for changed paths and Register row only | Independent runtime/native verdict not established here |
| Register reconciliation | `node --test tests/test_f14_register_reconcile.js` 4/4 PASS on the current manifest; #178 prior-row splice caught then corrected | Claude social row remains externally referenced, not locally validated; no merged Register |
| This branch | Documentation diff checked | No code behavior change; no PR or hosted Actions |

All six source candidates share a Register path; only #176→#175 and #177→#174 are inherited code dependencies. Future group adapter wiring would intersect Claude's runtime paths. Do not treat a clean Git text union as semantic compatibility. Source-contract tests must run again on an exact disposable composition tree; historical parent/blob assertions need explicit review, not silent relaxation. See the graph note, the [producer audit](F14_GROUP_GENERATION_PRODUCER_AUDIT_2026-11-13.md) and the [Register reconciliation audit](F14_REGISTER_RECONCILIATION_AUDIT_2026-11-13.md) for the remaining boundaries.

## What can move next

Ready for CS01 **source-level composition review**: #174/#175 pure models, #176 under Claude's PASS WITH DOCUMENTATION CONDITION (now documented/tested), #177 test-only negative witness and #178 offline tool, with their parent refs preserved. #176's unresolved generation/capacity contract remains visible. The graph note proposes an isolated application order and exact-source retest; no shared integration branch, merge or rebase has been made.

Not ready for runtime wiring or release: #176 lacks a verified non-reused session/map/membership/leader generation producer and a bounded tombstone retirement proof; #177 lacks a genuine Core pass-to-viewer adapter and client receipt; Claude social's runtime composition and macro-OFF behavior need separate evidence; Core #25/Playerbots installed versions and native build/gameplay are unknown. F15 WHO/visibility/Playerbots work is cross-repository reference, not an IP implementation. [Auction #53](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/53) remains a separate design dependency; its [test specification](AUCTION_VISIBILITY_WORKER_SEAM_TEST_SPEC_2026-11-13.md) changes no Core code.

Recommended next three safe source tasks: (1) after Claude's #176 verdict, test a disposable **local** composition tree and record exact resulting blobs/source-contract outcomes without changing shared branches; (2) trace Core #25 and pinned Playerbots callback/queue ownership to prove or refute each generation and quiescence requirement, with negative fixtures for missing proof; (3) produce a candidate-keyed Register reconciliation diff/validator on the disposable tree, flagging duplicate IDs, stale refs, live-status claims, dependencies and rollback gaps. Any actual shared integration branch, macro-ON runtime change, hosted Actions, production or live-data work follows the explicit coordination/owner gates in #51.
