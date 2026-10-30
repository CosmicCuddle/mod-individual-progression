# CosmicCuddle IP — Expansion-based grouping (development-only)

**Owner:** CosmicCuddle  
**Design date:** 2026-10-30  
**Branch:** `feature/expansion-group-rules-20261030` based on protected `develop/cosmic-ip`  
**Coordination:** [Issue #10 — grouping rule](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10) · [Issue #9 — F14 rndbot floor/visibility](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9)  
**Status:** Separate experimental source candidate. No merge, server compilation, in-game proof or production deployment is implied until individually verified.

## What is being implemented

Keep the **existing** `IndividualProgression.EnforceGroupRules = 0/1` option and do **not** add another setting.

- **OFF (0):** preserve the current IP grouping and bot acceptance/sync paths.
- **ON (1):** both invitation and acceptance require players to occupy the **same effective expansion**, **not** the exact same raid tier.
- Vanilla stages **0–7**; TBC **8–12**; WotLK **13–18**.
- Blood Elves (race 10) and Draenei (race 11), **including human characters**, cannot join Vanilla groups **while enforcement is ON**. This never makes humans invisible.
- A **valid** non-normal-account member can still sync from one detailed raid stage to its human group leader's stage *within the same expansion*; no human raid-tier rewrite.
- Respect `ProgressionLimit`: **reject recorded stages above the cap** rather than relabeling TBC/Wrath progression as Vanilla. Classify valid stages from their actual IP tier, not raw character level; never force a bot to a stage beyond the limit when enforcement is ON.
- When enforcement is ON, check all *persisted* group member slots (not only currently online members) on invitation and acceptance. If an existing member is offline or their stage cannot be safely read, conservatively **deny new invitations/acceptances**. This prevents an unverified mixed-expansion group from admitting another member but **may interrupt ordinary party invitations while one member is offline**. Requires owner gameplay review, exact Core API check, and manual in-game regression. OFF behaviour is unaffected.

## Specific changes and preservation

`src/IndividualProgressionGroupPolicy.h`: small pure, constexpr stage/race/limit policy; invalid stages/cap rejected. Includes a **test-only** F14 starting-floor rule in `IsAllowedRandomBotStage`. It is **NOT YET CALLED** by active server code; do not mistake it for a completed Blood Elf/Draenei rndbot fix.

`src/IndividualProgressionPlayer.cpp`: replace two existing complex grouping branches with consistent expansion predicate in `OnPlayerCanGroupInvite` and `OnPlayerCanGroupAccept`. OFF remains a separate original-behaviour route; when ON, same-era bots may still inherit the leader's detailed tier.

`src/IndividualProgression.cpp`: `SyncBotsProgressionToLeader` skips cross-era writes when enforcement is ON; when OFF its previous behaviour remains.

`tests/group_expansion_policy.cpp`: exhaustive checks of all 0–19 stage pairs across Vanilla/Blood Elf/Draenei race combinations, plus boundaries, caps and F14 tier-floor model.

`tests/check_group_expansion_source.py`: static source guards. Isolated GitHub runner compiles and executes the **actual header** in C++17 and C++20. It is not a full Worldserver integration test.

**Unchanged by design:** original Hunter spell source, Warlock logic, Playerbots, Naxxramas module, character DB/quests, SQL, DBC, client patches, config default and server state.

## IMPORTANT: not yet implemented — F14 positive-rndbot starting floor

Owner's separate rule: **Blood Elf/Draenei random Playerbots start in TBC stage 8 and can NEVER enter stage 0–7**, including on creation, login, level-change, group sync, manual stage change or after relog. During a Vanilla server cap they must be **ineligible for Vanilla population/grouping**, NOT forcibly lifted to stage 8 in a way that bypasses the cap.

The current IP `OnPlayerLogin` assigns stage 0 to all non-normal-account characters level <=60. The existing `ForceUpdateProgressionState` and related group paths also lack a positive RandomBot-only floor. IP's `isBotAccount` uses an account-name regular expression and is **not sufficient proof of live rndbot identity**; upstream Playerbots exposes a more specific RandomBot manager but current installed source and thread-safety must be verified. The correct fix may span IP and Playerbots' bot-generation/activation paths. **Do not silently set a human, Altbot, SelfBot or unknown identity to tier 8.**

For safety, this PR must remain **DRAFT and not be merged** until either the F14 code independently closes the known gap or a separately owner-approved staged deployment explicitly accepts the gap. The TBC-race grouping check in this PR does NOT fix the floor when `EnforceGroupRules=0`.

## Required verification before a code merge or deployment

1. **Code review/build:** exact owner compatible Core/Playerbots/module SHAs, full Worldserver compile and link, source preserving Hunter draft #1 and any locally edited PB header. Static/pure C++ tests are not full integration.
2. **Enforce ON:** human vs human in *different raid tiers of same expansion* allowed; cross-expansion denied in either invitation direction and when accepting; bot/Altbot/excluded-account controls tested; Blood Elf and Draenei humans denied in Vanilla, allowed in TBC+.
3. **Enforce OFF:** compare with previous behaviour for human invites, bot tier sync and existing mixed groups. No new config option.
4. **Hard edges:** offline/invisible party members, accepting invitations after progressing, leader changes, group formation before and after stage transition, GM/admin policy, IP disabled, malformed/unknown stages, `ProgressionLimit=7/8/12/13`, world/instance traversal, concurrent map workers. Verify game results; a compile is not enough.
5. **F14 separate acceptance:** positive rndbot identity, race 10/11 stage floor on login/summon/manual changes/group acceptance and Vanilla server cap, no human or Altbot side effects, visibility WHO/in-world and interaction semantics per #9. Do not claim this is fixed by a pure policy header.
6. **Backups/rollback:** preserve installed branch/commit, binary and character DB backups before any owner-approved live tests. Revert isolated PR in Git before merge; for a future release, revert the reviewed commits and restore known-good code/config/binary and relevant character state only with owner approval.

## Source status log

- **2026-10-30:** Code and tests proposed on isolated branch. [Hosted standalone run 41129586089](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41129586089) **PASS** after an independently inspected original job log: C++17 3,676/3,676; C++20 3,676/3,676; 11/11 static source-wiring checks. An earlier CI run failed because the **test's expected result omitted the second group member's race**; test fixed, then hosted CI passed. Also guarded negative/out-of-range `ProgressionLimit` from uint8 truncation and protected disabled-IP group updates. Historical pinned full Worldserver compile remains queued/unverified, no live game or deployment test. Source review and owner approval still required.
