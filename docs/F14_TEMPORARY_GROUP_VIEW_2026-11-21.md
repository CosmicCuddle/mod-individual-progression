# F14 temporary group-stage view — 21 November 2026

Owner: CosmicCuddle. This is an isolated implementation increment from `f3d0ff64b03fb72e5baca6511fb751a3ec9837f4` (IP Draft PR188). It keeps the older draft branches intact and reuses `IndividualProgressionF14GroupSyncEraFloor.h` and its C++ test byte-for-byte from the existing group-floor work (file evidence pinned at `05435925`; branch head `005d8949334b11c7c985225d6aa9a1cc120157a1`).

## Implemented behavior

With the existing `IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor` option selected, an in-world, non-GM, headless race10/11 character must carry IP's existing positive RandomBot login identity before using this view. Race, account name or a missing cache entry alone cannot qualify it.

The two IP group-time paths no longer call `ForceUpdateProgressionState` for that selected population. `GetEffectiveProgression` instead consumes a temporary view of the current normal-human leader's access stage. Vanilla leaders give TBC entry stage 8; TBC leaders give their current stage; a bot already earned into Wrath can follow a Wrath leader, subject to the existing cap checks. Borrowing an unearned Wrath era remains deliberately blocked until the Core mutation-hook dependency below is closed. An earned stage-17 bot can therefore use 8, then 15, then 8 as the leader changes, without losing its earned quest markers. Leaving returns the exact earned stage. A missing earned TBC floor on departure is not repaired or concealed.

Login preserves a verified bot's already-earned TBC-or-later stage when it is at least the normal level-derived stage. Newborn/missing-floor login retains the inherited behavior, including its attunement handling. IP command/reset writes through `ForceUpdateProgressionState` cannot erase a selected bot's already-earned TBC floor. New or corrupt evidence is never turned into an earned stage grant.

Human, Altbot, unknown, non-headless, GM and normal-race identities do not enter this new view. The normal-race bot path can still sync into Vanilla. Existing `EnforceGroupRules` admission is preserved: this increment does not authorize otherwise-forbidden cross-era invitations, kick members or alter guild policy. Legacy non-selected bot/excluded-account synchronization is retained, including its existing quest-writing semantics; this is not proof that inherited excluded-account handling can never mutate an excluded human. The new view itself performs no quest or entitlement write.

## Ownership and invalidation

`IndividualProgressionF14GroupSyncView.cpp` queues only group GUIDs. The world update resolves current groups/normal human leaders after the map workers complete, checks leader membership/session/connection and publishes a revision-qualified snapshot. Source evidence was checked in Core `45499a228fe4273acf83b4a52a45033cec1f4dbc`: `World::Update` updates maps before world scripts; `MapMgr::Update` waits for map workers. This is an interface prerequisite, not a native concurrency test.

Group add/remove/leader change, login/logout, reconnect/disconnect, IP progression writers and config reload invalidate views. Disband erases them. Current group and leader GUIDs must match at read time. A leader's own `OnPlayerBeforeUpdate` detects hidden quest changes outside the IP writers without resolving any foreign Player. Queries inspect their own Player/group metadata and the value cache only; no Playerbots or database lookup is added to a worker query.

The table is capped at 4,096 groups, coalesces pending work and rejects stale publications. Overflow invalidates all positive snapshots until reset; revision exhaustion never wraps and requires a process restart. Pending leader facts cannot confer Wrath. With current positive bot identity and established group membership, the approved temporary TBC group floor still applies, including a valid earned stage 0; leaving exposes the unchanged missing earned floor. Invalid earned-stage values produce no access evidence. Both existing Core session-rebind and disconnect hook macros are required before positive cached leader facts are published. Missing hooks leave preservation active but suppress borrowed leader facts (and never an unearned Wrath era). No Core or IP experiment macro is enabled here, and the paused visual-concealment lifecycle remains untouched.

## Developer tests

Clang 20.1.2 via the previously checksum-verified portable Zig 0.15.2, Windows x86-64; strict warnings, `-O2`:

- Reused era-floor C++ test: 3,610 grouped combinations plus solo/transition/invalid checks, PASS (`-std=c++20 -DNDEBUG`).
- New snapshot/stage policy: 3,895/3,895 PASS (`-std=c++20 -DNDEBUG`), including invalidation, stale revisions, reload, leader/group mismatch, capacity and revision exhaustion.
- `run_f14_group_view_adapter_test.py`: actual new adapter plus actual `GetEffectiveProgression`, `SyncBotsProgressionToLeader` and `hasPassedProgression` bodies compiled against explicit Core test doubles. Command/login and persistent-title predicates are extracted from actual source. 49/49 checks with session hooks absent and 51/51 with hooks present; repeated in C++20 and C++17/NDEBUG. Cases cover TBC/Wrath transitions, join/leave/switch/disband, direct hidden-quest changes, logout/relog under the same GUID, disconnect/reconnect, missing identity, GM/headless/race immunity, legacy normal-race downsync, earned-stage preservation and a lowered cap refusing TBC access.
- Python: social contracts 34, new group-view wiring 10, inherited group-floor 6, verified login-floor 3, guild hook patch 3: 56 PASS. Existing assertions remain, with exact reviewed login/effective-reader additions pinned.
- `git diff --check`: PASS.

Reproduction from the module root (replace compiler prefix as appropriate):

```text
clang++ -std=c++20 -pthread -Wall -Wextra -Werror -pedantic -O2 -DNDEBUG -Isrc tests/f14_group_sync_view_test.cpp -o group-view
./group-view
clang++ -std=c++20 -pthread -Wall -Wextra -Werror -pedantic -O2 -DNDEBUG -Isrc tests/f14_group_sync_era_floor_test.cpp -o era-floor
./era-floor
python -B tests/run_f14_group_view_adapter_test.py --compiler clang++
python -B tests/run_f14_group_view_adapter_test.py --std c++17 --ndebug --compiler clang++
python -B -m unittest discover -s tests -p test_f14_group_sync_view_source.py
```

Portable Zig uses `--compiler <zig.exe> c++`. Fixture-only hook defines exercise both branches without enabling production hooks. Test doubles do not prove actual Playerbot classification, Core script registration, map ownership, native module linking or gameplay. GCC/sanitizers/full Worldserver build/gameplay NOT RUN. Hosted Actions not dispatched.

## Specific remaining safety dependency

Pinned Core has no general rewarded-progression-quest mutation hook at both `RewardQuest` insertion and `RemoveRewardedQuest` erasure. The leader-owned polling hook catches external edits at its next update, leaving a possible tick window. Consequently the runtime explicitly refuses to borrow Wrath when the bot has not already earned Wrath. The reused pure model supports that future transition; this runtime increment does **not** claim to complete it. Next coding step: a minimal isolated Core hook and IP consumer that invalidate before either rewarded-stage mutation, with actual-source interleaving tests, before removing this fence. No owner policy change or production operation is needed to write that code.

Persistent Vanilla PvP title cleanup is explicitly kept on earned evidence for selected grouped bots, preserving existing title retention and cap checks. A temporary TBC group view cannot erase an unrelated title. Tests compile the actual cleanup predicate; actual title persistence/UI remains native/gameplay QA.

## Remaining integration and rollback

Keep this option and Paid OFF pending owner-approved integration testing. Confirm native group metadata ownership, script ordering, installed hook support, mixed/offline parties and performance. Global progression and level caps remain authoritative: a stage cap below TBC cannot authorize TBC content even if the bot has higher historic earned markers. External Playerbots `ResetQuests`, raw-stage consumers outside the effective API and birth/solo missing-floor repair are not solved here. Guild off-thread admission/LANG_ADDON policy remains unchanged; no visual feature is resumed.

Rollback: revert this isolated increment or keep the existing option OFF. No SQL, database migration, client/DBC asset, live host/config or service change. Reverting restores inherited group quest synchronization, so integration must avoid toggling the pilot casually during a gameplay test. Source compilation and fixture success are not release authorization.
