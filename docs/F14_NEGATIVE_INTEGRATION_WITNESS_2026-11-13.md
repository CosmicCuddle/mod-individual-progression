# F14 negative integration witness — synthetic pure test

Owner: CosmicCuddle. Codex coding support under the [Coordination #51 batch](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6851977983). This child of lifecycle PR #174 composes the actual pure `CurrentPassIdentity`, E1 `SendAttemptCorrelation`, `ViewerRestoreProtocol` and `PriorHiddenLedger` interfaces in one **test-only** witness. It adds no runtime adapter, positive hide policy or client action.

The fixture seeds **preexisting synthetic F14 server-hide records** through the protocol's existing pure test interface. That seed represents historical debt; it is not a live hide, client delivery or permission to conceal. The witness itself only reads and revokes. It requires exact current viewer GUID/login epoch/map generation/map key, active and batch-observed pass token, source-attributed E1 send facts, and an exact current creature incarnation before treating a correlation result as matched. E1's synthetic token has no pinned-Core producer. A send call is not client presentation.

Negative cases covered: nested/replayed/invalid-identity passes cannot upgrade a nominated correlation; delayed old-session pass revoke cannot erase a newer pass or candidate; the protocol's raw-GUID viewer revoke marks both retained login epochs pending; a matched send leaves every restore obligation pending; pass history saturation, E1 candidate capacity and local quarantine cannot clear frozen/global obligations; another viewer survives local pass ambiguity; creature incarnation mismatch at nomination or composed send boundary never matches. The source contract mutates both the witness route and ledger raw-GUID revoke into epoch routing, broadens a stale revoke into newer-correlation cleanup, adds forbidden restore-completion/global-reset calls, and spreads viewer quarantine. Every mutation must fail. It also rejects positive suppression methods in the witness harness.

Follow-up negative fixtures at this child head also establish that a server relink and proposed batched handoff after a matched send still leave restoration debt pending; an unknown pass end blocks a late nomination; rights change drops ephemeral candidates; creature retirement/map unload retain debt; and same-key incarnation reuse freezes restoration. A source mutation replaces the post-handoff debt assertion with an improper client-proof completion call and must fail. These remain synthetic pure tests. The proposed batch flush callback and independently verified client receipt do not exist at the pinned runtime.

**Boundary:** ephemeral pass/correlation evidence may be dropped or quarantined. The #94/#85 restoration debt persists until separate verified client restoration, which this test cannot supply. The raw-GUID `PriorHiddenLedger::RevokeViewer` intentionally spans every login epoch and is unchanged. No code here calls client-proof completion or frozen-reset APIs. Local unit composition cannot establish callback ordering, Core pass producer, ownership/thread coupling, full human rights, client restoration, installed source tuple or gameplay.

```sh
node tests/test_f14_negative_integration_source.js
for compiler in g++ clang++; do
  for standard in c++17 c++20; do
    for define in "" -DNDEBUG; do
      "$compiler" -std="$standard" -Wall -Wextra -Werror $define -Isrc tests/f14_negative_integration_witness.cpp -o /tmp/f14-negative-witness
      /tmp/f14-negative-witness
    done
  done
done
```

Local native matrix, macro-ON Worldserver and gameplay are not evidenced. Hosted Actions are reserved for a coherent milestone. F14/Paid OFF; no merge/deploy/DB/SQL/config/DBC/client/host action. Rollback: abandon/revert this test-only child branch.
