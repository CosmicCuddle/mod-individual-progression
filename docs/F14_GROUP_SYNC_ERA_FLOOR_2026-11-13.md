# F14 minimum-TBC temporary group-era floor — pure draft

Owner: CosmicCuddle. Codex coding support, coordinated with CS01-IP in [Coordination #51](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51). Owner policy is recorded in [Coordination #52](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/52).

## Source and intended behavior

- Repository: `CosmicCuddle/mod-individual-progression`
- Sibling branch: `feature/codex-f14-min-tbc-group-floor-20261113`
- Parent: IP172 `aed05a92f262362608f5d643c8570b504189aae0`
- Paths: new `src/IndividualProgressionF14GroupSyncEraFloor.h`, focused C++ and source-contract tests, this note, and an additive Custom Modifications Register entry.

The pure policy accepts an independently established eligible-bot fact, race, earned quest stage and current group stage. `WhileGrouped` returns only an **effective temporary era**. `AfterLeaving` reports the exact **earned era** and diagnoses a missing race10/11 TBC floor. Neither returns a `ProgressionState`, writes rewarded quests, grants entitlement, changes the progression cap, or infers RandomBot identity from race.

| Bot and context | Temporary effective era |
| --- | --- |
| Normal-race eligible bot in Vanilla/TBC/Wrath group | Group's era, including Vanilla downsync |
| Eligible Blood Elf or Draenei bot in Vanilla group | TBC |
| Eligible Blood Elf or Draenei bot in TBC/Wrath group | Group's TBC/Wrath era |
| Eligible bot after leaving | Exact earned era; race10/11 earned Vanilla is diagnostic `EarnedFloorMissing` and unusable as solo policy evidence |

Thus an earned Wrath race10/11 bot may temporarily show TBC with a Vanilla or TBC group, rise to Wrath with a Wrath group, and expose its earned Wrath era on leave. Its original earned stage stays intact. A missing/Vanilla earned marker for race10/11 produces temporary TBC **only while grouped**. On leave, the model reports earned Vanilla plus `EarnedFloorMissing` so the separate birth/login/reset data defect remains visible; it does not invent an earned stage-8 marker or a healthy solo TBC era. Unknown bot identity or invalid race/stage refuses to produce a usable era.

The model reuses the existing expansion boundaries: Vanilla 0–7, TBC 8–12, Wrath 13–18. It does not override `EnforceGroupRules` admission in [IP PR #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3). It also differs from [IP PR #45](https://github.com/CosmicCuddle/mod-individual-progression/pull/45)'s skip-only downgrade guard and [IP PR #80](https://github.com/CosmicCuddle/mod-individual-progression/pull/80)'s earned-stage write protection. Those behaviors remain untouched.

## Integration boundary

Current group sync calls `ForceUpdateProgressionState` on bot/excluded-account members. Passing this temporary era into that destructive path would violate the preservation rule. Runtime integration needs a separately reviewed temporary group-era consumer with a proven eligible-bot identity and teardown on leave/disconnect; this model does not provide one. The exact treatment of Altbots/SelfBots/unknown identities in that future caller must be decided from source and owner policy, not race alone. The server's Vanilla cap does not become a TBC progression grant merely because the model returns a temporary TBC group era.

Claude CS03-IP owns the safer social-interaction slice; no Claude adapter/runtime path is changed here. F14 visual disappearance is paused. No positive concealment, WHO/social behavior, Playerbots call, config, SQL, DB, DBC, client, host or production action is included.

## Tests and evidence

The standalone C++ test uses static assertions and an exhaustive 10-supported-race × 19-earned-stage × 19-group-stage matrix, plus explicit TBC/Wrath up/down, exact earned-era departure and missing-floor diagnosis, unverified identity and invalid race/stage inputs. The source-contract test checks the 0/8/13 boundaries, explicit identity input and absence of runtime/progression writes. It also applies an in-memory mutation restoring the old TBC-on-leave bug and requires the guard to reject it.

```sh
node tests/test_f14_group_sync_era_floor_source.js
for compiler in g++ clang++; do
  for standard in c++17 c++20; do
    for define in "" -DNDEBUG; do
      "$compiler" -std="$standard" -Wall -Wextra -Werror $define -Isrc tests/f14_group_sync_era_floor_test.cpp -o /tmp/f14-group-era-floor
      /tmp/f14-group-era-floor
    done
  done
done
```

At PR #175's original head `42f89c75`, CS01 independently compiled and ran the pure model with GCC and Clang, C++17/C++20, normal/NDEBUG: 8/8 PASS ([source review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6851421141)). That evidence does **not** cover this subsequent semantic correction. Local source-contract test must be rerun on the corrected head; GCC, Clang, MSVC and WSL are absent locally, so its native matrix is **not run here**. A standalone pass would not prove installed module behavior, group join/leave gameplay, or runtime identity classification. Hosted CI is reserved for a coherent milestone; no run is requested by this pure draft.

Status: source only, F14/Paid OFF, unmerged and not deployed. Rollback: abandon/revert this sibling branch; there are no data or live-state changes.
