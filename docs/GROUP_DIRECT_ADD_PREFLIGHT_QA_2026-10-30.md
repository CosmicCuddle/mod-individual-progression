# Playerbots direct group-add preflight source tests — review-only

**Owner:** CosmicCuddle  
**Date:** 30 October 2026  
**Parent:** docs-only [IP grouping bypass audit DRAFT #5](https://github.com/CosmicCuddle/mod-individual-progression/pull/5) at `858b2aafca7e76aacc4c4f6af2e514073d128caf`; upstream parent is frozen executable [grouping DRAFT #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3).  
**Status:** **S1/S3 pinned source study only.** NOT a new group veto, safe to merge, compiled C++ change, gameplay pass, or installed source verification.

## Specific gap this test protects against forgetting

In exact historical [Playerbots `GroupInviteOperation`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Script/WorldThr/PlayerbotOperations.h), direct `group->AddMember(target)` can run after `new Group`, `group->Create(bot)`, `sGroupMgr->AddGroup(group)`, and `ConvertToRaid()`. Ordinary [bot invite packets](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Ai/Base/Actions/InviteToGroupAction.cpp) and [accept packets](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Ai/Base/Actions/AcceptInvitationAction.cpp) do invoke Core's ordinary packet handlers, so preserve CosmicCuddle's reported existing blocks on those ordinary paths. [`PlayerbotMgr.cpp::OnBotLogin`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/PlayerbotMgr.cpp) can queue the direct operation for master-bot rejoining.

In exact historical [Core `Group.cpp`](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Groups/Group.cpp), `Group::Create` **persists the new group before seeding leader via `ASSERT(AddMember(leader))`**. Direct `Group::AddMember` itself mutates member slots, group cache, character DB and sends group updates before IP's `OnGroupAddMember` callback. This is why **post-add eject/kick is not a safe substitute for admission preflight**, and why a naive Core pre-veto on leader seed can break group creation. Arena direct group formation can disband previous groups and must NOT be affected before the owner selects the applicable exception policy.

## What this isolated PR tests

1. Read **exact pinned** Core and Playerbots files in disposable checkouts and verify the above source ordering and packet-handler distinction.
2. Detect representative deliberate synthetic source mutations (missing direct add, out-of-order post-hook, raid conversion moved).
3. Execute a pure model of **proposed** preflight-first ordering: when expansion policy denies a normal master-bot rejoin, it must reject *before* group creation, DB leader seed, raid conversion, AddMember, bot summon, or tier sync; if allowed, existing later actions remain reachable. With `EnforceGroupRules=0`, legacy flow must remain unchanged.
4. Explicitly hold arena/BG/LFG and mid-party progression decisions for CosmicCuddle; no guessed override.

**Strict three-path scope:** new Python source/regression test, new isolated workflow and this memo. No executable or upstream sources changed. The pinned checker intentionally fails and asks for **re-audit** if future Core/Playerbots source structure differs; passing means the historical risky call ordering still exists and is understood, **not** that a new restriction has been implemented. No new data or network from the test itself; GitHub Actions checkouts use public pinned commit sources.

## Required follow-up *after* owner approval

Match actually installed source and local patches; verify eligibility data is available before group creation on the appropriate **world-thread** integration seam; decide central Core hook versus targeted Playerbots protection and special group policy; keep stock leader seed; add **real** C++ negative-path tests demonstrating zero group/DB/raid mutation when rejected; run exact combined build and staging group rejoin/normal invite/arena/BG tests; document backup and rollback; get separate approval before any production change.

[Coordination #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10) remains blocked for policy/source matching; no production or owner data was accessed.
