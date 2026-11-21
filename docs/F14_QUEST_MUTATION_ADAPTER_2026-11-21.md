# F14 paired quest-mutation adapter — 21 November 2026

Owner: CosmicCuddle. Isolated child `feature/codex-f14-quest-mutation-adapter-20261121`, parent IP Draft PR189 `fabff319d8ff38ea8c3e06ced5f22e3b435f165d`. The earlier branches remain unchanged. Paired Core topic: `feature/codex-rewarded-quest-change-hooks-20261121`, parent Core master `21ff1973bc75216f8a0d0f3d7731eb3e8894ff97`. Exact resulting commits/PR links are recorded in Coordination #51/#68.

Tested Core HEAD: `c6e3bf04d2e49fd30afcb8b0e3416fca50287aab`. Two earlier F14 social/group notes are also normalized to UTF-8 here; their content and historical evidence are unchanged.

## Source behavior

The Core child exports `COSMIC_REWARDED_QUEST_CHANGE_HOOK_VERSION = 1`, with non-vetoing before/after notifications around rewarded markers and active-entry projection changes. It brackets `SetRewardedQuest`, `RemoveRewardedQuest`, `SetQuestStatus`, `RemoveActiveQuest`, and the outer AddQuest/RewardQuest operations. This matters because the unchanged Core `GetQuestStatus` reads active entries before rewarded storage. Pre-login hydration remains a separate login lifecycle. No Core reward, SQL/schema/config or quest rule is changed.

The IP adapter registers the new hooks only when the API is present. Hidden progression quest IDs 66000 through 66018 enter a nested global in-flight count. Every pair counts even with the pilot OFF, so an option change between its two halves cannot corrupt the count. The group GUID invalidation is selected only when the pilot is ON. No Player pointer or captured group lease survives a callback.

While a mutation is active, snapshots cannot be read or published. A blocked publication is requeued instead of silently losing liveness. Config reload clears rows but preserves the count; nested inner completion cannot release an outer mutation. Both old and current group metadata are invalidated at the corresponding boundary. An unmatched after-event or depth overflow permanently poisons positive snapshots for that process; reload cannot hide the corruption. The approved temporary TBC floor remains the safe view for a positively verified grouped racial RandomBot.

With API v1, both existing Core session hooks, fresh normal-human leader facts, positive current race10/11 headless RandomBot identity and Paid OFF, a TBC-earned bot can now temporarily follow a Wrath group and return to TBC without rewriting earned quests. Existing caps/admission remain authoritative. Leaving exposes exact earned state. No identity means no borrowed access. Old Core still keeps the prior unearned-Wrath fence. Paid-ON also keeps that fence because receipt revocation is a separate lifecycle; this increment does not claim to support it or enable paid mode.

Persistent title handling, normal-race/Altbot/human policies and the previous social reply work are unchanged in this child. Visual concealment remains paused, and guild off-thread admission/LANG_ADDON decisions remain unchanged. No Playerbots source or Claude-owned dungeon/AutoGear/flight code is modified.

## Executed evidence

Compiler: Clang20.1.2 via workspace-only Zig0.15.2, strict warnings/O2; no full server configure/build/install.

- Paired actual-source adapter matrix with `--core <paired-checkout>`: **67/69/69/71 PASS** for `(session,quest)` hook availability `(0,0)/(0,1)/(1,0)/(1,1)`. Repeated C++20 and C++17/NDEBUG, 276 checks per build mode. Compiles actual Core writer/status/getter/dispatch/guard and IP adapter/effective/sync/access bodies against explicit engine doubles; command/login/title predicates also come from actual sources.
- Cases force a world refresh during edits, nested edits, active-entry masking/reveal, option/config changes mid-pair, unrelated quests, no-ops, missing hooks/identity, malformed pairs and Paid-ON refusal. The supported-hook mode demonstrates unearned Wrath borrowing after completion and TBC fallback during mutation.
- Standalone snapshot policy C++20/NDEBUG: **3,906 PASS**, including nested fences, reload, deferred-publication retry, mismatch poison, capacity and revision saturation.
- Legacy IP runner without `--core`: **49/51 PASS** for absent/present session hooks. A strict-warning unused fixture constant was corrected before this pass.
- IP Python source/patch suites: **58 PASS**. Core source contracts: **8 PASS**; actual Core mutation/container/script tests: **35 PASS** in C++20 and C++17/NDEBUG.
- Diff checks PASS. Core SQL style PASS. Full Core C++ checker exits 1 with four existing diagnostics in three parent-identical, untouched files (`BattlegroundAV.h`, `MoveSpline.cpp`, `Spell.cpp`); none in changed C++ paths. No unrelated cleanup.

Reproduce from IP root:

```text
python -B tests/run_f14_group_view_adapter_test.py --core <paired-Core> --compiler clang++
python -B tests/run_f14_group_view_adapter_test.py --core <paired-Core> --std c++17 --ndebug --compiler clang++
python -B tests/run_f14_group_view_adapter_test.py --compiler clang++
clang++ -std=c++20 -pthread -Wall -Wextra -Werror -pedantic -O2 -DNDEBUG -Isrc tests/f14_group_sync_view_test.cpp -o group-view
./group-view
python -B -m unittest discover -s tests -p test_f14_group_sync_view_source.py
```

Zig uses `--compiler <zig.exe> c++`. Fixture hook defines do not activate production hooks or the pilot. Actual Core headers/engine linkage, native Playerbot classification, whole quest-reward execution, gameplay/performance, GCC and sanitizers are NOT RUN. No hosted Actions requested.

## Integration boundaries and rollback

Supported mutators are observable; direct writes through Core's exposed mutable quest maps are not. Native composition must check custom modules for direct human-leader progression-map writes and matching callbacks. Existing Playerbots direct-map/reset behavior is not repaired here; the source snapshot accepts only a normal human leader and the selected bot reads its own earned evidence. No universal claim is made for uninstrumented data access or installed module classification.

EnforceGroupRules admission is retained, including its existing cross-era refusals. Inherited non-selected/excluded-account sync behavior is unchanged, not a blanket guarantee for legacy excluded-human handling. Native QA must cover mixed/offline parties, stage limits/Vanilla60, Hunter/Naxx40/Onyxia40, OriginalGear/Altbot, title persistence and bot login/reset performance. This source increment closes the supported Core mutation-path dependency from PR189; it is not full F14 gameplay acceptance.

Runtime/distribution defaults remain OFF. Installed/live version and config state are not established. No merge, deployment, SQL/data migration, host/service/config, client/DBC or gameplay-rule change. Keep the pilot OFF until owner-approved composition testing. Rollback: revert this IP child and paired Core source, rebuild matching Core/modules; never mix old binaries with the new virtual API. The earlier IP branch safely retains the legacy fence. No data rollback is added, and production work still requires CosmicCuddle's approval.
