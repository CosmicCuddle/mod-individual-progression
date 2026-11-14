# F14 guild isolation (owner Option B): coverage and cross-repo proposal (14 Nov 2026)

## Status and scope

**Status:**
- source and design only;
- a candidate on the isolated branch `feature/cs03-ip-f14-guild-isolation-20261114`, not in the accepted social candidate;
- no Core or Playerbots edits were made, and nothing is live.

**Pins:**
- **Core25:** `395b68ee` (the candidate `240f6250` has the same `src/`);
- **Playerbots:** `405f8119` plus PR #21.

**Owner rule** (Option B, Issue #13 comment 6876819935, with the specific new-join instruction in 6876817653):
- a still-Vanilla human (real client) and a positively verified Blood Elf/Draenei RandomBot must not share guild membership or guild chat;
- real human Blood Elf/Draenei characters, Altbots, SelfBots and unknown identities are outside the rule;
- eligible (TBC/Wrath) humans interact normally.

**Boundary (CS01 6876847293), widened by the owner on 14 Nov 2026:**
- new joins, **and a still-Vanilla human logging in** to a guild (owner: "Yes remove them on login", Issue #13 comment 6877216646);
- there is no bulk sweep of existing mixed guilds (only per login, per guild);
- a real human is never removed;
- ranks and leadership change only by Core's stock succession inside `DeleteMember`.

## 1. Coverage inventory (Core25 source)

| Path | Where | Seam | Covered by IP? |
|---|---|---|---|
| Client invite | `Guild::HandleInviteMember` | proposed `OnPlayerCanGuildInvite` (Core patch helper, owner-approved) | yes, with the guild patch |
| Client accept | `Guild::HandleAcceptMember` → `AddMember` | `_LogEvent(JOIN_GUILD)` → `GuildScript::OnEvent` | yes: remediation after the add |
| Direct `AddMember` (online or offline GUID; Playerbots `PlayerbotFactory::InitGuild` `:5065`, GM `.guild invite`) | `Guild::AddMember` `:2257-2339` | same `OnEvent(JOIN_GUILD, guid)` at `:2332`, **always**, with the GUID even when `Player*` is null | yes: remediation after the add |
| Guild creation (charter or `.guild create`) | `Guild::Create` | leader `AddMember` ⇒ the same event | yes (the creator is the only member, so no mix) |
| Guild chat (client) | `ChatHandler.cpp:474-485` → `OnPlayerCanUseChat(…, Guild*)` → `Guild::BroadcastToGuild` | sender-level hook only | **partly:** a restricted bot sender is blocked; a human sender can't be filtered per recipient |
| Officer chat (client) | `ChatHandler.cpp:487-499` | same | same |
| Bot guild chat | Playerbots `PlayerbotAI.cpp:2954` calls `guild->BroadcastToGuild(bot->GetSession(), …)` **directly** | **no hook at all** | **no** |
| Guild roster, MOTD, events, bank logs | `Guild::Handle*`, `BroadcastPacket` | none | **no** (membership removal ends it) |
| A human logs in to a guild that became mixed while they were offline | `OnPlayerLogin` | existing player hook | **yes** (owner decision): a still-Vanilla human's login is marked like that human's join, with the human facts; the same proven removal path runs at the next world update |
| A human drops back to Vanilla (era regression) while in a guild with a restricted bot | stage change | none (by owner decision) | **at the next login**: owner accepted (14 Nov 2026) that nothing happens at the stage change itself; that human's next login runs the login check and removes the restricted bots |

## 2. IP-local implementation (candidate)

### Membership
1. **Join source:** `GUILDHOOK_ON_EVENT` with `GUILD_EVENT_LOG_JOIN_GUILD`.
   - It carries the joiner's GUID for every successful add, online or offline, from any thread.
   - This closes the "offline add, null Player" gap; `OnAddMember` (which needs a live `Player*`) is no longer used.
2. **Pending set:** `PendingGuildJoins`, keyed `(guild id, joiner)`.
   - It **never drops** an entry. Entries coalesce, so between two world updates it holds at most one entry per membership created in that interval, and it's emptied every world update.
   - This closes the "lossy queue" gap.
3. **Facts captured at the join:** on the world thread, the joiner's facts (still-Vanilla human / restricted bot) are recorded with the entry.
   - If the joiner logs out before the world update, the captured positive facts are used.
   - Otherwise an offline bot counts only if it was confirmed at its last logout.
   - Anything else is **UNRESOLVED**: logged, no action, never guessed.
   - This narrows the "logout before drain" gap to joins made off the world thread whose joiner logs out before the next update.
4. **Decision (world thread):**
   - **Vanilla human joined:** every restricted bot member is removed.
   - **Restricted bot joined:** it is removed if an **online** still-Vanilla human is a member.
5. **Removal:** `Guild::DeleteMember(guid, false, true, false)`, the stock kick flags of `HandleRemoveMember`, with `canDeleteGuild = false`.
   - **Before every call:** the guild is looked up again; the target is still a member; **member count ≥ 2**, so `DeleteMember` can never reach its sole-member `Disband()` branch; and the reason still holds (the joining human is still a member, or an online still-Vanilla human is).
   - **No calendar cleanup:** stock kick (`HandleRemoveMember`) doesn't do it; only voluntary leave does.

### Chat
- **Covered:** `OnPlayerCanUseChat(…, Guild*)` on the world thread, for guild and officer chat. A **restricted bot** sender is refused while a still-Vanilla human member is online.
- **Not covered:** a human's message is never blocked here, because that would silence the human towards everyone. There's no reply (no broadcast spam).

### Default OFF
- When OFF, nothing is recorded, the drain only clears, and the chat gate returns allow.

### Transient window
- Remediation runs on the **next world update** after the add.
- Core has already stored the member and broadcast `GE_JOINED` by then.
- So for up to one world-update interval, the roster and chat are shared.
- This is **not** admission prevention.

## 3. Minimal proposals for other lanes (not implemented here)

### P-1 (Core, default-true): guild chat recipient filter
In `Guild::BroadcastToGuild`, inside the recipient loop and before `SendDirectMessage`:
```cpp
if (!sScriptMgr->OnGuildChatRecipient(this, session->GetPlayer(), player, officerOnly))
    continue;
```
- **What it adds:** a new `GuildScript` boolean hook with an enum entry appended before `GUILDHOOK_END`. The default returns `true`, so it is ABI-additive like the guild-invite helper.
- **What it closes:**
  - a human's guild/officer chat reaching a restricted bot;
  - a bot's direct `BroadcastToGuild` call reaching a human, once P-2 has it running on the world thread.
- **IP side:** only on the world thread; both players by pointer, with IP's normal `AllowPairIn` scope.

### P-2 (Playerbots): route bot guild chat through the world thread
- **Change:** `PlayerbotAI.cpp:2954` should queue a `CMSG_MESSAGECHAT` (`CHAT_MSG_GUILD`) packet on the bot's session, as PR #21 does for invite/trade/port, instead of calling `BroadcastToGuild` from the bot AI.
- **Result:** bot guild chat then also gets the client hooks (including IP's sender gate) on the world thread.
- **Also removes** the existing unsynchronised iteration of `m_members` from a map thread.

### P-3 (Core, default-true, optional): admission veto before persistence
In `Guild::AddMember` after the "already in a guild" checks and before `try_emplace`:
```cpp
if (!sScriptMgr->OnGuildCanAddMember(this, guid, rankId))
    return false;
```
- **What it adds:** true **prevention** instead of after-add remediation, with no roster or chat window.
- **Caller impact:** Playerbots' `InitGuild` already logs a failed add, and the GM command reports failure.
- **IP side:** for a still-Vanilla human joiner, decide only on the world thread with an online player. Otherwise it stays "unknown" and the remediation path remains.

### P-4 (Playerbots, optional): bot guild assignment filter
`PlayerbotGuildMgr::AssignToGuild` would avoid guilds that hold a still-Vanilla human. This stops repeated randomizer adds (each one currently ends in removal).

## 4. Explicitly UNRESOLVED (not claimed)
- **Logout before the update, off-thread join:** a joiner added off the world thread (or offline) whose facts are unknown. Logged; no action.
- **Offline human:** resolved by the owner's login decision. The guild stays mixed only until that human's next login (world thread, option ON).
- **Era regression:** decided by the owner (14 Nov 2026): no action at the stage change; handled at that human's next login. Until then the guild stays mixed.
- **Chat:** the human→bot direction (P-1) and bot direct broadcasts (P-1/P-2).
- **Window:** the transient add→update window (P-3).
- **Not built or played:** native build into a server and gameplay.

**Membership compliance is therefore not claimed.** This candidate is "new-join remediation plus restricted-bot client-chat gate".
