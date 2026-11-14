# Combined Hunter + grouping + C3 leader guard: port of the partial F14 saved-stage login safeguard

**Owner:** CosmicCuddle
**Created:** 31 October 2026
**Status:** DRAFT only. Unmerged, not installed, not game-tested. The new flag is **OFF by default**.

| Item | Value |
| --- | --- |
| Branch | `integration/hunter-group-c3-f14-partial-login-20261031` |
| Parent (ancestor) | Combined Hunter + grouping + offline-leader guard [DRAFT #16](https://github.com/CosmicCuddle/mod-individual-progression/pull/16) at exact `c44c3320406670cdc8f25c963ebba13e9cf24b71` |
| Port source (not an ancestor) | Partial F14 saved-stage login guard [DRAFT #13](https://github.com/CosmicCuddle/mod-individual-progression/pull/13) at exact `a18397bbb0c7054052030a83ac30b699ac382d38` |
| Assignment | Coordination [Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13) (narrow port; IP #14 GM-command fixes deliberately **not** included) |

## Why this branch exists

Before this branch, #16 had the Hunter autocast, expansion grouping and C3 offline-leader safeguard, while #13 had the default-OFF partial F14 login safeguard. **No single branch contained both.** This branch is a line-by-line semantic port of #13's four executable/config deltas onto #16. It is not a rebase of #13, and it does not merge or overwrite either parent.

## Exact source port

Only #13's delta over its own parent (#11 `d4ff9374…`) in executable and config files was ported. That delta is four files, and it applied to #16 with no conflicts:

1. **`src/IndividualProgressionPlayer.cpp`** (two hunks, both copied verbatim from #13):
   - **Include head:** the optional `__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")` include, `COSMIC_IP_F14_PLAYERBOTS_AVAILABLE`, and a compile-time `#error` when `COSMIC_IP_F14_REQUIRE_PLAYERBOTS` is defined but the header is missing.
   - **`OnPlayerLogin`, non-normal-account branch only:** the original level-based `ForceUpdateProgressionState` (stage 0 / 8 / 13) is skipped **only** when all five conditions hold:
     1. the flag is ON;
     2. the race is Draenei or Blood Elf;
     3. the session exists and is headless;
     4. the saved stage read from rewarded markers is between `FirstTbcStage` (8) and `LastSupportedStage` (18);
     5. `sRandomPlayerbotMgr.IsRandomBot(GUID counter)` is positive.

     Otherwise the original three-way level write runs unchanged.
2. **`src/IndividualProgression.cpp`:** reads `IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin` with code default `false`. If the Playerbots header is absent, an ON request logs an error and is forced OFF.
3. **`src/IndividualProgression.h`:** one `bool` member.
4. **`conf/individualProgression.conf.dist`:** the new key, **`= 0`**, with #13's warning text.

#13 does not use #11's `IndividualProgressionRandomBotStagePolicy.h`, so that header is **not** brought in. The port relies only on `IndividualProgressionGroupPolicy::FirstTbcStage` and `LastSupportedStage`, which #16 already has.

### Preserved from #16, byte-identical and asserted by test

- `OnPlayerCanGroupAccept`, including the C3 early guard. The leader is resolved and must be non-null and in-world **before** any quest-stage read. With enforcement ON, an unknown leader is rejected with no mutation. With enforcement OFF, it is allowed with no null→stage-0 rewrite.
- Hunter `src/IndividualProgressionSpells.cpp` (blob `6ee93e1174b5aef6c5d61c9ea31526506c9d08da`).
- `src/IndividualProgressionGroupPolicy.h` and `src/cs_individualProgression.cpp`. The GM commands are the same as #16, **without** IP #14's separate fixes.
- All SQL and data. There is no new SQL, DBC or client dependency.

## Declared CI deviation in #16's workflow

#16's `ip-combined-offline-leader-source-20261031.yml` had an unconditional "Strict six-file delta" step. That step pins #16's exact paths against #10 and the fixed blob of `src/IndividualProgression.cpp`, so it would falsely fail on **any** child of #16.

This branch adds only an `if:` that limits **that one step** to #16's own head ref (pull request or `workflow_dispatch`). This is the same pattern #13 and #16 already used for the #10 and #11 frozen steps. Every regression step in that workflow still runs on this child: the offline-leader tests, the Hunter and grouping checkers, and the C++17/20 policy. The new integration test asserts that exactly five lines were added and that the job itself has no `if:`.

**Future merge requirement:** if the owner later approves folding this child into #16, #16's frozen delta and blob assertions will reject it by design. They must be rebaselined under review, not bypassed.

## Tests and CI

**`tests/test_ip_combined_c3_f14_partial_login.py`** (26 tests) covers:
- both pinned parents;
- #16 files preserved, the C3 block identical, all three code quest reads after the leader guard, and exactly two hunks in the player file;
- the login region, include head, config block and conf block verbatim from #13;
- default OFF and fail-closed when the header is missing;
- the exact guard conditions;
- the workflow deviation.

It also includes a **synthetic** model matrix over race, headless state, identity and stage, covering:
- the OFF baseline;
- human, normal and excluded accounts;
- Altbot/unknown identities;
- Vanilla races;
- saved stages 8, 12 and 18 versus unsaved and 0–7.

These synthetic cases are **not gameplay evidence**.

**`.github/workflows/ip-combined-c3-f14-partial-login-20261031.yml`** has two jobs:
- **Source job:**
  - makes both pinned parents available;
  - runs a strict nine-path allowlist versus `c44c3320`, scoped to this PR's own head for future children;
  - checks that the register is append-only and that Hunter, GM and group policy are unchanged;
  - runs the new test and every inherited #16 check, plus the C++17/C++20 policy matrix.
- **Native job:** a pinned full Worldserver configure, compile and link with:
  - Core `8daffdabe9202c228e45d833c9a1d399597a15cd`;
  - Playerbots `e88cc4099e69f35126b28942a412ca702d20453b`;
  - Naxx `80e9de1c39a6dc995188a80c31a5e785d0f2a6f9`;
  - `-DCOSMIC_IP_F14_REQUIRE_PLAYERBOTS`;
  - the same two **runner-only** Playerbots compatibility substitutions as earlier runs (`PlayerbotsDatabase.h` `CreateAsyncConnection`→`CreateConnection`; `LocaleColumns.h` `IsLocaleRegistered`→`localeNames`).

  It checks that `compile_commands.json` holds all three IP translation units, with the define on `IndividualProgressionPlayer.cpp`. It **never launches** the server, and it uploads and deploys nothing.

#16's inherited `ip-expansion-pinned-worldserver.yml` also runs on this branch, as a separate unmodified full build without the define. The earlier isolated builds of #13 and #16 **do not certify this combination**. Only this branch's own runs count.

## Not fixed here (documented blockers)

- With enforcement OFF, the group-accept stage write happens before Core can still reject the join.
- Playerbots' direct `Group::AddMember` bypass of the accept hook.
- C1: Playerbots `Randomize(false) → ResetQuests` and the other marker-removal routes (`initself`, level-bracket and reset-level paths, `DropQuestAction`) still delete markers 66001–66018. **Partial F14 is not C1 protection.**
- Full F14:
  - first-time stage 8 creation, TBC eligibility and the saved stage 0–7 migration decision;
  - rndbot-only observer visibility;
  - GM command achievement/stage hazards (IP #14 is separate);
  - group sync.
- The `IsRandomBot` read is source-reasoned as safe only in this world-thread login callback at the pinned revisions.

## Production and rollback

The live database engine and version, effective `sql_mode`, installed revisions and effective server settings are **UNKNOWN** until the owner's read-only inventory is done. Shipped `.conf.dist` values are not evidence about the running server. Nothing on this branch changes live configuration, the database, the host or any binary.

**Rollback:** discard or revert this branch only. Parents #16 and #13 are untouched. For any later approved staging build, setting the flag OFF or restoring the previous binary returns the original login algorithm. Neither restores rewarded quest markers already deleted. That requires restorable character and world database backups taken beforehand, plus the owner's separate approval.
