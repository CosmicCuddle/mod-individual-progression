# IP expansion group enforcement — direct AddMember bypass audit

**Owner:** CosmicCuddle  
**Status:** Confirmed source-level integration gap; **NO IMPLEMENTATION OF THE FIX** in grouping DRAFT PR #3.  
**Historical source pins:** Grimfeather Core `8daffdabe9202c228e45d833c9a1d399597a15cd`; Playerbots `e88cc4099e69f35126b28942a412ca702d20453b`. **These are not confirmed owner-host current SHAs.**  
**Change ownership:** Any core or Playerbots edits require a distinct, approved reviewable branch/PR; this document does not authorise one. No production/DB/config changes.

## Owner's observed baseline (important positive evidence)

CosmicCuddle reports from their established server use that `EnforceGroupRules` **does already block bots being invited or joining through the ordinary invitation flow**. **Preserve this confirmed owner-observed behaviour** and do not describe the current running server's normal bot grouping as broken. This report is **not** a controlled demonstration of the direct-add code path or a determination of whether the specific source pin is installed on the running host.

A more focused trace of the historical Playerbots source confirms `src/Bot/PlayerbotMgr.cpp:591–620` queues `GroupInviteOperation(masterGUID, botGUID)` from `PlayerbotHolder::OnBotLogin` when a bot has a master and needs to be added/re-added to a group (or a new group created). `GroupInviteOperation::Execute` subsequently calls `Group::AddMember` directly, so the potential gap is **bot-login/master rejoining**, *not* an ordinary invite from the client. `src/Ai/Base/Actions/InviteToGroupAction.cpp` and `AcceptInvitationAction.cpp` go through the standard packet handlers and are consistent with the owner's observed blocks.

This proves that an alternative source path exists in that **historical Playerbots revision**, **not** that a cross-expansion bot actually joined the owner's live server. Verify installed Playerbots SHA and reproduce the bot-login rejoin sequence safely in a development environment before proposing any Core/Playerbots hook or claiming this is a live defect.

## Why the grouping draft is not complete

The IP GroupScript/PlayerScript predicates currently apply in `OnPlayerCanGroupInvite` and `OnPlayerCanGroupAccept`. Those hooks run in the ordinary group-invite handler:

- [historical Core GroupHandler.cpp](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Handlers/GroupHandler.cpp#L65-L90) sends an invitation through `OnPlayerCanGroupInvite`.
- The [accept handler](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Handlers/GroupHandler.cpp#L240-L260) invokes `OnPlayerCanGroupAccept`.
- But [Core Group::AddMember](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Groups/Group.cpp#L427-L500) inserts the member and calls `OnGroupAddMember` **after** the insertion, without calling either PlayerScript eligibility hook. `ScriptMgr.h` exposes the post-add hook, not an examined pre-add `CanGroupAddMember` veto.

### Confirmed Playerbots direct group-member paths

| Source (Playerbots historical pin) | Direct operation | Consequence |
| --- | --- | --- |
| [`src/Script/WorldThr/PlayerbotOperations.h:38–99`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Script/WorldThr/PlayerbotOperations.h#L38-L99) | `GroupInviteOperation::Execute` creates group for bot if needed and calls `group->AddMember(target)` around line 79 | **Bypasses IP's ordinary invite AND accept eligibility checks**, potentially making mixed-expansion bot-originated groups when `EnforceGroupRules=1` |
| [`src/Script/WorldThr/PlayerbotOperations.h:310–357`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Script/WorldThr/PlayerbotOperations.h#L310-L357) | `ArenaGroupFormationOperation` creates arena group and calls `newGroup->AddMember(member)` | Special arena path bypasses ordinary checks; whether the owner's broad expansion policy applies to arena group formation is **not resolved**. Do not unilaterally disrupt arenas |
| [`src/Ai/Base/Actions/BattleGroundJoinAction.cpp:139–173`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Ai/Base/Actions/BattleGroundJoinAction.cpp#L139-L173) | Directly creates arena team group and calls `group->AddMember(member)` | Separate arena/BG code path; no IP pre-admission check |
| [`src/Ai/Base/Actions/InviteToGroupAction.cpp:25–51`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Ai/Base/Actions/InviteToGroupAction.cpp#L25-L51) | Calls `HandleGroupInviteOpcode` | **Does** take normal IP invite hook; good control case |
| [`src/Ai/Base/Actions/AcceptInvitationAction.cpp:21–47`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Ai/Base/Actions/AcceptInvitationAction.cpp#L21-L47) | Calls `HandleGroupAcceptOpcode` | **Does** take normal IP accept hook; good control case |

**No demonstrated global guarantee:** other modules or Core pathways may invoke `Group::AddMember` without packet handlers. A successful standalone grouping policy test cannot prove they all enforce the new rule.

### Do NOT "fix" this in the post-add callback

The existing IP `IndividualPlayerProgression_GroupScript::OnAddMember` is called **after** the member was inserted. Ejecting the member or reversing group creation in that callback is a potentially unsafe mutation/reentrancy scenario and does not prove full prevention. Current code's bot syncing there is a separate responsibility. Avoid ad hoc DB checks from map-worker threads.

## Candidate implementation options (not owner-approved)

1. **Minimal central Core pre-admission seam**, if required and compatible: a new, narrowly scoped, default-off/group-hook pre-veto in `Group::AddMember` calling the IP era policy through an authoritative interface, **before modifying group state**, including bot-direct membership, with explicit precedence for arena/BG special groups and scripts. Requires owner approval to edit Core; source-ABI, hook registration and feedback/error paths must be checked. This may be the most comprehensive guarantee if the owner requires all group routes.
2. **Targeted Playerbots API guard** in `GroupInviteOperation` and arena group-formation paths, using one shared IP policy. Less Core intrusive, but it must cover every direct add route and future updates and cannot claim to cover arbitrary other modules. Requires appropriate Playerbots fork/branch, owner compatibility checks and agreement with other module developers.
3. **Documented, limited scope**: only normal player/bot invitations are enforced by IP. This is **not** equivalent to the owner's current "only Vanilla with Vanilla" requirement and must never be silently chosen.

**Recommended next review:** confirm the actual installed Core/Playerbots SHAs and existing custom hooks before selecting option 1 or 2. Coordinate with Information and existing CS03/Naxx work; no duplicate implementation without ownership assignment. Preserve current deployed behaviour until approved and tested.

## Tests required before any merge

- Normal human-human, human-rndbot, human-Altbot group invitation/acceptance: same era/different raid stage allowed, cross era blocked, race-10/11 and Vanilla denied when enforcement ON.
- **Direct** `GroupInviteOperation` bot-created group (not ordinary `HandleGroupInviteOpcode`): Vanilla↔TBC denial and same-era allowed, including bot group leader and race-10/11.
- Arena group formation and BattlegroundJoinAction direct AddMember: explicit approved semantics and special-case tests; preserve vanilla BG and custom arena configurations.
- Third-party module direct Group::AddMember safety, group member slot creation and offline membership.
- EnforceGroupRules OFF original behaviour unchanged; IP disabled no enforced grouping; real human visibility unchanged.
- No pre-accept progression quest mutation on failed group creation. Sync only after successful add, stage-0 and nonzero bot sync, mandatory F14 rndbot stage floor independent of grouping toggle.
- Full native Worldserver Core + IP + Playerbots + Naxx module compile, map worker concurrency review, 3.3.5a client gameplay, backup and reversal path before any approved deployment.

**No conclusion of native compile or runtime correctness from this source audit.** The PR must remain DRAFT, unmerged, until the direct-add bypass has a tested/approved treatment.


## Second-pass source audit: pre-admission safety (30 Oct 2026)

**Scope:** read-only, source-matched engineering review of the historical pinned Core \`8daffdabe9202c228e45d833c9a1d399597a15cd\` and Playerbots \`e88cc4099e69f35126b28942a412ca702d20453b\`. This is not a new implementation, live bug reproduction, or approval to change Core/Playerbots.

### Newly confirmed lifecycle facts

1. **A new group's first member uses the same API.** In [Core Group.cpp, Group::Create](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Groups/Group.cpp#L130-L200), ordinary group creation inserts its group row, then calls \`ASSERT(AddMember(leader))\`. BG/BF creation also uses \`AddMember(leader)\`. A hypothetical pre-add veto must always permit a valid *first/seed* leader in an otherwise empty group, or Core group creation can assert/leave incomplete state. Do not try to compare this member to an absent group leader. A candidate core hook requires separate review of Group::Create error handling, not an assumption that any arbitrary veto is safe.
2. **Playerbots may create or alter a group BEFORE the member is considered.** [PlayerbotOperations.h, GroupInviteOperation::Execute](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Script/WorldThr/PlayerbotOperations.h#L35-L100) can \`new Group\`, call \`Create(bot)\`, register it with \`sGroupMgr\`, and convert a 5-member party to a raid **before** \`group->AddMember(target)\`. A Core pre-add veto protects membership, but cannot retroactively undo these already-completed creation/conversion side effects. A future separately approved Playerbots preflight **before group creation/raid conversion** (or explicit safe rollback) may be warranted if the owner requires no failed-attempt side effects.
3. **Restored groups use another route.** [Core Group.cpp, Group::LoadMemberFromDB](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Groups/Group.cpp#L225-L270) pushes persisted slots and updates caches without \`AddMember\`. An admission hook will **not retroactively validate historic saved groups**. Preserve existing groups and character data; propose diagnostics/deny *new* mismatched joins, not automatic disband or quest-stage rewriting. Any clean-up requires a distinct owner decision and tested backup/rollback.
4. **Progression can change during group lifetime.** A Vanilla group member reaching TBC (or a GM changing a tier) does not inherently invoke \`AddMember\`. Even a complete admission hook cannot guarantee a permanent same-era invariant after admission. Decide separately whether to warn, defer, disband, block entry, or leave the existing group until its next membership change. **No silent disband** or human tier downgrade.
5. **Existing GroupScript hook is after the fact.** [Core Group.cpp, Group::AddMember](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Groups/Group.cpp#L427-L515) appends the member, updates cache, group, character DB and sends group updates **before** \`sScriptMgr->OnGroupAddMember\`. [GroupScript.h](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Scripting/ScriptDefines/GroupScript.h) exposes \`OnAddMember\` and battleground **queue** eligibility, but no observed pre-insert membership veto. Never eject a member from this post-add callback as a substitute for prevention.

### Proposed direction for owner review — not approved code

**Preferred for an all-ordinary-group guarantee:** evaluate a new, side-effect-free, default-allow \`GroupScript\` **pre-admission** decision from \`Group::AddMember\`, *before* slot/cache/DB/packet modifications. Implement the actual IP predicate in the IP module, default-allow when \`EnforceGroupRules=0\` or IP disabled. Permit valid empty-group seed members. Check ALL existing member slots including offline/unknown fail-closed policy as currently drafted, unless the owner chooses a different offline rule. Return an ordinary failure to the caller without modifying member/quest state. Review hook ordering/ABI and preserve all existing GroupScript hook identifiers; adding a new hook is a Core change requiring explicit approval and compatibility checks.

**Additional Playerbots-side preflight may be needed** to avoid group creation or forced party-to-raid conversion when a would-be member is incompatible, even if Core subsequently denies \`AddMember\`. Apply the same IP predicate, not an independent second definition. Preserve ordinary packet invite behaviour; \`PlayerbotHolder::OnBotLogin\` auto rejoin and arena/BG operations must be separate tests. Do not call existing \`OnPlayerCanGroupAccept\` blindly from generic Core admission: when the switch is OFF, its current legacy path can call \`ForceUpdateProgressionState\` and alter bot quest state, and ordinary packet flow has already called that hook. A new pre-admission callback should be **read-only**.

**Special groups unresolved:** do not assume the same policy must deny battleground, battlefield, LFG, arena-team formation or arena queue membership. Identify intended scope and offer explicit per-group treatment for CosmicCuddle to approve. Preserve \`MapUpdate.Threads=8\`, bot identity restrictions, human visibility and F14 rndbot-only tier-8 floor as separate obligations. The core hook would be opt-in behaviour when enforcement ON, not a new config switch.

### Required negative and side-effect tests BEFORE any approved merge

| Scenario | Expected review target |
| --- | --- |
| \`Group::Create\` with one Vanilla/TBC leader, including race 10/11 | Group created normally, no invalid first-leader rejection or assert |
| Bot reconnects to human master's group with mismatched era | Direct world-thread \`GroupInviteOperation\` does **not** add member; no tier writes |
| Same-era bot rejoin / stage-0 pre-MC / party already at five members | Legitimate join still works; raid conversion only if admission will proceed |
| Cross-era bot rejoin into an existing five-person party | Reject before any unwanted raid conversion, DB member row, summon or teleport |
| Bot-directed new group with incompatible target | No unintended persistent orphan group or player progression mutation |
| Server reloads pre-existing group with an offline member | No destructive change; safe handling of the next *new* admission |
| Human's tier advances to TBC while remaining with Vanilla party | Record actual continuing-group behaviour; owner decides later response |
| Arena/BG/BF/LFG groups, leader change and IP disabled/OFF | Preserve established behaviour unless owner explicitly approves another rule |
| F14 race 10/11 rndbot vs Vanilla-cap server, humans/Altbots negative control | No cap bypass, no mistaken human concealment; distinct F14 coverage needed |

**Status:** This addendum is review/documentation only. It cannot declare a live bypass, successful full exact-head build or corrected gameplay. Grouping DRAFT PR #3 remains frozen at \`e0cd785f6b2090c8f099c824811077d0ab2a9de5\` during its native CI; this documentation is prepared on a *separate stacked branch*, not a new code revision of that run. Owner decision, cross-module code ownership, staging tests, backups, and separate merge/deployment approval are all still required.
