# Paid expansion entitlement: earned final milestone, then purchase (IP-11)

**Owner:** CosmicCuddle
**Created:** 1 November 2026
**Status:** DRAFT only. Unmerged, not installed, not game-tested. **OFF by default** (`IndividualProgression.PaidExpansion.Enable = 0`).

| Item | Value |
| --- | --- |
| Branch | `feature/ip-paid-expansion-entitlement-20261101` |
| Parent | [DRAFT #20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20) at `5fe0f9105d55d249aa55a25510d3fa23e3d84148` (Hunter, grouping, C3, partial F14, GM safety) |
| Plan | [DRAFT #22](https://github.com/CosmicCuddle/mod-individual-progression/pull/22) `docs/DEVELOPMENT_TASKLIST.md` §3 (IP-11A–L) |
| Gameplay record | [Coordination #18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18) |
| Assignment and claim | [Coordination #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13) |

## Owner rule

Beating the final raid of an era makes a character **eligible** to buy the next expansion from Chronomancer Vezrath. Only the purchase opens the next era. Gold alone, the boss alone, or another character's progress is never enough. A purchase opens the **start** of the next era and never marks one of its raids as completed.

## How the existing stages are used (no renumbering)

| Stage | Meaning in this fork | With the feature ON |
| --- | --- | --- |
| 7 | Kel'Thuzad 40 (final Vanilla milestone) | Eligible to buy TBC; still Vanilla, level cap 60 |
| 8 | TBC entered (before this change, only the Into the Breach quest granted it) | Granted by the TBC purchase. Characters already at 8+ keep TBC |
| 13 | Kil'jaeden (final TBC milestone). Legacy code also uses 13 as "Wrath open" in about 40 checks | **Unpaid:** treated as 12 for every access check (XP cap 70, no Northrend, TBC phasing, TBC arena season, TBC group band). **Paid:** unchanged 13 |
| 14 | First Wrath raid tier completed | Never granted by a purchase. Characters already at 14+ keep Wrath |

TBC already had a separate entry step, so stage 8 itself records "TBC entered". Wrath had none, so Wrath ownership is a separate stored flag.

## What changes when the feature is ON

1. **Access checks.** `hasPassedProgression` and `isBeforeProgression` use `GetEffectiveProgression`. That is the rewarded stage, except that an unpaid stage 13 counts as 12. This one change covers every existing XP, travel, instance, phasing, spell, arena and Death Knight check.
2. **Automatic progression.** `UpdateProgressionState` refuses a grant whose target lies in an era the character does not own. Examples: Into the Breach at stage 7 unpaid, or a Wrath achievement while Wrath is unpaid. The grant is refused whole, so a cross-era target can never award the current era's final milestone as a side effect. Earning stages inside the owned era, including Kil'jaeden to 13, works as before.
3. **Grouping (EnforceGroupRules = 1).** Invite, accept and bot-sync era comparisons use the effective stage, so an unpaid 13 groups as TBC. Stage **writes** (OFF accept sync, bot sync) keep raw stage values.
4. **Login reminder.** An eligible, unpaid character gets a yellow message: "You have completed the requirements to unlock The Burning Crusade! Visit Chronomancer Vezrath to purchase access." The Wrath message is the same with Wrath's name. It stops once the expansion is owned. The vendor name is configurable.

**Not gated (established behaviour kept):**
- bot and excluded accounts (`isNormalAccount` false);
- characters whose configured start stage (`DeathKnightStartingProgression`, `tbcRacesStartingProgression`, `StartingProgression`) already reaches the era;
- GM `.ip` commands and `ForceUpdateProgressionState`, which remain privileged overrides.

GM characters on normal accounts are gated like players.

With the feature OFF, `GetEffectiveProgression` returns the rewarded stage. The new grant check returns true before reading anything, so behaviour is identical to the parent.

## Storage

Purchases use the Core character settings store (`Player::UpdatePlayerSetting`, table `character_settings`, source `ip_paid_expansion`).

| Slot | Meaning | Values |
| --- | --- | --- |
| 0 | TBC purchased | 0 = no, 1 = paid |
| 1 | Wrath purchased | 0 = no, 1 = paid, 2 = grandfathered |
| 2 | Grandfather evaluation done | 0 / 1 (only written when the grandfather option is on) |

- **No new table and no SQL.** Core already ships `character_settings`.
- **Saved together:** Core saves settings, quest status and money in the **same `SaveToDB` transaction**, so gold and entitlement persist or roll back together.
- **Requires worldserver `EnablePlayerSettings = 1`, whose code default is 0.** The live value is unknown. If it is 0, the feature logs an error at startup and forces itself OFF, so nobody is locked out and no purchase is possible.
- The switch is **read at server start only**. `.reload` keeps the startup value.

**Thread safety.** Access checks run very often, including on map threads. They read only a small IP cache guarded by a shared mutex. The cache is filled on the login thread in `OnPlayerSendInitialPacketsBeforeAddToMap`, after the character's settings have loaded and before it enters a map, and cleared on logout. Core's `GetPlayerSetting` creates a missing slot as a side effect, so it is only called on the login thread or during a purchase.

## API for Character Services (vendor integration)

All functions live on `sIndividualProgression`. Rules are in `src/IndividualProgressionExpansionPolicy.h`.

`IndividualProgression.h` defines `COSMIC_IP_PAID_EXPANSION_API_VERSION 1`, so a dependent module can use `#if defined(COSMIC_IP_PAID_EXPANSION_API_VERSION)` and still compile against an IP build without this API.

| Function | Use |
| --- | --- |
| `IsPaidExpansionActive()` | Show the expansion offers only when this is true |
| `HasExpansionEntitlement(player, Era::Tbc / Era::Wrath)` | Already owned? |
| `CanPurchaseExpansion(player, era, costCopper)` | Pre-check for the gossip menu. It never charges |
| `PurchaseExpansion(player, era, costCopper)` | The only purchase path |
| `GetMaxServiceGrantableStage(player)` | Cap for any catch-up/tier service: 6 before TBC is owned, 12 before Wrath is owned |

**`PurchaseExpansion` steps:**
1. Revalidate everything.
2. Check `HasEnoughMoney`.
3. Charge.
4. Store the flag.
5. For TBC, grant stage 8.
6. Verify. On failure, clear the flag **and refund**.
7. `SaveToDB`.

**Results:**
- `Success`.
- `AlreadyOwned`: repeat or double click; no charge.
- `NotEligible`: boss not killed, or a forged or out-of-order era; no charge.
- `InvalidRequest`: unknown era value or cost above the int32 range.
- `InsufficientFunds`.
- `BlockedByProgressionLimit`.
- `EntryUnavailable`: stage-8 quest template missing; no charge, or a refund if detected after charging.
- `FeatureInactive`.
- `NotApplicable`: bot or excluded account.

**Character Services must:**
- call `PurchaseExpansion` instead of `ForceUpdateProgressionState(8 / 14)`, the snapshot's actions 200/201;
- never charge before validation;
- cap generic per-tier catch-up with `GetMaxServiceGrantableStage`;
- recheck at click time.

That integration belongs to the Character Services fork and is not part of this branch.

## Tests

- `tests/expansion_purchase_policy.cpp` covers the real policy header, both eras, under GCC and Clang, C++17 and C++20 (269 checks). Cases:
  - feature OFF identity for every stage;
  - bots and excluded accounts unchanged;
  - boss-only leaves the character unpaid;
  - wealthy without the boss is denied;
  - paid TBC means stage 8 and no raid tier;
  - paid Wrath means no stage write and no stage 14;
  - repeat purchase is never charged;
  - unpaid 13 means effective 12, so no Northrend or 70+ XP;
  - 14+ is never revoked;
  - forged era values are rejected;
  - account-alt and catch-up caps hold;
  - DK and TBC-race start privileges are kept;
  - the progression limit blocks;
  - grandfathering applies once only.

  The section marked **M** is a labelled model of the purchase sequence, not AzerothCore code.
- `tests/test_ip_paid_expansion_source.py` (27 checks) covers wiring:
  - default OFF;
  - forced OFF without the settings store;
  - startup-only switch;
  - effective stage used by `hasPassedProgression`/`isBeforeProgression`;
  - raw reader and `ForceUpdateProgressionState` unchanged;
  - grant refusal placed before any quest write;
  - group ON checks effective, OFF writes raw;
  - purchase order validate → charge → flag → grant → verify → refund → save;
  - no stage-14 write;
  - hot path never touches the settings store;
  - hooks and registration;
  - protected files unchanged;
  - no SQL.
- The workflow `ip-paid-expansion-entitlement-20261101.yml` runs these plus every inherited #16, #19 and #20 check. It also runs a pinned full Worldserver configure, compile and link (Core `8daffdab`, Playerbots `e88cc409` with the two runner-only compatibility edits, Naxx `80e9de1c`, `-DCOSMIC_IP_F14_REQUIRE_PLAYERBOTS`) and asserts that all four IP translation units are in the compile graph. It **never launches** the server.

## Declared CI deviations on this branch

1. `.github/workflows/ip19-gm-null-safety-integration-20261101.yml` (#20): its "six paths" step was unconditional and would fail on any child of #20. One `if:` now scopes that step to #20's own head. Its GM tests still run here.
2. `tests/test_ip_combined_c3_f14_partial_login.py` (#19): the C3-identical, two-hunk and quest-read assertions now first reverse **only** the documented group substitutions. Every other difference still fails.

If the owner later folds this branch into #20 or #19, their frozen assertions must be rebaselined under review, not bypassed.

## Not decided here (feature stays OFF)

- The exact final requirements for each era (Information #18). Current source uses Kel'Thuzad 40 and Kil'jaeden.
- **Grandfathering of characters already at stage 13.** `GrandfatherStage13 = 0` by default: they must buy Wrath. When set to 1, a character already at 13 on its first login with the feature on is recorded as owning Wrath.
- Whether GMs, Altbots or other identities are exempt.
- Whether within-era catch-up remains.
- Live prices (the snapshot defaults of 1,000 / 3,000 gold are unverified).
- `/reload` behaviour. The reminder is login-only; `/reload` reloads the client UI only, and no addon hook exists here.
- Bots stay ungated. Playerbots `ResetQuests` (C1) does not touch `character_settings` but can still remove a bot's stage markers.
- If a human's stage-8 marker is later removed (GM `.ip set` lower, or a Playerbots `initself`), the TBC flag remains but access follows the marker. Nothing is re-granted automatically.
- Live `EnablePlayerSettings`, the database engine and effective settings remain **UNKNOWN**.

## Rollback

- Leave or set `IndividualProgression.PaidExpansion.Enable = 0` and restart. All checks return to the parent behaviour.
- Stored flags stay in `character_settings` and are ignored while the feature is OFF.
- To remove the code, revert this branch; parents #20, #19 and #13 are untouched.
- A rollback never refunds gold automatically. Any refund or entitlement cleanup on a live realm needs a reviewed, backed-up procedure and the owner's approval.
