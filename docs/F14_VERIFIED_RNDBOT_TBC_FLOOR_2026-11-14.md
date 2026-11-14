# F14 verified RandomBot TBC floor: one authoritative rule (14 Nov 2026)

## Status and scope

**Status:**
- an isolated DRAFT candidate on `feature/cs03-ip-f14-rndbot-tbc-floor-20261114`;
- the base is the synced IP `master` `05ff032b625f6e8abb6b2b5be496bb8a3ee65c96`;
- default **OFF**; source/native/contract/mutation evidence only.

**Request:** CS01, Issue #13 comment 6884383148 (live acceptance found verified Blood Elf/Draenei RandomBots at raw stage 0).

**Owner rule** (Information #10, 14 Nov 2026):
- verified Blood Elf/Draenei RandomBots have an absolute TBC minimum;
- they may sync upward (for example to Wrath) and later fall only to TBC, never to Vanilla;
- Altbots never have their stage changed by this system;
- race alone never establishes RandomBot identity.

## Why bots sit at stage 0 (synced sources)
| # | Path | Effect |
|---|---|---|
| A | Login of a bot or excluded account (`IndividualProgressionPlayer.cpp`, non-normal branch) | the stage is forced from level: `<= 60 → 0`, `61–70 → 8`, `71+ → 13`, unless an earlier F14 option applied |
| B | `ForceVerifiedRandomBotTbcFloorOnLogin` prototype | correct rule, but compiled only with `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION` and otherwise forced OFF at config load |
| C | `ProtectVerifiedRandomBotStageFloor` | blocks only downgrades of an already-earned 8+, so it can't lift 0 → 8 |
| D | Group sync | bots sync to the leader; the old group floor only protects an already-earned 8+ |
| E | Playerbots full re-roll: `PlayerbotFactory::Randomize(false)` → `ResetQuests()` | removes every rewarded quest, **including IP markers 66001–66018**, so the stage is 0 at runtime. Callers include the level-up reset in `RandomBotLevelMgr`, which runs on a map worker |
| F | GM `.ip set` / group set | explicit writes |

**Root cause:** for the level-60 population, A writes stage 0 at every login, and the only code that would lift it (B) is compiled out. The visibility gate predates Option B: concealment is paused, and isolation is now social. E re-zeroes a bot at runtime, outside IP.

## The rule
- **Switch:** `IndividualProgression.F14.VerifiedRandomBotTbcFloor` (default 0; forced OFF without the Playerbots API). It is **not** tied to the Core visibility-refresh macro.
- **Population:** `IsF14VerifiedRandomBotTbcFloorTarget(player)` requires:
  - IP enabled and the option ON;
  - the player in world;
  - a bot account, race Blood Elf/Draenei and a headless session;
  - **and** the login-thread verified RandomBot identity (`HasF14VerifiedRandomBotIdentity`);
  - and a usable `ProgressionLimit` (0, or 8–18).
  - Humans, Altbots, SelfBots, GMs and unknown characters are never written. Mutable Playerbots state is not read here.
- **Pure policy** (`IndividualProgressionF14TbcFloorPolicy.h`):
  - `TargetStage(saved, level, limit) = max(saved, min(limit if > 0, max(levelStage(level), 8)))`. A corrupt saved value (> 18) is unchanged.
  - `ClampWrite(requested) = max(requested, 8)`.
  - `LimitAllowsFloor(limit)`: 0, or 8–18.

### Where it acts
1. **Login:** for the population, it replaces write A with `TargetStage`. This repairs an existing stage-0 bot at its next login, with no DB sweep or SQL. A saved higher stage, including Wrath, is kept.
2. **Every IP write:** `ForceUpdateProgressionState` raises a request below 8 to 8 for the population, before the older downgrade block and before any marker removal. A Wrath bot can therefore fall back to TBC via group sync or `.ip set`, but never to Vanilla.
3. **After a Playerbots re-roll (E):**
   - `F14TbcFloorRepairWorldScript` runs every 5 s in the world update, after the map update.
   - It snapshots only already-verified GUIDs under the identity lock, without reading any player under that lock.
   - Outside the lock, it re-checks each online bot with `IsF14VerifiedRandomBotTbcFloorTarget` and repairs `raw < target` through `ForceUpdateProgressionState` only.

### ProgressionLimit
- `0` means no cap.
- `0 < limit < 8` means **no writes** (logged at config load). Accepted by CS01 (Issue #13 comment 6884895017): never force a TBC-race bot past an explicit Vanilla cap.
- A target above a positive limit is capped at the limit.
- A saved stage above a later-lowered limit is kept untouched.

### Legacy options
`PreserveVerifiedRandomBotStageOnLogin`, `ForceVerifiedRandomBotTbcFloorOnLogin`, `ProtectVerifiedRandomBotStageFloor` and `ProtectVerifiedRandomBotGroupFloor` are unchanged (still default 0).
- **CS01 decision (6884895017):** keep them present and default OFF for compatibility/history, but treat them as **legacy/deprecated** when the unified switch is used. Don't delete them yet. The new key's config block says so.
- **CS01 decision:** the old Core visibility-refresh compile dependency is dropped for this rule (it belonged to the paused concealment design).

## One-time repair versus ongoing enforcement
- **One-time:** happens at each bot's next login. Nothing touches offline characters or the database.
- **Ongoing:** the login rule, the write clamp, and the 5-second post-re-roll repair.
- **Window:** a re-rolled bot is at stage 0 for at most one repair interval.

## AutoGear (owner #59): progression value for Playerbots
- **Recommendation:** use `IndividualProgression::GetEffectiveProgression(Player*)` (public, `IndividualProgression.h`). **No new IP API is needed.**
  - It returns the raw stage, except that the paid-expansion feature (default OFF) caps an unpaid stage 13 at 12. That's the stage the character may actually use, which is what a gear cap needs.
- **Not recommended:** `GetPlayerProgressionFromQuests(Player*) const` returns the raw marker stage (it ignores the paid cap).
- **Both** return 0 for a player not in world, and read the player's own quest status.
- **Call it** at command time on the world thread, or on the bot's own map update, behind `__has_include` of the IP header, like the existing IP/Playerbots coupling. Don't call it from session or WHO workers.
- **No AutoGear code is added to IP.**

## Evidence
- **Native** `tests/f14_tbc_floor_policy_test.cpp`: exhaustive target/limit/level tables plus lifecycle scenarios:
  - 0→8 at login; saved 8–18 kept; 71+ → Wrath;
  - Wrath→TBC on a Vanilla-leader sync, never Vanilla;
  - re-roll → repair, idempotent;
  - Altbot/human/SelfBot/unknown negatives;
  - limit cases; OFF.
- **Contracts:** `tests/test_f14_tbc_floor_source.py`. Optionally they pin Playerbots `ResetQuests` and `Randomize(false)` when `F14_PLAYERBOTS_SRC` points at a Playerbots tree.
- **Mutations:** T1–T12 are all caught.
- **Older F14 exact-text contracts:** `test_f14_social_isolation_source`, `test_ip_f14_stage_floor_source` and `test_ip_f14_verified_rndbot_tbc_floor` were updated minimally to allow this rule's lines. Each keeps its original guarantee.
- **Not done:** gameplay testing.
