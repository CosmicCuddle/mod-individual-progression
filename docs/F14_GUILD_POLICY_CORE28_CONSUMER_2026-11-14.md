# F14 guild policy: Core #28 consumer (14 Nov 2026)

## Status and scope

**Status:**
- an isolated DRAFT candidate on `feature/cs03-ip-f14-guild-core28-consumer-20261114`;
- the parent is the accepted social tree (`6634881458e7` = tree of `5918174b`);
- source/native/contract/mutation evidence only; not built into a server, not played, nothing live.

**Seams:** Core DRAFT #28 `137d5a27690bf5bae6e95b32e7f2df449b1a0863` (`COSMIC_F14_GUILD_POLICY_HOOK_VERSION 1`), providing:
- `GuildScript::CanAddMember(Guild const*, ObjectGuid)`, called in `Guild::AddMember` before any petition, membership, DB or broadcast mutation;
- `GuildScript::CanSendChatToMember(Guild const*, Player* sender, Player* receiver, bool officerOnly, uint32 language, std::string_view msg)`, called per online recipient in `Guild::BroadcastToGuild`.

**Unpatched Core:** every guild-policy piece is inside `#if defined(COSMIC_F14_GUILD_POLICY_HOOK_VERSION) && COSMIC_F14_GUILD_POLICY_HOOK_VERSION >= 1`, so on Core25 the behaviour is stock (verified by syntax checks against both trees).

**Owner policy:**
- **Option B:** a still-Vanilla human (real client) and a positively verified Blood Elf/Draenei RandomBot share neither guild membership nor guild chat. Humans, Altbots, SelfBots and unknown identities are never bot-removal targets, and race alone is never used.
- **New-join instruction:** the bots leave; a bot Guild Master leaves too, by stock succession.
- **Login remediation:** "Yes remove them on login".
- **Era regression:** no action at the stage change; the next login handles it.

## Behaviour (option ON; default OFF)

| Event | Seam | Result |
|---|---|---|
| A restricted RandomBot is added (client, GM, Playerbots `InitGuild`, offline GUID) to a guild with an **online** still-Vanilla member | `CanAddMember`, world thread | **refused before persistence**; no DB write, no broadcast, no removal needed |
| The same, off the world thread | `CanAddMember` | **allowed (fail open)**: no foreign Player/Guild traversal off the world thread |
| A human, Altbot, ordinary bot or unknown GUID is added | `CanAddMember` | allowed (only a positively restricted bot is ever refused) |
| A still-Vanilla human **joins** a guild containing restricted bots | `OnEvent(GUILD_EVENT_LOG_JOIN_GUILD)`, world thread | the join is allowed; the restricted bots are removed at the next world update (owner: "the Blood elf or Draenei left") |
| A still-Vanilla human **logs in** to a guild containing restricted bots | `OnPlayerLogin`, world thread | the same removal at the next world update (owner decision) |
| A human drops back to Vanilla | none | nothing until that human's next login (owner decision) |
| Guild or officer chat between a still-Vanilla human and a restricted bot, either direction | `CanSendChatToMember` | that delivery only is suppressed; everyone else receives it; no reply |
| `LANG_ADDON` guild traffic | `CanSendChatToMember` | always delivered |
| Chat off the world thread (Playerbots' direct `BroadcastToGuild` from bot AI) | `CanSendChatToMember` | decided only if both players are proven in the owned map, otherwise delivered (T1 fail open) |

## Removal (remediation only)
- **Work set:** `PendingGuildRemediation`, keyed `(guild id, human)`. It coalesces, **never drops**, and every world update empties it. It's started only by a still-Vanilla real-client human's join or login on the world thread.
- **At the world update (option still ON):**
  1. the guild is looked up again, and the human must still be a member (otherwise nothing happens);
  2. restricted members are online bots by live facts, or offline bots confirmed at their last logout. Unknown members are kept.
- **Immediately before each removal:** the guild exists, the target is still a member, **the member count is ≥ 2**, and the human is still a member. Then `Guild::DeleteMember(target, false, true, false)`: stock kick flags, `canDeleteGuild` false. With a surviving member, Core's sole-member `Disband()` branch can't be reached.
- **No calendar cleanup:** stock kick doesn't do it.
- There is no admission-time `DeleteMember` and no bulk sweep. A human is never removed.

## Remaining dependencies (not claimed)
- **Playerbots P-2:** bot guild chat calls `Guild::BroadcastToGuild` from bot AI on a map thread. The recipient hook then usually fails open. Routing it through the world thread is the PB fix.
- **Playerbots P-4:** bot-guild assignment filter. With admission refused, Playerbots' `InitGuild` just logs a failed add.
- **Admission off the world thread:** fails open. Whether any real caller adds off the world thread is a runtime inventory item.
- **Human join and login remediation:** removal runs at the next world update, so the roster is shared for that interval. Chat is already filtered per recipient.
- **Native build and gameplay:** UNKNOWN.

## Evidence
See the Issue #13 handback for exact counts:
- **Native:** `TestGuildPolicyCore28` — truth tables, exhaustive 7-kind guilds, a bot leader, multiple bots, the human leaving before the update, OFF before the update, eligible/Altbot/SelfBot/unknown, disband protection, era regression, chat in both directions, addon, mixed recipients, and the non-dropping work set;
- **Contracts:** `test_guild_policy_core28_consumer`;
- **Mutations:** C1–C16;
- **Syntax:** against Core #28, unpatched Core25 and the guild-invite patch.
