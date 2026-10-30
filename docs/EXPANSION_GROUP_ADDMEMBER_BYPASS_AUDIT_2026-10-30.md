# IP expansion group enforcement — direct AddMember bypass audit

**Owner:** CosmicCuddle  
**Status:** Confirmed source-level integration gap; **NO IMPLEMENTATION OF THE FIX** in grouping DRAFT PR #3.  
**Historical source pins:** Grimfeather Core `8daffdabe9202c228e45d833c9a1d399597a15cd`; Playerbots `e88cc4099e69f35126b28942a412ca702d20453b`. **These are not confirmed owner-host current SHAs.**  
**Change ownership:** Any core or Playerbots edits require a distinct, approved reviewable branch/PR; this document does not authorise one. No production/DB/config changes.

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
