# F14 social isolation — world-thread dispatch for bot-started interactions (DESIGN ONLY)

**Owner:** CosmicCuddle. **Developer:** CS03-IP. **Requested by:** CS01 verdict [6865093100](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6865093100) (group invite: CHANGES REQUIRED).
**Status:** a proposal for CS01 review. **No Playerbots or Core file has been edited.** Nothing here is deployed.
**Pins used:**

| Component | Pin |
|---|---|
| Playerbots | `CosmicCuddle/mod-playerbots@405f81196161017eebae3ee7a612d786a3ed2a9d` |
| Core: CS01's candidate | `240f6250` |
| Core: my local anchors | Core25 `395b68ee` |

The installed tuple is **UNKNOWN**.

## The problem
- **The rule being broken:** Core marks `CMSG_GROUP_INVITE`, `CMSG_GROUP_ACCEPT`, `CMSG_INITIATE_TRADE` and `CMSG_BATTLEFIELD_PORT` as `PROCESS_THREADUNSAFE`. A real client's packet for them is handled only on the world thread, while no map is updating.
- **What Playerbots does instead:** it calls those handlers **directly from bot AI**, which runs inside a map update:

  | File:line | Direct call |
  |---|---|
  | `InviteToGroupAction.cpp:50` | `inviter->GetSession()->HandleGroupInviteOpcode(p)` |
  | `AcceptInvitationAction.cpp:40` | `bot->GetSession()->HandleGroupAcceptOpcode(p)` |
  | `TradeAction.cpp:44` | `bot->GetSession()->HandleInitiateTradeOpcode(packet)` |
  | `AcceptBattlegroundInvitationAction.cpp:21` | `bot->GetSession()->HandleBattleFieldPortOpcode(packet)` |

- **Why that's a hazard:** with map worker threads active, Core's own handler code then runs on a map thread. For example, the group-invite handler calls `ObjectAccessor::FindPlayerByName` at `GroupHandler.cpp:80`, which reads the global name map, **before** any script hook. **No module hook can make that earlier Core read safe.**
- **Scope:** the same pattern exists for **35 distinct thread-unsafe handlers** in this Playerbots version (trade item/gold, loot, mail, guild accept/decline, duel accept, arena team, and more). Only the four above touch social isolation. The rest are a pre-existing Playerbots concern and out of scope here.

## What IP already does (my lane, done)
- **Group-invite name lookup:** IP no longer does its own global name lookup off the world thread. Off the world thread the invitee is resolved only among players on the map the current thread owns (`ResolveByName`). On the world thread, `FindPlayerByName` is safe and kept.
- **No reads before ownership:** every social hook reads another player only after the T1 ownership proof (world thread, or both on the owned map).
- **The limit:** **this does not fix Core's own earlier read on the bot route.** That needs the Playerbots change below.

## Proposed Playerbots change (smallest safe seam, using existing mechanisms)
Playerbots already has two world-thread mechanisms in this exact version:
- `WorldSession::QueuePacket`, used by `LfgActions.cpp` with the comment "JoinLfg is not threadsafe, so make packet and queue into session", and also by `GuildManagementActions.cpp` and `LootAction.cpp`. Who drains which queue (all on the world thread):

  | Queue | Drained by | Evidence |
  |---|---|---|
  | RandomBot packets | `sRandomPlayerbotMgr.UpdateSessions()` | "Per-bot packet queues, world thread only", `Playerbots.cpp:548` |
  | A master's own bots' packets | `PlayerbotHolder::HandleBotPackets`, reached from `SessionScript::OnSessionUpdate` (`Playerbots.cpp:451–455`) | Core fires that hook only inside `if (updater.ProcessUnsafe())` (Core25 `WorldSession.cpp:597–599`): the world-thread pass, never the map pass |
  | A real client's packets | `World::UpdateSessions` | — |

  `QueuePacket` itself is a `LockedQueue::add` (`WorldSession.cpp:361–364`), safe from any thread.
- `PlayerbotWorldThreadProcessor` with `PlayerbotOperation`, run from `WorldScript::OnUpdate` (`Playerbots.cpp:549`) on the world thread, after the map phase.

| # | File | Today | Proposed | Why that mechanism |
|---|---|---|---|---|
| 1 | `src/Ai/Base/Actions/InviteToGroupAction.cpp:50` | direct `HandleGroupInviteOpcode` | `inviter->GetSession()->QueuePacket(new WorldPacket(CMSG_GROUP_INVITE …))` with the same payload (name and roles) | the result is not used afterwards (`Invite()` returns `true` right away) |
| 2 | `src/Ai/Base/Actions/AcceptInvitationAction.cpp:40` | direct `HandleGroupAcceptOpcode`, then **uses the result immediately** (`bot->GetGroup()`, `SetMaster`, strategies, `TellMaster`, summon) | a new `GroupAcceptOperation(botGuid, inviterGuid)` in `PlayerbotOperations.h`. Its `Execute()` calls `HandleGroupAcceptOpcode` and then runs the existing follow-up unchanged, all on the world thread | plain queueing would lose the follow-up (a behaviour regression). The world-thread operation keeps it synchronous |
| 3 | `src/Ai/Base/Actions/TradeAction.cpp:44` | direct `HandleInitiateTradeOpcode` | `QueuePacket(CMSG_INITIATE_TRADE …)` | returns `true` immediately; no dependency |
| 4 | `src/Ai/Base/Actions/AcceptBattlegroundInvitationAction.cpp:21` | direct `HandleBattleFieldPortOpcode` | `QueuePacket(CMSG_BATTLEFIELD_PORT …)` | only `ResetStrategies()` follows, and it doesn't depend on the result |

**Effect:**
- All four reach Core's handler on the world thread, exactly like a client. IP's hooks then see the normal client path, where the existing world-thread branches apply.
- Bot actions are delayed by at most one world tick.
- `QueuePacket` is thread-safe (Core's `LockedQueue`).

**Also seen, not part of this seam:** `PlayerbotMgr.cpp:610` queues `GroupInviteOperation`, which calls `Group::AddMember` directly on the world thread (no invite or accept hook). It's used when a master's bots log in or join the master's party.
- It's thread-safe, but it **bypasses social isolation**.
- Its usual targets are the master's own bots, which isn't a restricted RandomBot pairing.
- Whether a master can pull a restricted RandomBot through it, and what to do if so, is an **open question** for CS01 or the owner.

## Ordinary human behaviour (traced separately)
- **Unchanged:** a human's invite, accept, trade and port come from the client as packets processed by `World::UpdateSessions` on the world thread. Nothing in this proposal touches that path.
- **One edge:** `LfgAction`/`JoinGroupAction` call `Invite(human, bot)` from **bot** AI with a human as the inviter. Item 1 queues that packet into the human's session, which is exactly how a client invite would arrive.

## Map worker threads disabled (separate case)
- **When it applies:** if the server runs with no map worker threads (`MapUpdate.Threads` set so the MapUpdater is not activated), all maps update on the world thread. The direct calls then run on the world thread and are not a thread hazard.
- **Why it doesn't waive the fix:** the installed setting is **UNKNOWN**, and the change is harmless in both configurations.

## Requirements, rollback and tests
- **Dependencies:**
  - the Playerbots edits only (4 call sites, plus one new operation class in `src/Script/WorldThr/PlayerbotOperations.h`);
  - no Core change;
  - no IP change beyond what this branch already has;
  - no DB/SQL/config/DBC/client change.
- **Rollback:** revert the Playerbots commit. IP is unaffected either way, and the social option stays OFF by default.
- **Tests to add with the implementation:**
  1. A source contract over the Playerbots tree: none of the four files calls `Handle*Opcode` directly; items 1, 3 and 4 use `QueuePacket` with the same opcode and payload; item 2 queues `GroupAcceptOperation`, whose `Execute()` contains the unchanged follow-up.
  2. A native check that the `GroupAcceptOperation` follow-up runs only after `bot->GetGroup()` contains the inviter.
  3. Runtime, on an approved test realm with map worker threads > 0:
     - bots invite and accept humans, trade with them, and accept battleground invites as before (at most one tick later);
     - a restricted RandomBot and a Vanilla human stay blocked;
     - no crash or race under load (ideally a ThreadSanitizer build).
- **Ownership:** the implementation belongs to whichever lane owns Playerbots, after CS01 reviews this seam. I'm **not** editing Playerbots from this lane.
