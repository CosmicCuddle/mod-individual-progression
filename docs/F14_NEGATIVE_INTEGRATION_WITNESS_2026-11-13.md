# F14 negative integration witness — synthetic pure test

Owner: CosmicCuddle. Codex coding support under the [Coordination #51 batch](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6851977983). This child of lifecycle PR #174 composes the actual pure `CurrentPassIdentity`, E1 `SendAttemptCorrelation`, `ViewerRestoreProtocol` and `PriorHiddenLedger` interfaces in one **test-only** witness. It adds no runtime adapter, positive hide policy or client action.

The fixture seeds **preexisting synthetic F14 server-hide records** through the protocol's existing pure test interface. That seed represents historical debt; it is not a live hide, client delivery or permission to conceal. The witness itself only reads and revokes. It requires exact current viewer GUID/login epoch/map generation/map key, active and batch-observed pass token, source-attributed E1 send facts, and an exact current creature incarnation before treating a correlation result as matched. E1's synthetic token has no pinned-Core producer. A send call is not client presentation.

Negative cases covered: nested/replayed/invalid-identity passes cannot upgrade a nominated correlation; delayed old-session pass revoke cannot erase a newer pass or candidate; the protocol's raw-GUID viewer revoke marks both retained login epochs pending; a matched send leaves every restore obligation pending; pass history saturation, E1 candidate capacity and local quarantine cannot clear frozen/global obligations; another viewer survives local pass ambiguity; creature incarnation mismatch at nomination or composed send boundary never matches. The source contract mutates both the witness route and ledger raw-GUID revoke into epoch routing, broadens a stale revoke into newer-correlation cleanup, adds forbidden restore-completion/global-reset calls, and spreads viewer quarantine. Every mutation must fail. It also rejects positive suppression methods in the witness harness.

Follow-up negative fixtures at this child head also establish that a server relink and proposed batched handoff after a matched send still leave restoration debt pending; an unknown pass end blocks a late nomination; rights change drops ephemeral candidates; creature retirement/map unload retain debt; and same-key incarnation reuse freezes restoration. A nomination against a previously ended, otherwise unknown pass token calls `AdvancePass`: its exact result is `TokenReorderedOrReused`, and it poisons that viewer's ephemeral correlation evidence, so a second nomination returns `ViewerPoisoned`. This is stronger than the earlier test's incorrect `PassClosed` expectation; both refuse to upgrade evidence, and neither clears ledger debt. A source mutation replaces the post-handoff debt assertion with an improper client-proof completion call and must fail. These remain synthetic pure tests. The proposed batch flush callback and independently verified client receipt do not exist at the pinned runtime.

Claude's [independent review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6857731743) passed the earlier `b75cabf7` witness and requested a G2 negative case. The follow-up fixture now nominates a source-attributed send after `Begin` but **before** `ObserveBatch`; `TrySend` must refuse the match and the restore debt stays pending. Only after the exact batch observation may the synthetic match occur, and even then it is not a client receipt. The source contract requires and mutates this ordering. The earlier 8/8 independent native matrix is evidence for `b75cabf7`, not this added test-only head.

Claude's later [exact-head run of `49af2465`](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6861241700) found the witness red at its unknown-end assertion in all eight configurations. The preceding end creates a closed token; `Nominate` treats reopening that token as `TokenReorderedOrReused` and poisons the viewer. This child corrects the **test expectation only** and asserts the follow-on `ViewerPoisoned` result. It does not change E1 correlation implementation or weaken the refusal. The new head still needs an independent exact-head native matrix; the local source contract alone cannot close it.

**Boundary:** ephemeral pass/correlation evidence may be dropped or quarantined. The #94/#85 restoration debt persists until separate verified client restoration, which this test cannot supply. The raw-GUID `PriorHiddenLedger::RevokeViewer` intentionally spans every login epoch and is unchanged. No code here calls client-proof completion or frozen-reset APIs. Local unit composition cannot establish callback ordering, Core pass producer, ownership/thread coupling, full human rights, client restoration, installed source tuple or gameplay.

```sh
node tests/test_f14_negative_integration_source.js
for compiler in g++ clang++; do
  for standard in c++17 c++20; do
    for define in "" -DNDEBUG; do
      "$compiler" -std="$standard" -Wall -Wextra -Werror -pedantic $define -Isrc tests/f14_negative_integration_witness.cpp -o /tmp/f14-negative-witness
      /tmp/f14-negative-witness
    done
  done
done
```

Local native matrix, macro-ON Worldserver and gameplay are not evidenced. Hosted Actions are reserved for a coherent milestone. F14/Paid OFF; no merge/deploy/DB/SQL/config/DBC/client/host action. Rollback: abandon/revert this test-only child branch.
