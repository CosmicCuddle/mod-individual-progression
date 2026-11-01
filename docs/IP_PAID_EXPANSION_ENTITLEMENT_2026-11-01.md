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
- `Enable` and `GrandfatherStage13` are **read at server start only**. `.reload` keeps the startup values.
- Access checks use that startup state. If `EnablePlayerSettings` is switched to 0 mid-session, **even followed by `.reload config`**, the gate stays on, so access is **not** opened for free and no unpaid stage 13 can earn stage 14. Instead purchases and grandfathering are refused (`FeatureInactive`) and an error is logged, because gold would be saved but the purchase flag would not. A paid character who relogs while the store is off reads as unpaid until it is restored.
- Purchase record: one `LOG_INFO` line on the `module` channel (Server.log by default) per purchase or refund. `character_settings` is not part of character dumps, so a transferred character keeps TBC (stage 8 marker) but not a paid Wrath flag.

**Thread safety.** Access checks run very often, including on map threads. They read only a small IP cache guarded by a shared mutex. The cache is filled on the login thread in `OnPlayerSendInitialPacketsBeforeAddToMap`, after the character's settings have loaded and before it enters a map (also on reconnect and worldport). It holds purchase bits only and is replaced on every load; it is deliberately not cleared on logout, which would briefly show a paid, still in-world character as unpaid. Core's `GetPlayerSetting` creates a missing slot as a side effect, so it is only called on the login thread or during a purchase.

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
| `GetAccountEffectiveProgression(accountId)` | Account maximum for unlock checks (e.g. a vendor's account-unlock ceiling): equal to `GetAccountProgression` when OFF; an unpaid stage 13 counts as 12 when ON |

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

- `tests/expansion_purchase_policy.cpp` covers the real policy header, both eras, under GCC and Clang, C++17 and C++20 (341 checks). Cases:
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
- `tests/test_ip_paid_expansion_source.py` (31 checks) covers wiring:
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

## Operations runbook (when the owner approves enabling)

1. Before the first start with `PaidExpansion.Enable = 1`: set worldserver `EnablePlayerSettings = 1`, decide `GrandfatherStage13`, and back up the character database.
2. Read the startup log. **Do not lower a historical starting tier just to silence a warning.** Each character origin is checked independently for **both** eras: a starting stage ≥ 8 warns about free TBC when its creation unlock is 0–7, and a starting stage ≥ 13 warns about free Wrath when its creation unlock is 0–12. Thus the default DK pair **13/13** does not warn, nor does a fully gated **8/8** TBC-race pair; but DK **8/7** warns about free TBC, and race **13/12** warns about free Wrath. Where both thresholds are bypassed, two warnings are expected. General `StartingProgression` ≥ 8 and independent `TbcRacesUnlockProgression` values 1–7 still warn. All messages are informational; no account creation, unlock, starting progression, paid flag or purchased era is changed.
3. Never set `EnablePlayerSettings = 0` while the feature is on. If it happens and is reloaded, an error is logged: the gate stays on, purchases and grandfathering are refused, and paid characters who log in meanwhile read as unpaid. **Stop qualifying progression activity during the outage:** cross-era automatic stage grants may be silently refused, and completing that quest or boss is **not automatically replayed** after storage recovers. Restore the setting and restart; stored purchase flags return on next login, **but any refused stage grant remains missing**. Existing code does **not** log each refused character GUID, so do not claim it supplies a complete affected-player list. Identify individual affected characters from approved backups/game records and verify their actual stage and eligibility before any owner-approved, backed-up GM repair; no blanket `.ip set` and no automatic paid entitlements.
4. Purchases and refunds appear as `module` log lines naming the character GUID, era and copper.
5. Rollback: set `Enable = 0` and restart to restore **historical raw-stage access**. This immediately lets unpaid stage-13 characters access Wrath again (expected old behavior), so do not mistake this for proof they purchased it. Characters whose Into the Breach or other qualifying grant was refused can remain at their lower stage even after rollback; a GM may restore an individually verified, legitimately earned stage only after CosmicCuddle approves the repair with a character-DB backup and rollback plan. `.ip set <name> 8` is a privileged override, **not** an automatic purchase or safe bulk recovery.

## Integration-stage owner safeguards — character transfers, F19 and grandfather timing (1 Nov 2026)

**READ BEFORE any owner-approved staging or live paid-feature enablement.** This combined development candidate includes the F19 login-floor code from [standalone draft #24](https://github.com/CosmicCuddle/mod-individual-progression/pull/24), but that code is default-OFF because it only applies when `F14.PreserveVerifiedRandomBotStageOnLogin = 1`. Do not enable F14 or paid-mode just because source was copied; the combined exact-head CI, linked Worldserver, Playerbots and game scenarios must still pass.

- **Character transfers / `.pdump`: high-impact known limitation.** The historical pinned Core's `PlayerDump.cpp` includes rewarded quest rows but not the per-character `character_settings` row that stores `ip_paid_expansion` paid/grandfather flags. A normal dump/load may transfer the raw stage but **not** the entitlement bits. With paid mode ON, a legitimate Wrath buyer at raw stage 13 could lose access, or `GrandfatherStage13 = 1` could mistakenly classify an already-evaluated unpaid character as grandfathered after the missing row is re-created. Until an owner-approved, tested transfer procedure exists, **do not perform `.pdump` / character-copy/transfer for affected paid or stage-13 characters**. A future repair needs authorized character-DB backups, source GUID/target GUID matching, verified original slot values and per-user reconciliation **before first target login**; never assume 66013 alone proves purchase, never auto-grant a new flag, and never publish player GUID/DB contents here. This source draft adds no SQL or Core PlayerDump fix.
- **Grandfather evaluation timing:** source hook registration evaluates grandfather eligibility using the stored **raw stage before the IP login catch-up write**. That avoids accidentally granting fresh DK/race characters grandfather entitlement, but an existing character whose qualifying stage rises from 12 to 13 during the same first login may not be grandfathered. This is a recorded first-login policy detail, **not** permission to reorder player hooks. CosmicCuddle must decide the grandfather option before first activation.
- **Account creation fail-closed window:** after a first-login grandfather grant, the in-memory flag may not be saved to `character_settings` until the normal player save; an account SQL check can temporarily refuse a DK/race or Outland start while the row is unsaved. This is a denial/lag, not proof of entitlement bypass. No separate online update to the settings row is authorised.
- **F19 F14 login floor:** when F14 is explicitly ON, preserve a verified, headless Blood Elf/Draenei RandomBot's saved stage only if its saved stage is **at least** the level-derived stage (0 through level 60, 8 for 61–70, 13 for 71+) and **does not exceed** a configured nonzero progression limit. Otherwise run the unchanged original level-based stage write. **F14 stays OFF until combined Worldserver/native and actual bot acceptance pass.** This is separate from normal-account purchase gating, which is not applied to bot accounts.

**Known historical fallback (separate owner gameplay choice; no runtime change in this draft):** In the non-normal account login path, if F14 does not preserve a saved stage, the existing `ForceUpdateProgressionState(LevelStage(level))` write **does not check** `progressionLimit`. A level-71+ bot or an `ExcludedAccountsRegex` human account can therefore be set to stage 13 even when its configured limit is 8–12. The same fallback exists with F14 OFF and in the pre-F19 source, so F19 introduces no new cap bypass; it reduces over-limit stage preservation. It can also lower a saved stage above the level-derived floor. Do **not** quietly clamp stages or change excluded human behaviour as part of enabling the unrelated paid purchase feature. CosmicCuddle must decide whether to preserve this historical rule, modify excluded human treatment, accept F19-declined lowering, or authorise a **separate default-OFF** safeguard with explicit staging tests. See [Grok's 1,000-case disposable model / source audit](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6565387289). The model is not an executed Worldserver or gameplay test.

**Proof boundary:** Source inspection and local regression evidence from the separate drafts are not linked/gameplay/installed proof of this combined candidate. [Grok's combined audit in Issue #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6564084464) records the underlying findings. No production SQL, paid flag, transfer, setting, deploy or actual server operation has been performed.

## Independent review of `62e6828` (Coordination #11) and follow-up

Changes requested by the reviewer and made on this branch:
- grandfather option pinned at startup and evaluated once per character regardless of its value (no retroactive grants);
- no fail-open if `EnablePlayerSettings` changes mid-session, including after `.reload config` (second review, N1); purchases and grandfathering refuse instead;
- cache no longer cleared on logout (removes the logout window);
- purchase log location and character-dump limitation documented.

Open, needing an owner/CS01 decision before `Enable = 1` (not changed here):
1. **Database conditions read the raw marker.** About 360 `conditions` rows of type QUESTREWARDED on quest 66013 (mostly vendor stock, some phasing gossip) are evaluated by Core directly, so an unpaid stage-13 character still passes them. Options: accept as a known limit; hold back 66013 until purchase (changes the Kil'jaeden meaning, needs a design pass); or a separate "Wrath paid" marker with retargeted conditions (an SQL migration).
2. **Start privileges grant an era without payment.** A configured DK start of 13 (code default) or TBC-race start of 8 counts as owning that era. **Partly closed:** with the feature ON, DK / TBC-race creation and the low-stage Outland start-zone check now use `GetAccountEffectiveProgression`, where an unpaid stage 13 counts as 12. An unpaid 13 can no longer unlock a DK who would own Wrath; a paid or 14+ character on the account still can. Existing characters keep their start privileges. Still for the owner: whether a DK or TBC-race start should own the era at all. Also, with `TbcRacesUnlockProgression` ≤ 7, an unpaid Kel'Thuzad kill still unlocks a Blood Elf/Draenei who starts at 8; setting it to ≥ 8 needs a TBC purchase on the account.
3. **GM `.ip set`** uses `ForceUpdateProgressionState`: setting 8+ or 14+ grants TBC or Wrath with no flag, and there is no GM command to show, grant or revoke a purchase. `cs_individualProgression.cpp` is a reviewed, protected file, so a GM command would be a separate draft.
4. **`.ip tele` (GM) reads the raw stage** for its Naxx40/Onyxia check, while the instance gate uses the effective stage. Same protected-file constraint.
5. **Rollback after refusals.** A character who completed Into the Breach while TBC was unpaid stays at stage 7 if the feature is later turned OFF; they need a GM `.ip set 8`. No automatic repair is added, because it could override a GM's deliberate setting or `DisableDefaultProgression`.

## Not decided here (feature stays OFF)

- The exact final requirements for each era (Information #18). Current source uses Kel'Thuzad 40 and Kil'jaeden.
- **Grandfathering of characters already at stage 13.** `GrandfatherStage13 = 0` by default: they must buy Wrath. Every gated character is evaluated once, on its first login with the feature on, **whatever the option is then**. With 1, a character already at 13 at that login is recorded as owning Wrath. Decide before first enabling the feature: turning it on later grants nothing retroactively.
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
