# F14 pinned Map::Update / AllMapScript hook timing safety gate — source only

**Owner:** CosmicCuddle. **Maintainer:** Server-Side-Maintenance CS01-IP. **Date:** 6 November 2026.
**Status:** isolated DRAFT based on [#120](https://github.com/CosmicCuddle/mod-individual-progression/pull/120) at `bfadef922bb36dd7a244222698047c7abef92d93`. No feature activation, merge, deployment, database or client edits.

## Precise safety boundary

The original pinned [Grimfeather Core Map.cpp](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Maps/Map.cpp) runs `Map::Update(t_diff,s_diff,...)`. Following `Events.Update`, the `!t_diff` branch processes `HandleDelayedVisibility()` and **returns without firing `OnMapUpdate`**. A nonzero `t_diff` update invokes `UpdateNonPlayerObjects`, `SendObjectUpdates`, later `HandleDelayedVisibility`, `UpdateExpiredCorpses` and **only then** `sScriptMgr->OnMapUpdate(this,t_diff)`.

Consequences:

1. #120's optional `AllMapScript::OnMapUpdate` observer is a **late diagnostic**, not a guarantee of a callback during zero-time updates, not proof of exclusive owning-map serialization and not an early enough universal creature-corpse restore hook.
2. The separate `Map::DelayedUpdate` performs `RemoveAllObjectsInRemoveList` on a later phase. It must **not inherit** #108's current `Map::Update` invocation marker or become a blind prior-hidden creature restore executor.
3. Even if Core sent an OUT or CREATE message, a call attempt is **not** proof of a 3.3.5a client render/ACK. Original Claude #94 exact prior-hidden restoration debt must not be cleared by a source-order argument.

## Implementation and review

- **Only four changed paths** from #120: new `tests/test_ip_f14_map_hook_timing_source_gate.py`, new `.github/workflows/ip-f14-map-hook-timing-source-gate-20261106.yml`, this note and append-only Custom Modifications Register.
- Test executes against **exact pinned Core SHA** `0d754d1921261dfd939f91060ebc022eae280f71` and existing opt-in #120 C++ observer. The positive audit checks early return, post-packet/post-delayed-visibility/post-corpse ordering, distinct delayed object cleanup, and unchanged read-only/guarded observer. Eight adversarial mutations exercise missing early return, a callback on zero path, premature dispatch, missing late visibility, absent delayed cleanup, a delayed-removal callback and forbidden active visibility. Any drift **fails closed**, not silently skipped.
- Workflow refuses any changes to IP runtime, Core patchers, configs, SQL, data, client or original Claude ledger. Both F14 and paid flags stay OFF. This source-only job **does not compile or launch a Worldserver**; #120 has its own independent full native build.

## Remaining release blockers

Verified selected headless BE/Draenei RandomBot-only actor, full human/Altbot quest/tap/loot/pet/guardian/DoT/group/assist rights, independently established owning-map serialization, positive hiding policy (currently ABSENT), current viewer-session + creature-incarnation runtime bridge, reliable restore/reconnect without an object ACK, and real F01–F20 3.3.5a client checks remain open. Owner-installed Core/IP/Playerbots/Naxx/Services/Character Services inventory, schema, config, DBC, backups and owner approval for ONE complete offline candidate also remain missing.

**Rollback:** reject this isolated test/documentation DRAFT. No production feature or data is touched.
