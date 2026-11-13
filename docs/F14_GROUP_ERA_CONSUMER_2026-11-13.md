# F14 temporary group-era consumer — pure draft

Owner: CosmicCuddle. Codex coding support under CS01-IP's continuous queue in [Coordination #51](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51). This child of group-floor [PR #175](https://github.com/CosmicCuddle/mod-individual-progression/pull/175) adds a pure temporary-state lifecycle model. F14/Paid remain OFF.

## Contract

`IndividualProgressionF14GroupEraConsumer.h` accepts an **independently verified eligible-bot** fact; bot GUID and session epoch; map ID and generation; group GUID and membership generation; leader GUID and revision; race; earned stage; and group stage. Nonzero generations must increase before a context can be reused. A leader or group-era change must advance the leader revision; a new membership must advance the membership generation; relog and map changes must advance their respective generation. `mapId = 0` is valid. These are caller obligations, not facts inferred by this model. The current Playerbots/Core source has not yet been shown to supply them all.

Claude's independent [#176 review recorded in coordination #51](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6857731743) passed the pure model with a documentation condition. For **one bot**, ordering is lexicographic `(sessionEpoch, mapGeneration, membershipGeneration, leaderRevision)`. A lower-order field may reset only after a higher-order field increases. Group GUID and leader GUID are equality facts, **not** ordering keys. Membership and leader versions therefore need per-bot or global non-reusing issuers; per-group/per-leader counters can go backwards when a bot moves or a leader changes. A violating producer receives `Stale` or `Ambiguous` and no temporary view. `Revoke` for a bot that was never `ObserveGrouped` creates a retained tombstone, so a runtime adapter must avoid meaningless revokes or establish a proven retirement design.

The tracker stores only a bounded, ephemeral temporary era. Race 10/11 has a TBC grouped-view minimum. Other races can downsync to Vanilla. With earned Wrath unchanged, a race 10/11 bot can show TBC, Wrath, then TBC in successive group contexts. `Revoke` accepts an exact captured context and a departure reason (leave, disband, kick, leader change, relog, map change or session invalidation). It ends the override and returns `EarnedView` from the **supplied earned stage**. A race 10/11 missing/Vanilla earned marker is exposed as `EarnedFloorMissing` plus Vanilla evidence, never silently repaired. The caller must reacquire current earned evidence; the tracker does not retain or write it.

For a given bot, a lower version is stale and cannot erase a newer override. Equal-version conflicting identity, race or group-stage facts quarantine that bot's override. Equal-version replay after revocation cannot revive it; a genuinely newer context is required. Unknown eligibility, incomplete identity, invalid stage, and exhausted bounded slots yield no temporary override. An exact captured teardown still clears and tombstones a temporary override after eligible-bot proof is lost; its returned view is `UnverifiedBot` with no usable era. No slot is evicted or silently reused. This bound is a prototype safety limit, not a sustained-runtime capacity proof; a future consumer needs lifecycle and retirement evidence before wiring.

`Current` returns a temporary era only on an exact verified identity and matching group facts. Otherwise it returns exact earned evidence where input permits. It never treats its temporary era as a quest marker, entitlement, cap, or persistence instruction. `ForceUpdateProgressionState`, group admission, Playerbots hooks, SQL/DB/config/client files, Claude's social paths and runtime adapters are unchanged. The model has no concealment or restoration authority.

Adversarial review: an exact captured teardown is allowed after eligible-bot proof is lost; this removes/tombstones the override but returns no usable era. An old teardown remains stale even after newer membership/session context and even if its eligibility fact is false. A lower numeric generation after `uint64` wrap is refused; the model cannot prove that an equal version after a full ABA cycle is genuinely new, so runtime producers must never reuse nonzero session/map/membership/leader generations within a tracker lifetime. When every slot is occupied, a new bot is refused even if an existing slot is a tombstone; the existing bot can still advance to a newer context. `Revoke` computes its earned-era result from the **current caller-supplied stage**, not a cached stage. These boundaries are tested but do not prove that Core/Playerbots provide the required generation and earned-stage facts.

The independent review also found that the current bot map-generation producer is lazy and cannot prove an unobserved A→B→A transfer; membership-generation and leader-revision producers are missing. These are runtime dependencies, not implementation facts supplied by this pure header. No generation issuer is added here.

## Tests and limits

The standalone test covers all 10 supported races × 19 earned stages × 19 group stages; TBC/Wrath transitions; all seven departure reasons; stale session revocation and old grouped replay; same-version ambiguity; another bot surviving local quarantine; unknown identity; missing earned floor; and capacity. The source contract applies in-memory regressions for stale-revoke and TBC-on-departure behavior, and checks the absence of runtime/write/visibility dependencies.

Follow-up fixtures cover a move A→B where a per-group membership counter resets and yields `Stale`; equal counter with changed group yields `Ambiguous` and no old temporary read; lower-order reset after a higher-order advance; `Current` on changed session/map/membership identity yields no override; and a blind revoke consuming a permanent slot. The source contract mutates the `Current` identity guard and requires those fixtures. These additions do not change consumer semantics. Claude's independent earlier 8/8 native matrix applies to `b40ba6f` only; this follow-up test-only head needs its own native run before claiming exact-head C++ evidence.

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
