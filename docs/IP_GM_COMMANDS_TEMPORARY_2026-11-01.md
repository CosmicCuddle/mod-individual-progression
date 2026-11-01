# IP GM commands — temporary owner-requested developer coverage (DRAFT)

**Date:** 1 November 2026. **Source base:** paid-entitlement [DRAFT #23](https://github.com/CosmicCuddle/mod-individual-progression/pull/23) at `8dc16ffb3ca7f70a9c7ddc7cfbe6c5bd40088e37`. **Status:** code review only; NOT merged, enabled or installed.

CosmicCuddle reported Claude on break and asked Information to temporarily cover the active coding queue. This separate branch preserves Claude's claimed branch, #20, #23, all existing SQL and protected Main. The owner remains the final decision-maker.

## Rules implemented in this isolated draft

### 1. `.ip get` — available to every ordinary player, SELF only

- Only this command changes registration to `SEC_PLAYER`. All other `.ip` commands, including `set`, `tele`, and `setbot`, stay `SEC_GAMEMASTER`.
- A normal authenticated in-game player with no name argument always gets **their own** character, ignoring a manually selected other player.
- An ordinary player passing an explicit character name may view it only when that name resolves to **their own connected GUID**. Any other or unresolved named character receives the same refusal without revealing progression or online/offline state.
- GMs preserve explicit target and selection/name fallback. Console retains explicit named-online-character use; no session plus no target fails.
- The historical raw-stage output is unchanged. When the optional paid-expansion feature is ON for a normal account, an extra display distinguishes the **earned raw stage** and **current effective access stage**. It never reveals another account's purchase markers to a regular player.

The self-only authority is checked *inside the handler*, not just through command registration; setting `SEC_PLAYER` alone would be unsafe.

### 2. `.ip set` — GM recovery preserved, paid-era grant audited

- Existing staff security and force-stage behavior are preserved, both with paid mode OFF and ON. No mandatory `force` token is added.
- Only when paid mode ON applies to a normal-account target does the command snapshot TBC/Wrath ownership before/after the existing stage write.
- A newly opened era (e.g. unpaid stage7→8 TBC, or unpaid stage13→14 Wrath) triggers a clear GM warning that it was granted **without a purchase** and **without setting a paid entitlement marker**.
- One `module` log entry records only the GM account number or `console`, the target character GUID, old/new raw stage and old/new era entitlement booleans. It records no account secrets, passwords or loot.
- Repeating the same grant, an already paid character, a bot/excluded account, or the paid feature OFF do not produce new grant warnings. This audit line is not an immutable database-backed purchase ledger.

### 3. Preserved `.ip tele` behavior

The actual source constant is **`PROGRESSION_TBC_TIER_5 = 13`** (not 12). The historical GM Naxx/Onyxia teleport uses **raw stage `<13`**, so an unpaid raw13 character is **denied**. Effective stage12 would have passed, but this draft **does not change** GM teleport behavior or open any unpaid paid-era access. The earlier `<12` claim was incorrect and has been withdrawn in coordination #13/#11.

## Tests and native build requirements

The new pure `IndividualProgressionGmPolicy.h` is consumed by the handler and is tested by `tests/ip_gm_policy.cpp` (13 isolated checks: self/other/console/staff, repeated/new/off-mode/bot grants, historical teleport constants) using GCC and Clang in C++17/20. The new Python wiring suite checks that only `get` is public and the self/explicit-target logic is actually wired; `.ip set` auditing stays paid-only and writes no paid marker.

Existing strict parent CI tests explicitly repin the modified GM blob, **without removing any other protected IP source rule**, and the child workflow runs separate pure/source tests.

**All of this is source-level development only until GitHub CI finishes on the exact new SHA and a full pinned Worldserver compile/link succeeds.** No command has been exercised against live characters or an owner host. The source-only harness does not prove actual ChatHandler target parsing, account security tiers, console output, log persistence or addon compatibility. Those need a later owner-authorized staged account test with player, GM and console sessions.

## Review handover and rollback

CS01 Maintenance reviews code/CI and decides whether to integrate in a new combined candidate. Grok remains the dedicated **IP-only** independent reviewer on an exact new SHA after the draft is stable; Claude can resume a different unclaimed task upon return. Backout before merge is to discard this branch. After any future approved merge, a protected rollback would restore the previous command code, keeping player/GM data and all purchase markers untouched.

**No protected-main merge, actual paid-expansion flag enable, production GM command, SQL write, character reset, Worldserver restart, or source/fork change outside this draft is authorized.**
