# F14 PB #3 ↔ IP #124 first-start proof boundary — source-native test only (7 November 2026)

**Owner:** CosmicCuddle. **Maintainer:** CS01 Maintenance. **Dependencies:** [PB real default-OFF creation/login observer DRAFT #3](https://github.com/CosmicCuddle/mod-playerbots/pull/3) exact `f209a3a09d4479e1d183ba62f591709822db32fe`; [IP stage8 first-start PURE eligibility DRAFT #124](https://github.com/CosmicCuddle/mod-individual-progression/pull/124) exact `820621861e55b2dba6d2517e2cb5fd2fa281d7d0`. This is an isolated test-only child of #124, **not** a production interface or stage-change implementation.

## Safety issue being tested

PB #3 observes only a **same-process newly generated candidate** after `RandomPlayerbotFactory::CreateRandomBots` queues `SaveToDB(true,false)` and adds a cache entry, then a later admitted world-thread headless genuine RandomBot AI login. Its in-memory GUID+account+race record is single-use and retired, but **does not prove its asynchronous DB commit occurred, no previous character-data row exists, quest markers are empty, or that any paid exception is authorised**. Its exposed `HasUnretiredAdmission` must NOT be used directly to satisfy IP #124's separate `GenuineFirstCreationEvidence` plus `!ExistingCharacterData` prerequisites.

Our new fixture **compiles and invokes BOTH ACTUAL HEADERS** from independently pinned owner PB #3 and IP #124. It intentionally supplies favorable *synthetic* policy inputs to challenge the boundary (pilot ON as a fixture, actual feature OFF; World TBC, cap18, hypothetical owner exception), but **always leaves birth evidence untrusted** and `ExistingCharacterData=true` until a second, explicitly hypothetical separate trusted data source is supplied. With only PB #3's proof:

- unseen / only-candidate / missing AI / not-world / nonheadless / wrong account / wrong race / human / relogged / retired actor cannot qualify for a new stage grant;
- even a successfully admitted race10/11 RandomBot with saved stage **0–7** must yield `NoAction` or `NeedsReview`, **not `EligibleNew`**;
- legitimate already-earned stage8/13 remains `PreserveEarned` (pure verdict only), cap mismatch remains review, and cap0 means *uncapped*, not birth proof;
- enabling paid-era model inputs without a separately approved exception must not grant; supplying an hypothetical exception still does not replace birth evidence;
- the only `EligibleNew` case comes from independently and explicitly forcing **both** birth-created AND no-existing-data inputs positive inside this **test fixture**, **not from the PB observer**. This is a demonstration of missing prerequisites, **not** a policy decision or authorisation for real stage8.

## Exact source and CI scope

Only **four new files** on the child branch: `tests/f14_pb3_ip124_birth_proof_firewall.cpp`, corresponding `tests/test_f14_pb3_ip124_birth_proof_firewall_source.py`, one read-only C++ CI workflow, and this document. No changes to `src/`, `conf/`, `data/`, SQL, Core, Naxx, Services, Playerbots, object/quest/DB state or compiled owner runtime. Workflow pins unchanged upstream PB #3 and IP #124 HEAD, asserts changed-file list, runs original IP #124 source guard and new source guard, then compiles both headers with GCC/Clang C++17/C++20 normal/NDEBUG. **Verify actual hosted result separately before claiming it passed.**

**Important trust boundary:** The `OnlyDiagnosticEvidence(...)` function and optimistic cap/owner exception flags are intentionally **in a TEST cpp under `tests/`**. It must NEVER be copied into IP runtime as a stage-grant helper. Current owner IP #46 still has its existing real non-normal-account fallback; this child neither overrides it nor fixes old saved stage0–7. Existing IP paid per-character receipt protections and PB earned marker reset must remain intact.

## Outstanding implementation gates

1. A durable and restart-safe receipt of *genuine new character creation* after verified character DB commit (not `AT_LOGIN_FIRST`, not bot race/name/prefix, not PB same-process probe), with rollback and no existing saved quest/paid ledger data; owner approval for how to treat missing/legacy/previous 0–7.
2. Safe cross-thread and incarnation-aware verified genuine RandomBot ID and login lifecycle, differentiated from human Blood Elf/Draenei and same-account Altbots.
3. Actual explicit owner decisions on capped Vanilla world, TBC start floor, paid exception/exemptions, `.ip setbot` override, preexisting mixed state and earned stage8–18 retention.
4. Reviewed real IP integration on a separate source branch, exact five-owner-fork native build with Core #2/PB #3/Naxx/Services if appropriate, disposable 3.3.5a world/DB and F01–F20 tests with backups, and production approval. **No live installation, SQL change or grant authorised by this fixture.**

**Evidence level:** Source and native model tests only. Real PB #3 + Core #1 five-owner Worldserver link has independently passed at [Services #18 / Actions 42384548883](https://github.com/CosmicCuddle/mod-character-services/actions/runs/42384548883), but that tuple **does not include this IP model child, nor any stage8 issuer**. Actual owner-installed host and backups unknown.

**Rollback:** discard this test-only child. No runtime or data to roll back. Keep [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9) and [five-repository routing #40](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/pull/40) as the tracking sources.
