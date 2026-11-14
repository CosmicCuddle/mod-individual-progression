# F14 guild policy: Playerbots dependencies (P-2, P-4) and runtime evidence intake (14 Nov 2026)

## Status and scope

**Status:**
- design notes for the Playerbots lane (CS01/Codex) plus an evidence template;
- nothing here edits Playerbots or Core, and nothing is live.

**Pins:**
- **Playerbots:** PR #21 `1cf279183a35721b997033d73270e2d69becad75` (no changes since the last review);
- **Core:** #28 `137d5a27`;
- **IP:** the Core #28 consumer branch `feature/cs03-ip-f14-guild-core28-consumer-20261114`.

## P-2: bot guild chat through the world thread

### Today
`PlayerbotAI::SayToGuild` (`PlayerbotAI.cpp:2938-2958`) calls `guild->BroadcastToGuild(bot->GetSession(), false, msg, LANG_UNIVERSAL)` directly.

Its callers are bot AI actions and helpers, which run on map workers:
- `SayAction.cpp:305, 562`;
- `BroadcastHelper.cpp:44, 90, 628, 644, 666, 670`.

**Consequences:**
- `BroadcastToGuild` iterates `Guild::m_members` and calls `FindPlayer` for every member, off the world thread. This is an existing data-race class.
- Core #28's per-recipient hook runs there too. IP can only decide when both players are proven in the caller's map (T1); otherwise it delivers. So a restricted bot's guild line usually still reaches a still-Vanilla human.
- `OnPlayerCanUseChat(…, Guild*)` is bypassed entirely.

### Minimum change (Playerbots, mirrors PR #21's queued packets)
```cpp
bool PlayerbotAI::SayToGuild(std::string const& msg)
{
    if (msg.empty() || !bot->GetGuildId())
        return false;
    WorldPacket packet(CMSG_MESSAGECHAT);
    packet << uint32(CHAT_MSG_GUILD) << uint32(LANG_UNIVERSAL) << msg;
    bot->GetSession()->QueuePacket(new WorldPacket(packet));
    return true;
}
```
- **Drain:** `PlayerbotHolder::HandleBotPackets` on the world thread (as for invite, trade and port).
- **Path:** Core's `HandleMessagechatOpcode` → the rank-right check inside `BroadcastToGuild` → `OnPlayerCanUseChat(…, Guild*)` → `BroadcastToGuild` on the world thread.
- **Result:** Core #28's recipient hook then decides with full scope.
- **Behaviour change:**
  - the return value no longer reports the rank-right check;
  - Core's client chat checks (mute, length, `|0` filter) now apply to bot text;
  - the rank-right check stays inside `BroadcastToGuild`.
- **Tests (PB lane):**
  - the source contract shows no direct `BroadcastToGuild` call in `src/Ai`, `src/Util` or `src/Bot`;
  - the queued packet layout is `type, lang, cstring`.

## P-4: don't assign bots to guilds with real members

### Today
- `PlayerbotGuildMgr::AssignToGuild` (`PlayerbotGuildMgr.cpp:71-114`) skips only guilds whose cache has `hasRealPlayer`.
- `hasRealPlayer` is computed from the **leader's** account alone (`:214-217`, and the comment at `:286-287`: "Guild membership by real players does not affect this").
- So a bot-led guild that a still-Vanilla human has joined still receives new RandomBots from `InitGuild` (`PlayerbotFactory.cpp:5040-5070`).

### Effect with the IP Core #28 consumer
- **World-thread adds** are refused by `CanAddMember`, and `InitGuild` logs "failed to join guild".
- **Map-worker adds** (the level-up re-roll) are removed at the next world update.
- The guild stays isolated, but the randomizer keeps retrying and logging.

### Minimum change (Playerbots)
Treat a guild as unavailable for bot assignment when it has **any** member whose account isn't a RandomBot/AddClass account, not just a non-bot leader.

**Shape:**
- recompute `hasRealPlayer` in `OnGuildUpdate` and at load from `Guild::GetUnorderedMembers()` account ids (via `sCharacterCache`);
- or check it in `AssignToGuild` before returning a name.

**Constraints:**
- world thread only;
- no IP or stage knowledge needed. This is broader than F14 (bots stop joining any guild with real players), so it's **an owner/CS01 policy choice** for the Playerbots lane.

## Runtime evidence intake (combined disposable build)
Fill one row per case.

**Builds:**
- IP Core #28 consumer head, plus the social candidate tree;
- Core with #28 and, optionally, the guild-invite patch;
- PB #21 head;
- `MapUpdate.Threads` = 4 and = 0.

| Case id | Build SHAs (IP / Core / PB) | MapUpdate.Threads | Option | Characters (H stage, R verified?, others) | Action | Expected (from plan) | Observed (client + server log) | Pass/Fail | Notes / log excerpt |
|---|---|---|---|---|---|---|---|---|---|
| GP1 | | | ON | | | refused add | | | |
| GP2 | | | ON | | | bots leave next update | | | |
| GP3 | | | ON | | | bot leaves after H login | | | |
| GP4 | | | ON | | | per-recipient suppression | | | |
| GP5 | | | OFF / no #28 | | | stock | | | |
| GP6 (new) | | 4 | ON | R with `ResetBotLevelEnabled`, guild with online H | level R up to a reset level | R leaves at the next update after its re-roll | | | |
| PBD1–3, PBB1–4 | | | | | | see the runtime plan | | | |

**Evidence rules:**
- record exact SHAs and the `worldserver` log lines around each case;
- every "Fail" gets a minimal repro;
- nothing is run on the live realm or its databases.
