# IP GM commands: temporary CS01 Maintenance implementation — 1 November 2026

**Owner rule:** CosmicCuddle's established `.ip get` must be available to any player to view **their own** progression. Only a GM may inspect another player's progression, set markers, teleport, or synchronize bots. This is a DRAFT implementation for review; it is not installed or enabled.

## Actual code paths

- A single command table change: only `ip get` becomes `SEC_PLAYER`. The seven other commands remain `SEC_GAMEMASTER`.
- Ordinary player calls `.ip get` with no name read `handler->GetSession()->GetPlayer()`, ignoring selected other targets. A named player different from that same connected player is **refused**. Explicit own name is accepted. GM/console follows the historic named/selected route and retains `Console::Yes`.
- The historical output `Progression Level for X = raw` is **unchanged**. When the new paid gate is ON and the target is a normal account, an extra labelled `Earned progression stage: X | Current access stage: Y` is displayed. A regular player never receives another player's paid access, stage, flags or markers.
- `.ip set` stays a privileged manual override, with the same stage write, achievement rollback and phasing. When paid gate ON and normal-account target newly obtains TBC/Wrath access, emit a GM warning and a nonsecret `module` log of console-safe issuer, issuer account id, target low GUID, old/new raw, and before/after era ownership. **No purchase flag is created**, no `force` token is added, no existing GM access is removed. If logging is configured to suppress the `module` category, the durable operational record may be missing; confirm retention before owner staging. GM stage grants can be revoked by later stage reduction, unlike a paid purchase.
- `.ip tele` is intentionally untouched. The actual `PROGRESSION_TBC_TIER_5` constant equals **13**, not 12. Historical GM tele requires raw `<13`. Unpaid raw13 may effectively have access12, but GM tele still refuses, a restriction rather than free Wrath. Bots/excluded accounts retain old behavior.
- `.ip get` cannot be used for offline characters; previous GM behavior required online target too.

## Tests and source preservation

- `src/IndividualProgressionGmCommandAccessPolicy.h`: pure C++ decision table for self-only reads and *newly granted* era warnings; `tests/ip_gm_command_access_policy.cpp` exercises offline portable matrices under GCC/Clang C++17/C++20.
- `tests/test_ip_gm_self_audit_source.py`: exact command permission, self-only route, no paid flag write, old raw GM tele constants and no spoofed target. Requires hosted execution to claim PASS.
- Older IP #19/#23 source pinning tests and the exact native link workflow now pin only the approved new `cs_individualProgression.cpp` Git blob `278aaa48dc5fb7770bd6013e659e4872752d4b03`. **Source pin updates are explicit scope exceptions**, not carte blanche to rewrite command behavior.
- Developer branch derives from [paid expansion DRAFT #23](https://github.com/CosmicCuddle/mod-individual-progression/pull/23) `8dc16ffb3ca7f70a9c7ddc7cfbe6c5bd40088e37`. It does not mutate Claude's claimed branch, IP #19/#20/#24, SQL or Core.

## Manual staged acceptance (NOT yet performed)

1. Player A runs `.ip get` with no selected unit; displays A. A selects B, runs `.ip get`; still displays A.
2. A explicitly types B and is refused; A explicitly types own name and succeeds.
3. GM views player B by name/selection; console with B name succeeds; console without target refuses; regular player cannot use any other `.ip` GM command.
4. With paid ON, unpaid stage13 displays earned13/current12; paid stage13 displays earned13/current13. Feature OFF yields unchanged historical output and gameplay stage.
5. GM uses `.ip set B 8` after 7, and `.ip set B 14` after unpaid13: a warning and operational audit appear with no paid bit. Repeated set, already-owned or bot target must not trigger false paid-grant warning; GM rights remain.
6. Historical Naxx/Onyxia GM `.ip tele` under raw13 still refuses; preserve all historical quest markers. Verify Worldserver log category/format and actual permissions on the pinned runtime before enabling any paid mode.
7. Perform server backups, data restore/restart tests and production risk review before CosmicCuddle authorises any merge or deployment.

**Rollback:** revert this isolated development PR, retaining original #23 paid implementation and all historic GM commands; no migrations or client patch. Runtime rollback would require owner-approved binary/config restoration and server maintenance window.