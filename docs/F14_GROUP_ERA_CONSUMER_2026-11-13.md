# F14 temporary group-era consumer — pure draft

Owner: CosmicCuddle. Codex coding support under CS01-IP's continuous queue in [Coordination #51](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51). This child of group-floor [PR #175](https://github.com/CosmicCuddle/mod-individual-progression/pull/175) adds a pure temporary-state lifecycle model. F14/Paid remain OFF.

## Contract

`IndividualProgressionF14GroupEraConsumer.h` accepts an **independently verified eligible-bot** fact; bot GUID and session epoch; map ID and generation; group GUID and membership generation; leader GUID and revision; race; earned stage; and group stage. Nonzero generations must increase before a context can be reused. A leader or group-era change must advance the leader revision; a new membership must advance the membership generation; relog and map changes must advance their respective generation. `mapId = 0` is valid. These are caller obligations, not facts inferred by this model. The current Playerbots/Core source has not yet been shown to supply them all.

The tracker stores only a bounded, ephemeral temporary era. Race 10/11 has a TBC grouped-view minimum. Other races can downsync to Vanilla. With earned Wrath unchanged, a race 10/11 bot can show TBC, Wrath, then TBC in successive group contexts. `Revoke` accepts an exact captured context and a departure reason (leave, disband, kick, leader change, relog, map change or session invalidation). It ends the override and returns `EarnedView` from the **supplied earned stage**. A race 10/11 missing/Vanilla earned marker is exposed as `EarnedFloorMissing` plus Vanilla evidence, never silently repaired. The caller must reacquire current earned evidence; the tracker does not retain or write it.

For a given bot, a lower version is stale and cannot erase a newer override. Equal-version conflicting identity, race or group-stage facts quarantine that bot's override. Equal-version replay after revocation cannot revive it; a genuinely newer context is required. Unknown eligibility, incomplete identity, invalid stage, and exhausted bounded slots yield no temporary override. An exact captured teardown still clears and tombstones a temporary override after eligible-bot proof is lost; its returned view is `UnverifiedBot` with no usable era. No slot is evicted or silently reused. This bound is a prototype safety limit, not a sustained-runtime capacity proof; a future consumer needs lifecycle and retirement evidence before wiring.

`Current` returns a temporary era only on an exact verified identity and matching group facts. Otherwise it returns exact earned evidence where input permits. It never treats its temporary era as a quest marker, entitlement, cap, or persistence instruction. `ForceUpdateProgressionState`, group admission, Playerbots hooks, SQL/DB/config/client files, Claude's social paths and runtime adapters are unchanged. The model has no concealment or restoration authority.

## Tests and limits

The standalone test covers all 10 supported races × 19 earned stages × 19 group stages; TBC/Wrath transitions; all seven departure reasons; stale session revocation and old grouped replay; same-version ambiguity; another bot surviving local quarantine; unknown identity; missing earned floor; and capacity. The source contract applies in-memory regressions for stale-revoke and TBC-on-departure behavior, and checks the absence of runtime/write/visibility dependencies.

```sh
node tests/test_f14_group_era_consumer_source.js
for compiler in g++ clang++; do
  for standard in c++17 c++20; do
    for define in "" -DNDEBUG; do
      "$compiler" -std="$standard" -Wall -Wextra -Werror $define -Isrc tests/f14_group_era_consumer_test.cpp -o /tmp/f14-group-era-consumer
      /tmp/f14-group-era-consumer
    done
  done
done
```

Local native matrix, macro-ON Worldserver link and gameplay are not evidenced. No hosted Actions are requested for this source-only draft. Rollback: abandon/revert this child branch; no migration or live-state rollback applies.
