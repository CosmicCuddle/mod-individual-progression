# GM commands: paid-mode `.ip set` audit and player self-only `.ip get`

**Owner:** CosmicCuddle
**Created:** 1 November 2026
**Status:** DRAFT only. Unmerged, not installed, not game-tested.

| Item | Value |
| --- | --- |
| Branch | `feature/ip-gm-commands-paid-audit-get-self-20261101` |
| Parent | [DRAFT #23](https://github.com/CosmicCuddle/mod-individual-progression/pull/23) (paid expansion) at `8dc16ffb3ca7f70a9c7ddc7cfbe6c5bd40088e37` |
| Requests | Maintenance [#6560471210](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6560471210) and [#6560554885](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6560554885); owner rule recorded by Information in [#6560476663](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6560476663) |
| Claim | [Coordination #13 #6560714182](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6560714182) |

## `.ip get`: any player, own character only

The owner previously decided that any player may use `.ip get`, but only to read their own progression. The command was registered `SEC_GAMEMASTER`. Its optional name argument and selection fallback would let a non-GM read anyone, so changing the security level alone was not safe.

- `get` is now registered `SEC_PLAYER`. This is the **only** permission change; `set`, `pet`, `tele`, `setbot`, `setrep`, `pvp` and `attune` stay `SEC_GAMEMASTER`.
- Before the existing lookup, `ResolveGetTarget` decides the target (pure rule `DecideGetTarget`):

| Issuer | No name given | Names themself | Names someone else |
| --- | --- | --- | --- |
| Console | existing behaviour (needs a name) | – | existing behaviour |
| GM (security ≥ GAMEMASTER) | existing: selection, else self | existing | existing |
| Ordinary player | **themself** (a selected target is ignored) | themself | **refused**: "You can only view your own progression." |

- The existing line "Progression Level for X = N" is unchanged, so behaviour with paid expansions OFF is the same.
- With paid expansions ON and a normal-account target, one more line shows the **earned** stage and the **current access** stage (for example an unpaid stage 13 shows access 12). Non-GMs only ever see themselves, so nobody sees another player's purchases.

## `.ip set`: warning and audit line when a GM grants an era without a purchase

With paid expansions ON and a normal-account target, TBC and Wrath ownership are read just before and just after the existing `ForceUpdateProgressionState`. If the command **newly grants** an era (pure rule `NewlyGranted`: not owned before, owned after; in practice crossing into stage 8+ for TBC or 14+ for Wrath without a purchase):

- the GM sees an orange warning naming the era and the target, and that no purchase record was created;
- one `module` log line records:
  - the issuer (character name and account id, or `console`);
  - the target character GUID;
  - the old and new raw stages;
  - TBC and Wrath ownership before and after.

**Not changed:**
- No purchase flag is written, and no stage beyond what `.ip set` already writes.
- `SEC_GAMEMASTER`, the arguments and the achievement handling are as before.
- With the feature OFF, or for bot/excluded accounts, nothing is added.
- Lowering a stage never counts as a grant. A warning when `.ip set` *removes* access is a possible follow-up, not included.
- An entitlement grant/revoke command and any `force` token are reserved for the owner.

## `.ip tele`: deliberately unchanged

The Naxx40/Onyxia40 check uses the raw stage: `progressionLevel < PROGRESSION_TBC_TIER_5`, and `PROGRESSION_TBC_TIER_5` is **13**. An unpaid stage-13 character is therefore refused this GM teleport, although the instance gate treats them as TBC (effective 12). The difference only ever refuses a teleport and never grants access, so Maintenance asked for no change. A source test keeps the raw check and the constant pinned.

## Files

- **Changed:** `src/cs_individualProgression.cpp`. One include, the `get` security level, the target-resolution call, the access-stage line, and the two audit calls around the existing write. Nothing else.
- **New:**
  - `src/IndividualProgressionGmPolicy.h` (pure rules);
  - `src/IndividualProgressionGmTools.h/.cpp` (helpers, using the #23 paid-expansion API).
- **Tests:**
  - `tests/ip_gm_policy.cpp`: 1,477 checks, GCC and Clang, C++17 and C++20. Covers the full player/GM/console target matrix, the `NewlyGranted` truth table, and a labelled **MODEL** of `.ip set` warnings on the real paid rules: unpaid 7→8 warns TBC; unpaid 13→14 warns Wrath; a paid holder, repeated set, lowering, feature OFF, and bot or excluded accounts do not warn.
  - `tests/test_ip_gm_commands_paid_source.py`: wiring.
  - `tests/ip_gm_cs_baseline.py`: reverts only the documented lines and checks the result is the reviewed blob `654119d`.
- **Workflow:** `ip-gm-commands-paid-20261101.yml`, source jobs plus a pinned full Worldserver link (never launched).

## Declared CI deviations

These files pinned the exact GM blob `654119d` and now use `tests/ip_gm_cs_baseline.py`, which accepts only that blob plus the documented lines:

- `tests/test_ip_combined_c3_f14_partial_login.py` (#19);
- `tests/test_ip_paid_expansion_source.py` (#23);
- `.github/workflows/ip-paid-expansion-entitlement-20261101.yml` (#23 native verify line).

#20's own GM test file passes unchanged.

## Rollback

Revert this branch; #23 and its parents are untouched. With paid expansions OFF, the only visible change is that ordinary players can run `.ip get` on themselves.
