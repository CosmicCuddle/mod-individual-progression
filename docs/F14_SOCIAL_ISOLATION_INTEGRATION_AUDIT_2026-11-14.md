# F14 social isolation: integration audit (14 Nov 2026)

## Status and scope

**Status:**
- source and design evidence only;
- nothing here was executed on a realm;
- no gameplay, installed or live claim is made.

**Pins:**
- **IP social:** branch `feature/cs03-ip-f14-social-isolation-20261113`. Runtime is identical to `e62de448`.
- **Playerbots:** PR #21 `06b6c7ae` (parent `405f8119`; first reviewed at `0737ed3e`).
- **Core25:** `395b68ee`. The candidate Core `240f6250` adds only a test and a workflow file; `src/` is identical.

**Owner rules applied:** the decisions recorded in `docs/F14_SOCIAL_ISOLATION_2026-11-13.md`:
- every surface is owner-approved;
- the T1 rule allows by default;
- option B applies;
- Information #10 (owner, 14 Nov 2026): the temporary grouped-era floor applies to verified RandomBots only, and Altbots never have their stage changed by it.

**Terms:**
- **H** = a still-Vanilla human: `HumanStillVanilla` — present, in world, a **non-headless session** (a real client), a player-security account, not in GM mode, not a verified RandomBot, IP stage below 8.
- **Headless Altbots/SelfBots** are **neither** side of F14 social isolation: not H (no client session) and not R (not a verified RandomBot). They're never restricted, whatever their race or stage.
- **Ordinary expansion grouping** (`EnforceGroupRules`) treats Altbots like other player characters under its own rule. That rule is separate from social isolation.
- **R** = a verified Blood Elf/Draenei RandomBot. It is never inferred from race alone.
- **OFF** = `IndividualProgression.F14.RandomBotSocialIsolation = 0` (the default).

## 1. End-to-end event matrix

Each row lists, in order: the initiator, the target, the thread, the Core entry, IP's decision, the reply path, the state that must not appear, the Playerbots dependency, and the behaviour when OFF.

| Surface | From → to | Thread | Core entry | IP decision | Reply | Must not appear | Playerbots dependency | OFF |
|---|---|---|---|---|---|---|---|---|
| Whisper | H → R | world (client chat) | `OnPlayerCanUseChat` | block | one per pair per window | the whisper | none | stock |
| Whisper | R → H | map (bot AI) | same | block if the owned map holds both, else allow (T1) | none | the whisper | none | stock |
| Trade | H → R | world | `TradeHandler.cpp:842` `OnPlayerCanInitTrade` | block | yes | `m_trade` on either player (`:846-847`) | none | stock |
| Trade | R → H | world (#21 queued packet) | same | block (full scope) | none | same | #21 `TradeAction.cpp:44` | stock |
| Group invite | H → R | world | `GroupHandler.cpp:89` `OnPlayerCanGroupInvite` | block | yes | any invite or group (`:164+`) | none | stock |
| Group invite | R → H | world (#21) | same | block (full scope) | none | same | #21 `InviteToGroupAction.cpp:50` | stock |
| Group invite | human master on a bot's behalf → R | world (human session queue) | same | block | yes, to the master | same | #21 `:284`/`:434` | stock |
| Group accept | R accepts an invite to H's group | world (#21 `GroupAcceptOperation`) | `GroupHandler.cpp:257` `OnPlayerCanGroupAccept` (S1 checks every member) | block | none | membership; `SetMaster`/strategies/greeting/summon | #21, epoch gate **OPEN** | stock |
| Group accept | H accepts into a group holding R | world | same | block | yes | membership | none | stock |
| Group add (bot login) | master's bot → master's group | world (`GroupInviteOperation`) | PB #21 `06b6c7ae` preflight `OnPlayerCanGroupInvite(master, botName)` before `Create`/`AddMember` | social: master is the initiator. Ordinary grouping: `EnforceGroupRules` checks the invitee against the inviter and every member | social: master only, if R (unreachable under option B) | group create/convert/add | PB #21 `c5b9d6c3`+ | stock |
| Duel | H ↔ R | caster's map / world | `AllSpellScript::OnSpellCheckCast` (`SPELL_EFFECT_DUEL`) | `SPELL_FAILED_BAD_TARGETS` if the owned map holds both | H as caster: one, queued to the world thread | the duel request | none | stock |
| Mail (receiver online) | H → R | world | `OnPlayerCanSendMail` | refuse | yes | the mail item | none | stock |
| Mail (receiver offline) | H → R | world | same | refuse if the GUID is in the confirmed set | none | the mail item | none | stock |
| Guild invite | H ↔ R | world (client, or PB `GuildManagementActions.cpp:66` queued packet) | patched `Guild.cpp` `OnPlayerCanGuildInvite`, after every Core check | block | H as inviter: yes | `SetGuildIdInvited`, the invite packet | Core guild patch (not landed) | stock (hook absent ⇒ compiled out) |
| /who | H views R | session/WHO | `OnF14ShouldExcludeWhoEntryForViewer` (Core WHO macro) | exclude the row | none | the row | Core WHO macro | stock |
| Dungeon Finder | proposal mixing H and R | LFG worker | `CanCreateLfgProposal` (values only) | refuse the candidate | none | the proposal | none | stock |
| BG queue join | solo R or H | world | `OnPlayerCanJoinInBattlegroundQueue` | record the human / veto the bot | none | — | none | stock |
| BG port | R into an instance with H | world (#21 queued) / map | `BattleGroundHandler.cpp:430` | refuse | none | the port | #21 `AcceptBattlegroundInvitationAction.cpp:21` | stock |
| BG matcher | groups mixing H and R | world (after the map phase) | `CanAddGroupToMatchingPool` | refuse the candidate group | none | selection into the match | none | stock |

## 2. Adversarial stale-state cases

| Case | Expected behaviour (source) | Evidence |
|---|---|---|
| Logout then relog (human or bot) | Per-player facts are erased at logout (`QueueFacts().Erase`, `BgQueued().Erase`). A bot is remembered for mail only if verified; it's forgotten at its next login | Script `OnPlayerLogout`/`OnPlayerLogin`; contracts |
| Map transfer or teleport | Per-thread owned-map records are refreshed by `OnPlayerBeforeUpdate` and expire by tick. Nothing is keyed by map | `OwnedScopeValid` native tests |
| Queue leave and rejoin (LFG) | Facts are overwritten at the rejoin; a party record is overwritten under its group GUID | `Record`/`RecordGroup` |
| Queue leave and rejoin (BG) | (b) entries are re-checked live and pruned; (d) reads live | (b)/(d) source |
| Invite cancelled then re-sent | Core `RemoveInvite` / a new invite; IP keeps no invite state; each attempt is decided anew | `GroupHandler.cpp:249` |
| Group leader change | IP keeps no leader state. Core LFG `OnChangeLeader` keeps the queue (membership unchanged) | `LFGScripts.cpp:283-297` |
| Group disbanded and recreated | A new group GUID. A stale party record is inert (never named by a new proposal) and is erased when any member logs out | `Erase` |
| OFF → ON → OFF | ON: every check is live; queued players are unknown until they re-queue. OFF: every veto returns allow straight away; replies already queued drain within one world tick | Section 4 |
| Identity revoked | `ReadBot` is live on every check. The offline set is cleared at the bot's next login | mail lifecycle |
| Server restart | Every structure is static and in memory only, so it starts empty | Section 3 |
| Stale bot operation across a relog (PB) | Not an F14 bypass: Core and IP checks run at execution with live facts. PB #21 `06b6c7ae` adds login epochs (bot and inviter/master) and invitation generations, so a stale operation is refused | Issue #13 comment 6876246814 §7; §12 below |
| Member added during the Dungeon Finder role check | **F-2:** the tracked party record gains the member and its facts (world thread, option ON) | native `TestRoleCheckMemberRefresh`; contract `test_f2_role_check_member_refresh` |
| Character deleted / created with a reused GUID | **F-3:** the offline-mail entry for that GUID is forgotten (also when OFF) | native `TestCharacterCreateDeleteCleanup`; contract `test_f3_create_delete_forget_offline_mail` |

## 3. Caches and registries at the latest head

| Structure | Writer | Reader | Threads | Removed when | After restart | If stale | UNKNOWN |
|---|---|---|---|---|---|---|---|
| `ReplyLimiter<1024>` | reply decision | same | any thread, `ReplyLock` | slot reuse after the cooldown | empty | a stale slot only delays one reply | none |
| `PendingReplies<256>` | map-thread block decision | world `OnUpdate` | `PendingRepliesLock` | drained every tick | empty | both players are looked up again; anyone gone gets nothing | none |
| `QueueFactsTable` (`_facts`, `_groups`) | `OnPlayerCanJoinLfg` (world, option ON); **F-2** `GroupScript::OnAddMember` (world, ON, tracked party in role check only) | `CanCreateLfgProposal` (LFG worker) | `QueueFactsLock` | logout (the player and every party listing them); overwritten at rejoin; **group disband** (`OnDisband`, also OFF) | empty | unknown ⇒ allow | none |
| `BgVanillaRoster` | `OnBattlegroundAddPlayer` (world, ON) | port check | `BgLock` | leave / destroy (regardless of the option) | empty | missed humans are covered by (d) | none |
| `BgQueuedHumans` | BG queue join (world, ON) | (b) bot join | `BgLock` | live re-check plus prune; logout; BG add | empty | re-checked live | none |
| `ConfirmedRestrictedBots` | logout (world, ON, verified R) | mail (world) | `ConfirmedLock` | that GUID's next login; **character create or delete** (F-3, also OFF) | empty | over-blocks mail only for that GUID, until create/delete/login | none (cluster GUID reuse is no longer a dependency) |
| `WorldThreadMarker` | `WorldScript::OnUpdate` | all hooks | atomics | — | unset until the first tick (until then everything is allowed) | — | none |
| `thread_local OwnedMapRecord` | `OnPlayerBeforeUpdate` | same thread | per thread | tick expiry | — | an expired record ⇒ no scope ⇒ allow | none |
| Reply text | config load/reload | reply send (world) | `_f14SocialIsolationReplyLock` | reload | config | — | none |

## 4. Default OFF

`OptionOn()` requires `enabled && f14RandomBotSocialIsolation`.

**Hooks that return allow first when OFF:**
- whisper, trade, guild (`AllowPairIn`);
- group invite and accept;
- mail; LFG join and proposal;
- BG queue join, port, matcher and add;
- duel; /who.

**Ungated, and cleanup only:**
- login `Forget`; character create/delete `Forget` (F-3);
- logout erase; group disband erase (F-2);
- BG leave/destroy removal;
- the world-tick marker;
- draining replies already queued.

None of these blocks anything or changes gameplay.

**Nothing is written while OFF:** no recording (LFG facts, BG roster, BG queued, confirmed set) happens unless ON.

## 5. Dungeon Finder party changes: **closed (source); the role-check case is fixed by F-2**

**Core source (`LFGScripts.cpp`):**
- `OnAddMember` (`:155-195`): if the group is `LFG_STATE_QUEUED`, it calls `LeaveLfg(gguid)`. A member who was queued solo is also removed.
- `OnRemoveMember` (`:197-224`): it calls `LeaveLfg(guid)` while the member is still mapped to the group. `LFGMgr::LeaveLfg` (`LFGMgr.cpp:911-946`) then sees the group GUID and removes the **whole group** from the queue.

So a party that changes while queued always leaves the queue. To requeue it must rejoin, and IP records fresh facts at that rejoin.

**The narrow case:** a member added during `LFG_STATE_ROLECHECK`.
- `OnAddMember` doesn't leave the queue in that state.
- `UpdateRoleCheck` (`LFGMgr.cpp:1570-1574`) queues the party with the role-check roles from the join.
- IP's party record comes from the join too, so a human H added during the role check isn't in IP's member list for that party GUID.
- **Impact:** narrow. Joining the party already went through IP's group-accept check against every member, so H can't join a party that contains R. The only gap is a cross-party proposal that pairs this party with a separately queued R.
**F-2 (implemented, CS01-approved hardening):**
- `F14SocialIsolationGroupScript` subscribes to `GROUPHOOK_ON_ADD_MEMBER` and `GROUPHOOK_ON_DISBAND` only.
- **Add:** the gate is option ON, a valid group and GUID, and the world thread, before any read. Then:
  1. only a party IP already tracks;
  2. only while `sLFGMgr->GetState(group) == LFG_STATE_ROLECHECK`;
  3. the member is read on the world thread and its facts appended under `QueueFactsLock`.
- **Never** creates a party record.
- **Why role check only:** a QUEUED party is removed from the queue by Core on any add (`LFGScripts.cpp:178-179`), so its record is inert. The order of IP's and Core's group scripts doesn't matter for the role-check state, because Core's LFG script doesn't change it on an add.
- **Disband:** erases the party record (group GUIDs are never reused), so `_groups` is bounded by live groups plus logout cleanup.
- **The LFG worker veto still reads values only.**

## 6. Cluster GUID and offline mail

**Without cluster mode** (`sToCloud9Sidecar->ClusterModeEnabled() == false`): character GUIDs only count up for the life of the process (`ObjectGuid.h:305-315`, seeded from `MAX(guid)`), so GUIDs are never reused.

**With cluster mode:** GUIDs come from the external sidecar (`ObjectGuid.cpp:106-125`). Whether they're reused can't be proven from this source, so it stays **UNKNOWN**. Whether this server enables cluster mode is a read-only runtime check (C0).

**What depends on it:** only the offline-mail set. A reused GUID can only mean an earlier character with that GUID was deleted.
- **If it happens:** an H mailing a *new* offline character with a reused GUID is refused until that character's first login.
- **Direction of failure:** over-blocking mail only, never a leak.

**F-3 (implemented, CS01-approved hardening):**
- `OnPlayerDelete(guid)` and `OnPlayerCreate(player)` both call `Confirmed().Forget(guid)`, unconditionally (option OFF too).
- They never remember or classify anyone; no database access.
- **Core ordering:** a refused deletion goes to `OnPlayerFailedDelete` and returns before `OnPlayerDelete`. `OnPlayerDelete` runs on the accepted path just before `Player::DeleteFromDB`. `OnPlayerCreate` runs after the creation transaction succeeded.
- **Limit:** a database commit failure after `OnPlayerDelete` isn't visible to this hook. The only effect would be a forgotten entry, i.e. no block until that bot's next logout.
- **Result:** cluster-GUID non-reuse is **no longer a correctness dependency** of the offline-mail set. Whether cluster mode is enabled stays a runtime inventory fact (C0).

## 7. Guild hook helper against the candidate Core

**Insertion point:** `Guild::HandleInviteMember`, after every Core check (not found, ignored, member limit, faction, already in a guild, already invited, rank right).

**Mutation order:** the hook runs before the success result, `SetGuildIdInvited`, the event log and `SMSG_GUILD_INVITE`. So a refusal leaves no state.

**Enum:** `PLAYERHOOK_ON_F14_CAN_GUILD_INVITE` is appended just before `PLAYERHOOK_END`, so nothing is renumbered.

**ABI:** a new virtual function in `PlayerScript` changes its vtable layout. Every module must be rebuilt against the patched Core. That's normal for an AzerothCore build, but a binary-only module mix is unsafe.

**OFF compatibility:** without the macro the IP script is compiled out. The default returns `true`.

**Pins:** the candidate Core `240f6250` has the same `src/` as `395b68ee`, so the 112 anchors and the patched syntax check carry over. The landed SHA must be rechecked.

**Residual:** Playerbots adds bots straight into bot guilds (`PlayerbotFactory.cpp:5065` `guild->AddMember`) without asking any hook. Owner policy covers guild *invites*; whether bot-only guild fills that later receive humans matter is a **CS01 classification**.

## 8. Regression list (none of these may change)

| Population | Expectation |
|---|---|
| Ordinary humans (any stage) with ordinary humans | stock everywhere |
| TBC/Wrath humans with R | stock everywhere |
| Ordinary RandomBots (not Blood Elf/Draenei, or not verified) | stock |
| Headless Altbots / SelfBots (any race, any stage) | **neither H nor R**: never restricted by social isolation. Ordinary grouping (`EnforceGroupRules`) applies its own rule |
| GM targets / GM viewers | stock (no special-casing; identity decides) |
| Blood Elf/Draenei human characters | not R; never hidden or blocked as bots |
| Unknown identity (unverified, offline, unreadable thread) | allow (stock) |
| Race alone | never classifies anyone as R (contract plus native tests) |
| Option OFF | byte-for-byte stock decisions |

## 9. Information #10: grouped-era floor population (**resolved by the owner, 14 Nov 2026**)

**Owner decision** (Information #10 comment 6876429606):
- verified RandomBots only;
- Altbots never have their stage changed by this system;
- a verified Blood Elf/Draenei RandomBot has an absolute TBC minimum. It may sync upward (for example to Wrath) and later fall back only to TBC;
- race alone never establishes RandomBot identity.

**Variant B is rejected** and must not be wired. Expectations (test-only here; this branch wires nothing):

| Case | Expected |
|---|---|
| Verified Blood Elf/Draenei RandomBot | TBC floor; never used as a Vanilla-era bot; may sync up to Wrath and fall back no lower than TBC |
| Verified RandomBot of another race | stock grouped-era sync |
| Blood Elf/Draenei Altbot (any account) | stage never changed |
| Blood Elf/Draenei human-played character | stage never changed |
| Unverified bot, Blood Elf/Draenei race only | stage never changed |

## 10. Composition manifest (no merge or rebase)

| Item | Head | Parent / contains | Touches |
|---|---|---|---|
| IP social | `69b39493` (runtime `e62de448`) | `aed05a92` (IP172) | runtime IP, conf (3 keys OFF), tests, docs, register |
| #177 | `281c2ca` | contains #174 `3bf97b4` | tests/docs/register |
| #176 | `971ad2c` | contains #175 | tests/docs/register |
| #178 | `701c4a4` | — | docs/register |

- **Tree conflicts:** only `docs/CUSTOM_MODIFICATIONS_REGISTER.md` (×3, resolved by a union). Config union = the social keys only, all `0`.
- **Build dependencies:**
  - the Core guild macro is optional (the script part is compiled out without it);
  - the Core WHO macro is optional;
  - PB #21 is needed for bot-AI routes to reach the hooks on the world thread.

## 11. Runtime test-realm script (disposable, not executed)

**Setup:**
- a disposable realm built from IP social + Core (with and without the guild patch) + PB #21;
- `MapUpdate.Threads` = 4, then 0;
- characters: H (stage 0), T (stage 8+), a verified Blood Elf/Draenei R, a non-BE/Draenei RandomBot, a Blood Elf/Draenei Altbot of H's account;
- the option ON, cooldown 60.

**Steps:**
1. **C0 (read-only):**
   - record the build SHAs;
   - `MapUpdate.Threads`;
   - cluster mode;
   - `Playerbots.RandomBotAccountPrefix`;
   - `MaxRandomBots` / `AddClassAccountPoolSize`;
   - the macros present.
2. **Each matrix row (section 1):** perform the action from both sides. Observe the client result (no whisper, trade window, invite or duel; mail error) and count replies (≤ 1 per 60 s).
3. **Section 2 cases:** relog, teleport, requeue, leader change, disband, OFF→ON→OFF via `.reload config`, restart.
4. **PB #21:** PBD1–3, PBB1–4. Repeat with a backlog (many bots) for queued-operation delay.
5. **F-1 (ordinary grouping, `EnforceGroupRules = 1`):** a T master groups with R, then logs in a Vanilla Altbot. Expected: PB #21's preflight is refused by IP's grouping rule, so no group is created/converted and the Altbot isn't added. With `EnforceGroupRules = 0` the add proceeds (stock).
6. **F-2:** a party queues; during its role check a Vanilla H joins; a solo R is queued for the same dungeon. Expected: no proposal pairs them.
7. **F-3:** a fixture GUID reuse isn't reproducible without cluster mode, so test the observable halves: (a) remember R (logout), delete R, then H mails that name/GUID → no refusal; (b) a newly created character is never refused before its first login.

**Cleanup:** delete the disposable characters; restore the config.

**Rollback:** the option `0`, or a binary built without the branch.

## 12. Playerbots PR #21 at `06b6c7ae` (re-review)

| Gate | Result (source) | Evidence |
|---|---|---|
| Login epoch (bot) | **PASS** | `IssueLogin` in `OnBotLogin` (after the duplicate/AI-null early returns, before any operation is built); `RetireLogin` in `LogoutPlayerBot`, `DisablePlayerBot` and `OnPlayerBeforeLogout` |
| Login epoch (human) | **PASS** | `IssueLogin` in the player script's `OnPlayerLogin` for non-headless sessions; retired in `OnPlayerBeforeLogout` |
| One login, one epoch | **PASS** | a process-wide counter starting at 1, under a mutex; at `UINT64_MAX` it issues 0, which fails closed (no operation is queued or run); erased at retire, so the map is bounded by online players |
| Invitation generation | **PASS** | `IssueInvitation` on `GROUPHOOK_ON_INVITE_MEMBER` (Core `Group::AddInvite`, `Group.cpp:388`, after `SetGroupInvite`); cleared at login/retire |
| `GroupAcceptOperation` | **PASS** | captures bot epoch + inviter epoch + invitation generation at queue time (0 ⇒ not queued). `EpochsMatch()` is checked in both `IsValid()` and first thing in `Execute()`. A→logout→A, inviter relog, and re-invite before the drain are all refused |
| Direct group add (`GroupInviteOperation`) | **PASS** | bot and target epochs are captured in the constructor and checked in `IsValid`/`Execute`. `OnPlayerCanGroupInvite(bot, targetName)` runs after live resolution and the target-in-group check, **before** `new Group`/`Create`/`AddGroup`/`ConvertToRaid`/`AddMember` |
| IP interaction | **PASS** | every route reaches IP on the world thread (`anyPlayer` scope). The preflight's initiator is the master. Social applies only if the added bot were R, which option B prevents. Ordinary grouping applies `EnforceGroupRules` |
| Option B and dispatch ordering | unchanged from `0737ed3e` | Issue #13 comment 6876246814 §1–6 |
| Native / gameplay | **UNKNOWN** | not built here |

## 13. F-4: Playerbots bot-guild direct add (classification only)

- **Callers:** `PlayerbotFactory::InitGuild()` (`PlayerbotFactory.cpp:5040-5070`), reached from factory randomization (`:1048`) and `RandomPlayerbotMgr.cpp:2893`. It calls `guild->AddMember(bot, rank)` for a guild that `PlayerbotGuildMgr::AssignToGuild` picked, or creates a new bot guild.
- **Population:** RandomBots only (`RandomBotGuildCount` 20, `RandomBotGuildSizeMax` 15 by default; skipped when `DeleteRandomBotGuilds`).
- **Could a human end up there?** Only if a human joined that bot guild earlier, by invite from a member. With the guild patch, an invite between a restricted pair is refused. An invite from a non-restricted member (another human or a non-BE/Draenei bot) is allowed.
- **Effect:** a later direct fill could put a verified Blood Elf/Draenei RandomBot into a guild that already holds a still-Vanilla human. That shares only guild chat and roster. No whisper, trade, group, duel or mail rule is bypassed, because those are decided per pair.
- **Classification:** the owner-approved surface is the guild **invite**. Guild **membership/chat** isn't an approved surface. **No implementation without an explicit owner decision.**

## Findings needing a decision

- **F-1 (Playerbots):** **closed at PB #21 `c5b9d6c3`+** (preflight). It was an ordinary expansion-grouping admission gap, not social isolation (CS01 6876278508).
- **F-2 (IP):** **implemented** (section 5).
- **F-3 (IP):** **implemented** (section 6).
- **F-4 (Playerbots guild fill):** classification only (section 13); an owner decision is needed only if guild membership/chat should be isolated.
