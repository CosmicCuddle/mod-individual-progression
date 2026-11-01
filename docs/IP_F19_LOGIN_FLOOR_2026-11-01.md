# F19: level floor and progression limit for the F14 saved-stage login guard

**Owner:** CosmicCuddle
**Created:** 1 November 2026
**Status:** DRAFT only. Unmerged, not installed, not game-tested. The F14 login safeguard stays **OFF by default**; this change only affects its ON path.

| Item | Value |
| --- | --- |
| Branch | `fix/ip-f19-login-preserve-level-floor-20261101` |
| Parent | [DRAFT #20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20) at `5fe0f9105d55d249aa55a25510d3fa23e3d84148` (contains the #19 F14 login guard) |
| Findings | F19-1, F19-2 and F19-6 from the independent review of #19 ([Coordination #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6559437888)); fix requested by Maintenance ([decision](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6559542934)) |
| Claim | [Coordination #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13) |

## Problem

With `IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin = 1`, the #19 guard keeps the saved stage (8–18) of a verified, headless Blood Elf/Draenei RandomBot instead of running the original level-based login write. That write is: level ≤60 → stage 0, 61–70 → stage 8, 71+ → stage 13.

- **F19-1:** a level 71–80 bot with saved stage 8–12 kept 8–12, while the original write gives 13. Turning the safeguard ON could therefore leave a bot **lower** than the original behaviour.
- **F19-2:** the preserved stage was not compared with a nonzero `progressionLimit`.
- **F19-6:** a comment said not to call the `currentBots`-backed predicate, but the GUID overload used is itself `currentBots`-backed.

## Change

Inside the existing guard, one extra condition, via the pure header `src/IndividualProgressionF14LoginPolicy.h`. The saved stage is preserved only if:
- it is **at least** the original level stage; and
- it is **not above** a nonzero `progressionLimit`.

Otherwise the original level write runs, exactly as before.

| Level | Saved stage | Before (ON) | After (ON) |
| --- | --- | --- | --- |
| ≤60 | 8–18 | kept | kept (if within limit) |
| 61–70 | 8–18 | kept | kept (if within limit) |
| 71+ | 8–12 | kept (lower than original 13) | **original write: 13** |
| 71+ | 13–18 | kept | kept (if within limit) |
| any | above a nonzero limit | kept | **original write** |

**Unchanged:**
- flag OFF, which is identical to #20;
- humans, non-TBC races, non-headless sessions, Altbots and unknown identities;
- the Playerbots identity check, which is still the last step;
- the original write itself;
- every other file (core, header, conf, Hunter, GM, group policy, SQL).

The comment above the `IsRandomBot` call now says the GUID overload is `currentBots`-backed and safe only on the verified WorldScript login thread.

With the current level-60 cap, F19-1 is forward compatibility rather than a live-severity issue.

## Tests

- `tests/f14_login_preserve_policy.cpp`: 1,953 checks, GCC and Clang, C++17 and C++20. Covers:
  - level boundaries 1–85;
  - the full level × saved stage × limit matrix;
  - the named review cases (levels 71–80 with stages 8–12; level 60 with stage 8; limits 8 and 13);
  - a labelled **MODEL** of the whole login decision, showing it is never below the original write, stays within the limit, and leaves OFF / human / non-headless / unverified cases unchanged.
- `tests/test_ip_f19_login_floor_source.py`: wiring. The condition sits inside the existing guard, after the flag/race/headless checks and before the identity call. The original write is unchanged, and the diff against #20 is exactly the documented lines.
- All inherited #16, #19 and #20 checks run.
- Workflow `ip-f19-login-floor-20261101.yml`: source jobs plus a pinned full Worldserver configure, compile and link (Core `8daffdab`, Playerbots `e88cc409` with the two runner-only compatibility edits, Naxx `80e9de1c`, `-DCOSMIC_IP_F14_REQUIRE_PLAYERBOTS`). It **never launches** the server.

## Declared CI deviations on this branch

1. `.github/workflows/ip19-gm-null-safety-integration-20261101.yml` (#20): the same 5-line `if:` as on the #23 branch, scoping #20's frozen six-path step to #20's head. Its GM tests still run.
2. `tests/test_ip_combined_c3_f14_partial_login.py` (#19): reads `IndividualProgressionPlayer.cpp` with **only** the documented F19 lines reverted (the include, two comment lines, the extra condition, and the corrected comment). All #19 assertions then stay as strict as on #19; any other change still fails.

## Relationship to #23

#23 (paid expansion) is a sibling on the same parent. It changes the group blocks of `IndividualProgressionPlayer.cpp`, while F19 changes only the include line and the login guard. Both branches carry the identical deviation-A edit. If both are later combined, the #19 test file needs both normalisations; that is a reviewed integration step.

## Rollback

Leave the F14 flag at 0 (the default); F19 has no effect then. To remove the code, revert this branch; #20 and #19 are untouched.
