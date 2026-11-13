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

**Consequence for the BE/Draenei minimum-TBC group floor (Codex #175/#176):**
- That floor *protects* (it prevents a downgrade), so its `eligibleBot` input should cover **at least the whole sync write population**. Otherwise a Blood Elf/Draenei Altbot or excluded account could still be synced down to a Vanilla leader with `EnforceGroupRules = 0`.
- Feeding it from the verified RandomBot cache alone would leave that hole.
- Feeding the social restriction from `!isNormalAccount` would be wrong in the opposite direction, because Altbots and excluded humans would be restricted.
- Whether the owner's "BE/Draenei bots" rule includes Altbots and excluded accounts is **UNKNOWN**. It's recorded for CS01/the owner; this branch doesn't decide it.

## What is left unchanged
- These are new, separate scripts. Core ANDs boolean hooks across scripts, so every existing IP hook (including the `EnforceGroupRules` invite/accept checks) is byte-identical and still runs.
- No world-visibility query is subscribed. Visual disappearance is paused.
- No stage, quest, DB, Playerbots-manager or config write. No `SyncBotsProgressionToLeader` or group-sync change (Codex owns the group TBC floor).
- One OR-reason was added to the existing login identity gate, so the verified cache is populated when this option is on (the same pattern as earlier F14 options). The option is forced OFF without the Playerbots API.

## Threads
- **Client-initiated (a real player's packet):** the four opcodes (`CMSG_MESSAGECHAT`, `CMSG_INITIATE_TRADE`, `CMSG_GROUP_INVITE`, `CMSG_GROUP_ACCEPT`) are `PROCESS_THREADUNSAFE` in Core25 (`Opcodes.cpp`), and `MapSessionFilter` refuses them. They therefore run in `World::UpdateSessions` while no map is updating, so reading the other player (and, for accept, every member) is safe.
- **Bot-initiated (Playerbots AI):** on the locally inspected Playerbots `0d754d19`, bot AI runs from `OnPlayerAfterUpdate`, inside the bot's **map** update. Its invite, accept and trade actions call `HandleGroupInviteOpcode`, `HandleGroupAcceptOpcode` and `HandleInitiateTradeOpcode` directly, and its whispers use `Player::Whisper`. So the same hooks fire there, but on a **map thread**. When the other player is on a different map, the hook reads that player's quest status from another thread. Core's own handlers do comparable cross-map reads on that path, but this is still a **known residual risk (T1)**. A proposed fix is awaiting CS01; see the Gaps section.
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
- **Direct group adds:** on the historical Playerbots pin `e88cc409`, earlier audits found group creation calling `Group::AddMember` directly. On the locally inspected `0d754d19`, ordinary invite and accept go through Core's handlers, so the hooks fire. The only direct `AddMember` left is arena-team group formation in `BattleGroundJoinAction`. Its members come from the bot's own arena team, so this needs no change here. Which Playerbots version is installed is **UNKNOWN**.
- **T1 — bot-initiated hooks run on a map thread:** see the Threads section. Proposed fix, not applied: record the world thread id from a `WorldScript::OnUpdate` subscription. Off the world thread, a player on a **different map** would be treated as having unknown facts, which is the existing "unknown means not restricted" fallback, so nothing would be read across threads.
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
- **Workflow:** `workflow_dispatch` only (milestone). It includes a **Core25 anchor job** pinning 33 lines:
  - the four hook declarations and enums, plus `BuildChatPacket`;
  - Core's wrong-faction checks before the whisper (ChatHandler.cpp:428 → :448) and trade (TradeHandler.cpp:830 → :842) hooks;
  - the /who exclusion (MiscHandler.cpp:401) before `matchCount++` (:407), so the visible list and count stay consistent;
  - for the group-accept member scan: the accept hook call, `Group::GetMemberSlots` and `MemberSlot::guid`, `ObjectAccessor::FindPlayer` returning in-world players only, and the four social opcodes being `PROCESS_THREADUNSAFE` (with `MapSessionFilter` refusing them), so they run on the world thread.

## Evidence levels and rollback
Source and model proof, plus a local header syntax check. No Worldserver link, gameplay or client test has been done.

**Rollback:** keep the option `0` (the default), or close the DRAFT. No host, DB, config or client was changed.
