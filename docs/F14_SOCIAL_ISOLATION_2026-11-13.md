# F14 safer social isolation for verified Blood Elf/Draenei RandomBots

**Owner:** CosmicCuddle. **Developer:** CS03-IP. **Lead/review:** CS01-IP.
**Date:** 13 November 2026.
**Owner pivot and claim:** Coordination #13 [6851131248](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6851131248), claim [6851259769](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6851259769); workload board #52.
**Base:** IP #172 `aed05a92f262362608f5d643c8570b504189aae0`. **Core:** Core25 `395b68ee` hooks.
**Status:** DRAFT, default OFF. This replaces the paused positive world concealment; nobody is hidden in the world.

## Behaviour
With `IndividualProgression.F14.RandomBotSocialIsolation = 1`, the pair is restricted when both of these hold.

**The bot side, all required:**
- the login-thread verified RandomBot identity (`HasF14VerifiedRandomBotIdentity`);
- Blood Elf or Draenei;
- a headless session;
- not in GM mode.

**The human side, all required:**
- a real (non-headless) client;
- a player-level account;
- not in GM mode;
- not a verified bot;
- an in-memory IP stage below 8.

For a restricted pair:

| Surface | Hook (Core25) | Effect |
|---|---|---|
| Whisper | `OnPlayerCanUseChat(…, Player* receiver)`, inside `Player::Whisper` after Core's receiver, filter and faction checks | declined both ways |
| Trade | `OnPlayerCanInitTrade`, after Core's faction, distance and state checks | declined both ways |
| Group invite | `OnPlayerCanGroupInvite` | declined both ways |
| Group accept | `OnPlayerCanGroupAccept` | the leader is checked both ways (pair rule). A Vanilla human is also refused if **any other current member** is a restricted bot; that member sends the reply. Offline or unresolved members are skipped |
| /who | `OnF14ShouldExcludeWhoEntryForViewer` | the bot is left out of the human's list **and** count. Core calls this only in builds with its `COSMIC_F14_WHO_GUID_QUERY_EXPERIMENT`, and skips the row before counting |
| Dungeon Finder | `OnPlayerCanJoinLfg` (records facts) + `GlobalScript::CanCreateLfgProposal` (veto) | a candidate group mixing a still-Vanilla human with a restricted bot is refused; Core matches other combinations. TBC/Wrath humans are stock. Owner decision 13 Nov 2026 |
| Battlegrounds | `OnPlayerCanJoinInBattlegroundQueue` (b), `OnPlayerCanBattleFieldPort` (a), roster from the battleground add/leave/destroy hooks | (b) a restricted bot's solo queue join is quietly refused while a still-Vanilla human waits in a matching queue and level bracket; (a) its "enter battle" is refused when it's invited to an instance holding a still-Vanilla human (either team). TBC/Wrath humans are stock. Owner decision 13 Nov 2026, option (c) |
| Battleground matching (d) | `AllBattlegroundScript::CanAddGroupToMatchingPool` | the matcher never puts a still-Vanilla human and a restricted bot into the same match, **whichever queued or entered first**: a candidate group is refused if the other kind is already selected for the match (either team), inside the target instance, or invited to it. The refused group waits for another match. Owner-approved option (d) |

**The in-character reply:**
- **When:** only when the human started the interaction. Addon whispers get no reply.
- **What:** the bot sends one whisper. Default: *"You are not yet ready for the road I walk. Return when your journey has carried you into a later age."* It's configurable, and empty means no reply.
- **How it's sent:** built with `ChatHandler::BuildChatPacket(CHAT_MSG_WHISPER, LANG_UNIVERSAL, bot, human, …)` and sent only to the human's own session. It never goes through `Player::Whisper`, so it can't trip the block or bot chat logic, and nothing is written to the bot.
- **Text safety (CS01 review hardening):** before sending, the configured text is normalized to one line. Every CR, LF, NUL, tab, other C0 control byte and DEL becomes a space, and surrounding spaces are trimmed. It's capped at **512 UTF-8 bytes**, cut without splitting a multi-byte character. That's a module-level safety cap, not a claimed Core protocol limit. The default text is unchanged, and text that is empty or becomes empty means the interaction is declined silently.
- **Rate limit:** at most one reply per (human, bot) pair per cooldown (default 60 s, clamped 5–3600 s), in a bounded table of 1,024 pairs under a mutex. If every slot is still cooling down, the interaction stays declined but there's no reply. A backwards clock never produces a reply.

**Resumes immediately:** every attempt is decided afresh from the live stage, so the restriction lifts the moment the human earns stage 8. Nothing is cached except reply timestamps.

## Never decided from race alone
- Humans have a real client.
- Altbots aren't `IsRandomBot`.
- SelfBots have a real client.
- GMs, and anything unknown, don't qualify.

None of these is ever restricted. An unknown human stage (out of world) is treated as "not Vanilla", so the historical behaviour applies. The /who path uses the cached GUID's verified identity plus the cached race; race alone never excludes anyone.

## Identity sources across F14 lines (shared-identity contract)
Two identity notions are in use, and they are **deliberately different**. A source test (`IdentitySources`) pins both, so a change to either shows up.

| Purpose | Population | Source | Why |
|---|---|---|---|
| **Restriction** (this feature) | verified RandomBots only | IP's login-thread verified cache, plus headless and not GM | it *limits* what a character can do. A false positive would cut off a real human, Altbot or SelfBot, so it needs positive proof |
| **Group sync writes** (existing IP `SyncBotsProgressionToLeader`) | every non-normal account: bot-account **or** excluded-account match | `!isNormalAccount(member)` | historical IP behaviour; unchanged |

**Wider identity map (v2 item 5).** These predicates may differ, but the provenance marked *shared* must be the same source:

| Consumer | Predicate | Provenance (must be shared?) | May differ because |
|---|---|---|---|
| Social restriction | verified RandomBot + headless + not GM + race 10/11 | the login-thread verified cache (**shared**) | it limits interaction, so it needs positive proof |
| /who exclusion | the verified cache **by GUID** + cached race | the same verified cache (**shared**); race comes from Core's /who cache | /who has only a GUID; its equivalence with the social rule is tested (720 sets) |
| Group sync population | `!isNormalAccount` (account-name regex) | IP account match | historical sync behaviour; it protects, never restricts |
| #176 `eligibleBot` (future producer) | must be **exactly the sync population** for that member | the same IP account match as the sync (**shared with sync**, not with social) | a floor that skipped part of the sync population would let a BE/Draenei character be synced down |
| Auction snapshots (#53, not started) | UNKNOWN | if it isolates RandomBots it must use the verified cache; if it follows sync, the account match | no auction design exists yet; an Information reply is pending |

**Never allowed anywhere:** race alone; Playerbots manager queries off the login thread; promoting an unknown classification.

**Consequence for the BE/Draenei minimum-TBC group floor (Codex #175/#176):**
- That floor *protects* (it prevents a downgrade), so its `eligibleBot` input should cover **at least the whole sync write population**. Otherwise a Blood Elf/Draenei Altbot or excluded account could still be synced down to a Vanilla leader with `EnforceGroupRules = 0`.
- Feeding it from the verified RandomBot cache alone would leave that hole.
- Feeding the social restriction from `!isNormalAccount` would be wrong in the opposite direction, because Altbots and excluded humans would be restricted.
**Group-floor population contract** (CS01 6858469463, plus the owner's answer):
1. **Population first.** The BE/Draenei TBC floor applies only to characters **already** in the existing group-sync write population, i.e. those `SyncBotsProgressionToLeader` writes to (`!isNormalAccount`).
2. **Then the race floor.** Only inside that population does race 10/11 raise the minimum to TBC.
3. **No expansion by race.** Race never adds anyone to the population. Ordinary human accounts stay outside, and nothing is classified as a bot from race.
4. **The population is account-name based**, not Playerbots-identity based:
   - `isBotAccount` matches `IndividualProgression.BotAccountsRegex` (default `"^RNDBOT.*"`);
   - `isExcludedAccount` matches `IndividualProgression.ExcludedAccountsRegex` (default empty).

**What this means with the shipped defaults:**
- RandomBot accounts are in the population.
- **Altbots are not:** they're characters on a player's own (normal) account, so IP never syncs them. That matches the owner's answer (13 Nov 2026): "Altbots should get the same treatment as normal players". They keep their own earned era and are never synced down, so the floor never needs to act on them.
- An earlier relay of mine, which said Altbots are written to by the sync, was wrong for the default configuration. This corrects it.

**UNKNOWN:**
- The running server's regex values. If an operator widened either regex, those accounts are in the population and get the floor.
- Excluded accounts are empty by default; if configured, they are in the population (CS01 rule).

This social feature is unaffected: it never restricts Altbots.

## What is left unchanged
- These are new, separate scripts. Core ANDs boolean hooks across scripts, so every existing IP hook (including the `EnforceGroupRules` invite/accept checks) is byte-identical and still runs.
- No world-visibility query is subscribed. Visual disappearance is paused.
- No stage, quest, DB, Playerbots-manager or config write. No `SyncBotsProgressionToLeader` or group-sync change (Codex owns the group TBC floor).
- One OR-reason was added to the existing login identity gate, so the verified cache is populated when this option is on (the same pattern as earlier F14 options). The option is forced OFF without the Playerbots API.

## Threads
- **Client-initiated (a real player's packet):** the four opcodes (`CMSG_MESSAGECHAT`, `CMSG_INITIATE_TRADE`, `CMSG_GROUP_INVITE`, `CMSG_GROUP_ACCEPT`) are `PROCESS_THREADUNSAFE` in Core25 (`Opcodes.cpp`), and `MapSessionFilter` refuses them. They therefore run in `World::UpdateSessions` while no map is updating, so reading the other player (and, for accept, every member) is safe.
- **Bot-initiated (Playerbots AI):** on the locally inspected Playerbots `0d754d19`, bot AI runs from `OnPlayerAfterUpdate`, inside the bot's **map** update. Its invite, accept and trade actions call `HandleGroupInviteOpcode`, `HandleGroupAcceptOpcode` and `HandleInitiateTradeOpcode` directly, and its whispers use `Player::Whisper`. So the same hooks fire there, but on a **map thread**. Reading a player on a **different** map from there would be a cross-thread read (T1). **T1 guard (owner decision 13 Nov 2026: allow, i.e. fail-open):** off the world thread, **no foreign `Player` is touched** until ownership is proven (CS01 review 6860967009 rev 2).
  - **How ownership is proven:**
    - At the start of every player update (`OnPlayerBeforeUpdate`), the thread records that player's own map and the current world tick.
    - The thread itself is updating that map, and each map is updated once per tick; the world waits for all of them before the tick ends.
    - So, for the rest of that tick, the recorded map is either the one this thread is updating or one it already finished, which nothing else updates until the next tick.
    - A pair is read only if that record is from the current tick **and both players' pointers are found in that map's own player list**. That's pointer comparison only; neither candidate object is dereferenced.
  - **Group accept off the world thread:** the acceptor must pass the same check. Leader and members are resolved only from the owned map's list, never through a global lookup.
  - **Anything else:** nothing is read, and the pair is not restricted (the existing "unknown means not restricted" fallback; owner decision: allow).
  - **Client-initiated attempts** run on the world thread and are unchanged.
  - **The world thread and tick** come from a one-line `WorldScript::OnUpdate` marker that reads nothing.
- **What the adapter reads:** only race, GM flag, session headless flag and security, in-memory quest status, and IP's lock-protected verified cache.
- The reply text is behind a small mutex, because `.reload` can rewrite it.
- The /who hook reads only the viewer's own state and the verified cache, by GUID.

## Gaps (no Core change requested)
- **Guild invite:** no script hook in Core25.
- **Duel request:** `OnPlayerDuelRequest` is notify-only.
- **Mail:** a hook exists, but an offline receiver can't be verified. Not included.
- **/who:** inert in Core builds without `COSMIC_F14_WHO_GUID_QUERY_EXPERIMENT`.
- **Stage range:** only the Vanilla-vs-TBC restriction for now. Later-era isolation (TBC human vs Wrath bot) is not in scope.
- **Group accept and offline members:** an offline or unresolved member has no live facts, so it is skipped and never guessed from race. A group whose only restricted bot is offline can still be joined. That bot is again checked on every later interaction once it is online.
- **Group membership that bypasses invite/accept (v2 item 4; source only):** Core25 calls `Group::AddMember` directly, with no pre-add veto hook, in these places:

  | Path | Where | Can a restricted RandomBot and a Vanilla human end up grouped? |
  |---|---|---|
  | Dungeon Finder | `LFGMgr.cpp:1807` | **Yes.** RandomBots queue through Playerbots `LfgActions` (a queued join packet, `0d754d19`) |
  | Battleground raid | `Battleground.cpp:1274` | **Yes**, as a temporary battle group |
  | Arena | `Arena.cpp:136`; Playerbots `BattleGroundJoinAction` | only through arena-team membership |
  | Wintergrasp | `Battlefield.cpp:631` | Wrath content; not reachable on a Vanilla-era server |
  | GM `.group join` | `cs_group.cpp:160` | a deliberate GM action |

  - **On `0d754d19`:** ordinary Playerbots invites and accepts go through Core's handlers, so the hooks fire. The older direct `GroupInviteOperation` add recorded for `e88cc409` is gone.
  - **Which Playerbots version is installed is UNKNOWN.**
  - **Enforcement seams that remain:**
    - `GroupScript::OnAddMember` runs **after** the add, so it can't veto. Undoing the add there would be a group mutation.
    - `OnPlayerCanJoinLfg` and `OnPlayerCanJoinInBattlegroundQueue` are **per-player**, not per-pair: they can't know which humans a bot will meet.
  - **Owner decision (13 Nov 2026):** "If the player queues & they are in vanilla the blood elfs and draenei should be kept out of the queue, if they are in tbc or wotlk then they can join the queue."
  - **Dungeon Finder: implemented.**
    - **Recording facts:** at queue join (`OnPlayerCanJoinLfg`, world thread: `CMSG_LFG_JOIN` is `PROCESS_THREADUNSAFE`, and Playerbots queues that same packet), the joiner and every group member get a GUID-keyed `{restrictedBot, stillVanillaHuman}` record, under a mutex. It's erased at logout.
    - **The veto:** `CanCreateLfgProposal` runs on the LFG worker thread beside the map updates, so it reads **no Player**: only the proposal's player GUIDs (its `roles` map) and those records. If they mix a still-Vanilla human with a restricted bot, that one combination is refused (`LFG_INCOMPATIBLES_REJECTED_BY_SCRIPT`), and Core keeps matching others.
    - **Joining is never blocked.** Unknown GUIDs never restrict.
    - **Stale record:** a human who reaches TBC while still queued keeps the Vanilla record until they re-queue. That's conservative (keeps bots out), never an admission.
  - **Battlegrounds: option (c) implemented** (owner decision, 13 Nov 2026). Core25 has no per-group battleground seam, so two checks combine:
    - **(b) Queue join.** `CMSG_BATTLEMASTER_JOIN` is `PROCESS_THREADUNSAFE`, and Playerbots queues the same packet, so this runs on the world thread.
      - Still-Vanilla humans (and the still-Vanilla humans of a group that joins) are recorded with their queue and level bracket.
      - A restricted bot's **solo** join is refused (quietly, `ERR_BATTLEGROUND_NONE`) while such a human, **re-validated live** (online, still in that queue, still Vanilla), waits in the same queue or the random battleground, at the same bracket.
      - Stale entries are pruned. Group joins are not refused here; (a) covers them.
    - **(a) Enter battle.** Playerbots calls the port handler from bot AI, which is a map thread.
      - Only "enter" (action 1) is checked; leaving a queue is never refused.
      - After the T1 ownership proof, only the bot's own state is read: `IsInvitedForBattlegroundInstance`, checked against a value-only roster of instances that hold a still-Vanilla human.
      - The roster is fed when humans enter (their worldport ack, a world-thread client packet). It's emptied using GUID and instance values when they leave or the instance is destroyed.
      - The check covers either team, because the owner asked for bots to be "kept out of the queue".
    - **(d) Matcher (owner-approved, 13 Nov 2026).** This closes the order gap.
      - **No Core change was needed.** Core25 already calls `AllBattlegroundScript::CanAddGroupToMatchingPool` at **every** pool insertion: fill into an existing battleground, premade, normal match and skirmish.
      - Its queue state (`m_SelectionPools[].SelectedGroups`, `m_QueuedGroups`) is public.
      - All queue updates run in `BattlegroundMgr::Update` on the world thread, after the map phase (World.cpp:1270, after :1255).
      - The hook reads the candidate group's players, the groups already selected (both teams), and for an existing instance its players plus the groups already invited to it. All reads are live, through `ReadHuman`/`ReadBot`.
      - It refuses the candidate if it would meet the other kind. It never mutates the queue, the pools or the battleground.
      - **Whoever is matched first decides; the other kind waits for another match.**
    - **Remaining notes:**
      - (a) and (b) stay as extra layers.
      - A group invited to the same instance from a *different* queue type (for example the random battleground) is covered by (a) and the instance's player list once inside, not by the invited-groups scan.
      - A refused port lets the bot's invite lapse and Core removes it from the queue. With Core's `Battleground.TrackDeserters` on, Core itself logs a "didn't press enter" row for that bot.
      - Arena is unaffected (team-based).
- **T1 residual (owner-approved fail-open; bots rarely start cross-map interactions, players normally do):** a restricted bot on a **different map** can still invite, whisper or trade with a Vanilla human, because the human's stage can't be read safely there. Fail-closed would block that, but would also block bots with every non-Vanilla human on other maps; that's an owner choice.
- **Identity timing:** the verified cache is filled only for bots that log in while an F14 option is on. Bots already online when the option is enabled by `.reload` stay unrestricted until they relog (this fails toward stock behaviour).

## Tests
- **Native** (`tests/f14_social_isolation_policy_test.cpp`, GCC/Clang, C++17/20, with and without NDEBUG):
  - exhaustive bot facts × race: restricted exactly when all 5 facts hold and the race is 10/11;
  - exhaustive human facts;
  - every stage 0–255;
  - per-surface direction and reply rules, and /who;
  - the rate limiter (cooldown, per-pair independence, full table, backwards clock, clamping), plus a 100,000-step spacing run;
  - the default reply has no technical words;
  - reply normalization: every control byte, CR/LF/NUL, oversized text, 2/3/4-byte characters at every cut offset, and a 20,000-case randomised run (always one line, at most 512 bytes, valid UTF-8, trimmed).
  - /who exclusion equals the social restriction for every equivalent fact set (720 combinations), so /who never lists a bot the human can't reach;
  - no reply loops: 600 s of continuous attempts yield exactly 10 replies at a 60 s cooldown, and bot-initiated attempts never reply;
  - group accept: a restricted bot anywhere in the group (first, middle or last, sizes up to 40) refuses a Vanilla human; eligible, GM and bot humans, the option OFF, and every never-restricted member kind (including race-only and offline facts) allow. This is exhaustive over groups of 0–5 members × 8 member kinds × stages 0–18 and unknown.
- **Source contracts** (`tests/test_f14_social_isolation_source.py`):
  - default OFF, and forced OFF without Playerbots;
  - every hook checks the option first;
  - only the four social hooks plus /who, and no world-hiding or mutation calls;
  - verified identity, never race alone;
  - the reply is a direct packet, never `Whisper(`;
  - the stage mirror matches IP's reader;
  - exact one-line login OR change, additive-only edits, existing hooks unchanged, every other file byte-identical;
  - the two identity sources: restriction uses only the verified cache, and the existing group-sync write population is every non-normal account.
- **Local Core25 header syntax check** (`-fsyntax-only`) of the new script and both modified IP sources. Not a link.
- **Workflow:** `workflow_dispatch` only (milestone). It includes a **Core25 anchor job** pinning 88 lines:
  - the four hook declarations and enums, plus `BuildChatPacket`;
  - Core's wrong-faction checks before the whisper (ChatHandler.cpp:428 → :448) and trade (TradeHandler.cpp:830 → :842) hooks;
  - the /who exclusion (MiscHandler.cpp:401) before `matchCount++` (:407), so the visible list and count stay consistent;
  - for the group-accept member scan: the accept hook call, `Group::GetMemberSlots` and `MemberSlot::guid`, `ObjectAccessor::FindPlayer` returning in-world players only, and the four social opcodes being `PROCESS_THREADUNSAFE` (with `MapSessionFilter` refusing them), so they run on the world thread;
  - for T1: `WORLDHOOK_ON_UPDATE` and `OnWorldUpdate` after `sMapMgr->Update`, the before/after player-update hooks at the start and end of `Player::Update`, `Map::Update` calling `player->Update`, `Map::GetPlayers`, and `MapMgr` scheduling every map before `m_updater.wait()`;
  - for the Dungeon Finder: `CMSG_LFG_JOIN` world-thread processing, the join and proposal hooks, `Lfg5Guids` GUIDs and roles, and the LFG update scheduled beside the maps;
  - for battlegrounds: the join, port and worldport-ack opcodes as world-thread, both hook calls, the add/leave/destroy roster hooks, `IsInvitedForBattlegroundInstance`, and `ERR_BATTLEGROUND_NONE`;
  - for the matcher (d): the matching-pool hook enum and declaration, five of its call sites, the public queue state, `GetPlayers`, and the world-thread `BattlegroundMgr::Update` after the map phase.

## Evidence levels and rollback
Source and model proof, plus a local header syntax check. No Worldserver link, gameplay or client test has been done.

**Rollback:** keep the option `0` (the default), or close the DRAFT. No host, DB, config or client was changed.
