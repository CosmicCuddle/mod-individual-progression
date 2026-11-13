# F14 Register reconciliation audit — no merged Register

This read-only local check compares each candidate's `docs/CUSTOM_MODIFICATIONS_REGISTER.md` against its **own pinned parent**, so child inheritance is not counted twice. It is a review aid for [coordination #51](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51), not a merge or runtime release proof.

```sh
node tests/f14_register_reconcile.js --manifest docs/F14_REGISTER_RECONCILIATION_MANIFEST_2026-11-13.json
node --test tests/test_f14_register_reconcile.js
```

The manifest pins #174–#178 local branch names, exact expected HEADs and parents. The tool reads local Git blobs only, with optional locks/lazy fetch/fsmonitor/prompting disabled. It checks a parent's exact merge-base relation, missing/moved refs, removed or modified prior sections, duplicate IDs, and added rows lacking test, dependency, rollback or non-live status wording. It flags obvious positive live/deployment claims for review. These are conservative text checks; semantic truth, exact test results and installed state still require human evidence. The tool never writes a reconciled Register, edits a branch, fetches, runs a workflow, or reads an external service.

Current local result: #174 adds `IP-F14-PASS-IDENTITY-LIFECYCLE-HARDENING-20261113`; #175 adds `IP-F14-GROUP-SYNC-ERA-FLOOR-20261113`; #176 adds `IP-F14-GROUP-ERA-CONSUMER-20261113`; #177 adds `IP-F14-NEGATIVE-INTEGRATION-WITNESS-20261113`; #178 adds `IP-MODULE-SYNC-PREFLIGHT-20261113`. These five IDs are distinct. At #178 `21b3e73a`, the tool detected an existing IP172 pair-rights test paragraph spliced under the new #178 heading. #178 `e75e2fe7033ccee35fd476fb008f6d231d4f6d5e` restores the original paragraph to its prior section; the pinned local candidates now each add one row with no prior section modified or removed. The four validator fixtures pass, including a regression for that exact class of splice.

Claude's parked social branch `d8a43a77f012d3e5aca1912a91eed2b6cbbffbbf` is **external reference only** in this local audit. Its [pinned comparison](https://github.com/CosmicCuddle/mod-individual-progression/compare/aed05a92f262362608f5d643c8570b504189aae0...d8a43a77f012d3e5aca1912a91eed2b6cbbffbbf) shows a dated Register heading without a stable `IP-...` feature ID. That is a reconciliation item, not permission to edit Claude's branch. The tool marks that heading `UNKNOWN` and does not pretend to inspect its full blob locally. A later approved composition should preserve the social row, assign/agree a stable ID with its owner if needed, and review its test/dependency/live-status claims. F15 Core/WHO/Playerbots refs remain cross-repository dependencies only.
