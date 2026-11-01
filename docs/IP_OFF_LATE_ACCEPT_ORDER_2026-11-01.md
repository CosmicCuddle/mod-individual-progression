# OFF group invitation — late Core rejection after an IP progression write

**Date:** 1 November 2026. **Owner:** CosmicCuddle. **Author:** Server-Side-Maintenance CS01-IP. **Disposition:** negative diagnostic / historical pinned source regression, **NOT A FIX**. This is a review-only child of [IP DRAFT PR #20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20), exact parent `5fe0f9105d55d249aa55a25510d3fa23e3d84148`. Does not amend #20 or its frozen parent #19.

## Why this is a distinct test, not duplication

Existing [IP DRAFT #9](https://github.com/CosmicCuddle/mod-individual-progression/pull/9) tests the **Playerbots direct** `Group::AddMember` bypass and potential preflight ordering; [IP #15/#16](https://github.com/CosmicCuddle/mod-individual-progression/pull/16) test offline-leader guard before quest reads. This review instead pins the **ordinary player's accepted invitation that Core REJECTS AFTER calling IP's accept hook**, when `EnforceGroupRules=0`. It intentionally does not test or replace PR #9's separate direct bot-add model.

## Exact historical source claims

1. [IP combined child #20's unchanged PlayerScript](https://github.com/CosmicCuddle/mod-individual-progression/blob/5fe0f9105d55d249aa55a25510d3fa23e3d84148/src/IndividualProgressionPlayer.cpp) `OnPlayerCanGroupAccept`: for a known online leader with enforcement OFF and a joining `!isNormalAccount` bot/excluded account at a different quest stage, `ForceUpdateProgressionState(player, leaderStage)` is called **in the accept predicate itself**. The call removes/reissues rewarded IP stage quest markers. The ON branch correctly defers all such writes until later membership; the missing/offline leader C3 guard protects the earlier null case.
2. [Pinned Grimfeather Core `GroupHandler.cpp` `8daffdabe9202c228e45d833c9a1d399597a15cd`](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Handlers/GroupHandler.cpp): `HandleGroupAcceptOpcode` invokes `OnPlayerCanGroupAccept` **before** the subsequent trial-level/member-cap, attempted self-invite, full group, and spectator/missing-leader creation checks. These checks can still reject the invitation without calling `group->AddMember`.
3. [Pinned Core `Group.cpp` same SHA](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Groups/Group.cpp): after `Group::AddMember` records the new member slot and persists the group, it invokes `OnGroupAddMember`. In the IP [PlayerScript source](https://github.com/CosmicCuddle/mod-individual-progression/blob/5fe0f9105d55d249aa55a25510d3fa23e3d84148/src/IndividualProgressionPlayer.cpp), existing `GroupScript::OnAddMember` then calls `SyncBotsProgressionToLeader(group)`.
4. [IP `IndividualProgression.cpp` current #20](https://github.com/CosmicCuddle/mod-individual-progression/blob/5fe0f9105d55d249aa55a25510d3fa23e3d84148/src/IndividualProgression.cpp): the ordinary post-add `SyncBotsProgressionToLeader` currently **returns on a stage-0 leader while enforcement OFF**, but the earlier accept hook can set the joining bot to stage 0. Simply deleting the unsafe pre-accept write would silently change legitimate successful OFF tier-zero syncing. Adding unconditional tier-zero post-add writes could affect direct PB `Group::AddMember` and BG/BF/leader-creation groups that did not use the same invitation path.

## What this test actually checks

- Strictly the **three new non-runtime files** in this child branch. All inherited Hunter, grouped progression, F14, GM and configuration blobs remain identical to #20. No register edit is necessary for a diagnostic that creates **no custom runtime modification**.
- Disposable GitHub Actions checks out historical pinned Core SHA and current IP exact commit. The Python tests pin source ordering across `GroupHandler`, `Group::AddMember`, IP `OnPlayerCanGroupAccept` and `SyncBotsProgressionToLeader`.
- A deliberately tiny **negative witness** illustrates that an OFF bot stage8 joining a stage0 leader can be rewritten to stage0 even if a **later** Core rejection prevents membership. A green diagnostic means it has successfully detected/retained the historical hazard, **not that a fix exists**.
- Source tests do **not** execute C++, invoke Playerbots, simulate all Core session outcomes, mutate quests, read live DB rows or demonstrate a live incident. Absence of error in a synthetic model is not a guarantee.

## Constraints for the future real correction

**Required behaviour:** OFF (0) legacy successful invite stage sync including **stage0** remains; failed invites must not change 66001–66018 quest-marker state. ON (1) same-expansion admission and post-join bot sync stay enforced. Unknown leader fails closed when ON and avoids quest writes when OFF. No changes to real human characters, account classifications, legitimate owner commands or other established custom features.

**Do not blindly use:** stage0-only `GroupScript::OnAddMember` special case, a stale pending-invite map, a post-add kick, or a global `Group::AddMember` alteration without verifying BG/BF/arena, direct Playerbots injection, raid creation, realm-thread ordering and rollback. A temporary pending-state design must prove rejection cleanup and no stale state across unrelated bot direct-add or later invites.

**Next accepted code task** (separate draft, not authorised by this test): identify a proven invitation-success-only callback/hand-off, or a minimal Core/PB extension seam, that moves OFF `ForceUpdateProgressionState` after the actual membership commit with no change to tier-zero semantics; otherwise report a blocker and request CosmicCuddle's specific approval of any changed behaviour. In a disposable, backed-up test server, exercise trial restriction, full group, self-invite, leader missing/spectator, successful OFF 0/7/8/13, ON 0/8 mismatch, human/Altbot/excluded, PB direct group add, relog and rewarded quest rows before/after. Never use a live character to demonstrate the hazard.

## Status and rollback

This child adds no IP, Core, PB or Naxx runtime change, no SQL, config, worldserver launch, upload, merge, or production operation. **Discard this test branch** to roll it back. Parent #20 remains DRAFT with new-head native CI not yet confirmed at this test's creation; #19's historical pinned full Worldserver build success must not be misattributed to #20. Owner-installed source/config/DB/backups remain UNKNOWN. Grok's independent review is paused on user instruction.
