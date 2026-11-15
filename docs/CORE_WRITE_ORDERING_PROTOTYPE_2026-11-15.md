# Keyed save-ordering prototype: results (15 Nov 2026, revision 2)

**Scope.** This is the isolated prototype CS01 asked for in Part09, decision 2: "durable transaction ID, ambiguous commit/restart reconciliation, duplicate purchase, queue backpressure and shutdown; no shared Core runtime changes until approval". It tests design C rev2 (`CORE_PER_CHARACTER_WRITE_ORDERING_DESIGN_2026-11-15.md`, §2 and §8).

**Revision 2** answers CS01's review 6894208993 of revision 1 (`81eddfee`). See §6 for each finding and how it's answered.

What it changes:
- **Test-only.** Nothing here is compiled into the module or the worldserver.
- **No Core change.** The dispatcher sits *above* Core's real `DatabaseWorkerPool` and doesn't modify it.
- **No owner database.** It ran against a throwaway MySQL 8.0.46 only.
- **No config, SQL or schema change** to any real database.

## 1. Files
- `tests/db_harness/keyed_dispatcher_prototype.h`: the dispatcher under test. It's header-only and uses Core's `CharacterDatabase`, `AsyncCommitTransaction`, `AsyncQuery` and `AsyncCallbackProcessor`.
- `tests/db_harness/keyed_ordering_harness.cpp`: scenarios K1–K23. Saves use the real `CHAR_REP_CHAR_SETTINGS` statement and the real IP receipt policy and purchase-state headers.
- `tests/db_harness/harness_setup.sql` adds these, for the **harness database only**:
  - `harness_write_seq(guid PK, seq, fence, owner)`: the per-character marker. A trigger makes it compare-and-set (§2).
  - `harness_write_log(guid, seq PK, fence, op)`: one row per committed keyed write.
  - `harness_purchase_journal(purchase_id PK, guid, seq, fence, op)`.
  - `harness_fail`: a trigger that always fails, used for definite failures.
- `tests/db_harness/commit_drop_proxy.py`: a local TCP proxy between Core and MySQL that drops a tagged COMMIT, or its reply (K22).
- `tests/db_harness/run_keyed_harness.sh <core_src> <core_build> <socket> <compile_prefix>`. It:
  - builds the harness against the Core build;
  - sets `innodb_lock_wait_timeout = 1` **on the disposable server**;
  - runs `keyed` at 1, 2, 4 and 8 async workers;
  - runs the two halves of the restart test as two separate processes;
  - runs `netloss` through the proxy.
- `tests/db_harness/run_keyed_mutations.sh` (same arguments) and `keyed_mutants.py`: the mutation check (§4).

## 2. How the prototype works
**Per-key FIFO.** Each character has a queue. The head op starts only when the previous op for that character has **finished and its callbacks have run**. Different characters run in parallel across all workers.

**Marker with compare-and-set (fencing).** The marker row holds `(seq, fence, owner)`. The harness trigger allows exactly two transitions:
- **write:** same fence, same owner, `seq + 1`. A keyed write's **first** statement is `UPDATE marker SET seq = n, fence = F`, so a write carrying an older fence, or a sequence number that doesn't fit, aborts its whole transaction. Because it comes first, the row lock is held for the whole transaction.
- **claim:** `fence + 1` with a new owner token, same `seq`. A process claims a key before writing to it: at its first session, after a restart, or to retake a Fenced key.

Once a claim commits, **no transaction of the old fence can ever commit**.

**Exact operation identity.** Every keyed write inserts `(guid, seq, fence, op token)` into the write log. A purchase also inserts `(purchase_id, guid, seq, fence, op token)` into the journal. The op token is the process nonce plus the op ticket.

**Ambiguous outcome means reconcile by claim.** Core reports `false` for a definite failure, a connection lost during COMMIT, and exhausted deadlock retries, and these can't be told apart. On any `false` the dispatcher **claims** the key (`UPDATE … SET fence = fence + 1 … WHERE fence = F`).
- The claim waits for any in-flight transaction on the row. Once it commits, fence `F` is dead.
- Then one read decides for good:
  - **Committed** only if the log row for `(guid, n)` carries this op's fence **and** token (and, for a purchase, the journal row is this character's, at `n`, with this token). A higher marker alone proves nothing.
  - Otherwise **NotCommitted**. For a purchase whose id is already journaled, it's **DuplicatePurchase** if the row is this character's, or **IdConflict** if it's another character's (never attributed).
  - If another claimant moved the fence first, the outcome is still decided from the log, and the key becomes **Fenced**: writes are refused until a new session re-claims.
- A claim or read that fails or times out is retried; nothing is replayed or granted without proof.

**Callbacks first.** An op's waiters are called **before** the key advances (state `Delivering`). So a queued lazy builder sees whatever state the callback restored, and anything submitted from a callback queues behind. Builder exceptions are delivered from `Process()`, never inside a `Submit` call.

**Stall.** A claim running longer than `stallMs` fires `onStalled`, and `LoginBlocked(guid)` turns true, so new logins are refused.

**Admission lease.**
- `BeginSession` issues a new epoch. Submits from an older epoch get **StaleEpoch**, and submits after `EndSession` get **NoSession**.
- Ops the old session already queued, such as its logout save, still run in order, before the new session's login read.

**Bounded queues.** Caps are checked first, in this order:
1. global waiter cap;
2. per-op waiter cap;
3. per-key depth;
4. global depth.

Any refusal returns **Backpressure**; the caller keeps memory dirty and retries, and nothing already admitted is dropped. `BeginSession` is refused over the global caps.

Coalescing merges **only a save into a pending save of the same session epoch**. Purchases never coalesce, in either direction. Only one purchase per key may be queued or running.

**Cancellation and exceptions.**
- Cancelling a queued read removes it. Cancelling a running read discards its late result.
- An exception while building a write reports **BuildFailed**, and the key advances.
- Writes are never cancelled.

**Shutdown.** `Shutdown(drainMs)` stops admission and drains every key. A key still claiming at the deadline, and the ops queued behind it, report **Unresolved**; the next process reconciles them from the marker, log and journal.

## 3. Results (Core 21ff1973 DB layer, MySQL 8.0.46, async workers 1/2/4/8)

| # | Scenario | Result (every worker count unless noted) |
|---|---|---|
| K1 | Reverse hazard: an older autosave sleeps 1.5 s in the DB, then the newer purchase is submitted | Purchase waits for the older save. Final money 600, paid, marker 2. **Newest state kept at 1/2/4/8.** (The unkeyed baseline `run_harness.sh reverse 2` overwrites the confirmed purchase.) |
| K2 | Relog fence: the old session's logout save is slow, then the new session's login read is queued | Login read is **FRESH** |
| K3 | Fence timeout | Login refused after 300 ms. The queued read is cancelled; a running read is cancelled and its late result discarded. The next read is delivered. |
| K4 | Definite failure (trigger) | NotCommitted; DB unchanged, marker 0. The next save is Committed at seq 1. |
| K5 | Lost ack of a **committed** purchase (hook) | Committed after one reconciliation. One transaction, one journal row, no replay. |
| K6 | Same purchase id replayed, same character | **DuplicatePurchase**, charged once. The replayed debit is undone and the next save converges. |
| K7 | Lease | Old epoch gets StaleEpoch, closed lease gets NoSession, unknown character gets NoSession |
| K8 | Parallelism | 4 keys × 1 s took ≈4.0 / 2.0 / 1.0 / 1.0 s at 1/2/4/8 workers. One key with 2 × 1 s took ≈2.0 s (strictly serial). |
| K9 | Coalescing and backpressure | Per-key-only cap refuses. Caps 2/2: 51 saves gave 2 queued plus 48 coalesced. Reads, saves and logins are refused at the caps. After retry, 52/52 callbacks Committed, final money 51. |
| K10 | Shutdown | 4 keys × 2 ops drained: 8/8 Committed and persisted. After that, submits get ShuttingDown and logins are refused. |
| K11 | Restart (two **processes**) | **Process 1:** a purchase's ack is lost while an external session holds the marker and runs a further same-fence write (seq 2). Shutdown can't reconcile, so the result is **Unresolved** and no access is granted. **Process 2:** its claim waits 1.4 s for that transaction, takes a new fence and sees seq 2. Journal plus log prove the purchase (exact join on guid, seq, fence and op). An **old-fence write is refused**, relog has access, and the next save is seq 3. |
| K12 | Uncertain blocks (2/4/8 workers) | A lost ack while an external session holds the marker for 3 s. The queued save does **not** run; an independent key commits in 6–9 ms; stall fires and logins are refused. After release: first save Committed, then the queued save, exactly 2 writes. |
| K13 | Exception while building a queued write | BuildFailed; the read and the save behind it still run |
| K14 | **The same transaction commits after the client gave up.** An external session holding the marker commits this op's seq, fence and token after 2.5 s. | The claim waits; the log proves **Committed**; the next save is seq 2 |
| K15 | Old fence after reconciliation | A late old-fence write is **refused** by the marker CAS |
| K16 | **Callback before next dispatch** (H2): a replayed purchase (DuplicatePurchase) with a save queued behind it | The callback restores memory first; the queued save writes **600**, not the replayed-debit 200 |
| K17 | **Unrelated higher seq** (H1): while our write is uncertain, an intruder claims and writes seq n and n+1 | Ours is **NotCommitted** (the log row at n isn't ours). The key is **Fenced**; after re-claim the next save is seq 3. |
| K18 | **Purchase id of another character** (H1) | **IdConflict**; the second character is not charged and the charge is not attributed |
| K19 | **Flood** (H3): 1000 saves, waiter caps 16 per op and 20 global | 2 queued, 15 coalesced, 983 refused. An idle key is refused at the global waiter cap. Peak waiters 20; 21 admitted, 21 delivered. |
| K20 | **Cross-session** (H3): old session's pending logout save, then a new session's save | Not merged: 3 writes in order, final state is the new session's |
| K21 | **Purchase coalescing** (H3) | A purchase never merges into a pending save, and a save never merges into a pending purchase |
| K22 | **Real network loss at COMMIT** (`netloss` mode, no hook): the proxy drops the COMMIT reply or the COMMIT itself; Core reconnects and reports failure | Reply lost: **Committed**, one write, no replay. COMMIT never sent: **NotCommitted**, DB unchanged, the next save converges. Purchase with reply lost: **Committed**, one journal row, paid. Three reconciliations, all from Core's own `false`. |
| K23 | **Two writers** (H1): a second dispatcher instance claims the key; the first still thinks it owns it | The first one's next write fits the sequence but carries the old fence, so the CAS refuses it. NotCommitted, then Fenced. The new owner commits. |

Checks per run: 138 at 1 worker (K12 is skipped, see §5), 148 at 2, 149 at 4 and 8. Plus 3 and 6 for the restart halves, and 13 for `netloss`. **All pass.**

## 4. Mutation check (each mutant must make the harness fail)
Tooling: `tests/db_harness/run_keyed_mutations.sh` applies each mutant in `tests/db_harness/keyed_mutants.py` to a copy of the header, rebuilds, and runs `keyed 4`.

**22/22 mutants are killed** (`keyed 4`; first failing scenario shown).

| Mutant | What it breaks | Killed by |
|---|---|---|
| M2 | Advance the key on any `false` (rev1 without reconciliation) | K5 (lost ack reported NotCommitted) |
| M3 | Reconcile by reading **without claiming first** | K4, K6, K12, and K14 (a late-committed op reported NotCommitted) |
| M4 | A higher marker counts as proof (rev1 `dbSeq >= seq`) | K17 (the intruder's seq counted as ours) |
| M14 | Treat `false` as committed (auto-grant) | K4 |
| M22 | Write without carrying the fence | K23 (the stale writer's write commits after a takeover) |
| M23 | Fenced admission ignored | K17 |
| M7 | No duplicate-purchase detection | K6 |
| M7b | Another character's journal row attributed as DuplicatePurchase | K18 |
| M6 | No epoch check | K7 |
| M16 | Closed lease ignored | K7 |
| M17 | Waiters called **after** the key advances (rev1 order) | K16 (the queued save writes the pre-callback state) |
| M11 | No per-key depth cap | K9 |
| M12 | No global depth cap (writes, reads, logins) | K9 |
| M13 | No coalescing | K9 |
| M18 | No per-op waiter cap | K19 |
| M19 | No global waiter cap | K19 |
| M20 | Coalescing across session epochs | K20 |
| M21 | Purchases coalesce | K21 |
| M8 | No shutdown drain | K10 |
| M9 | A cancelled running read is still delivered | K3 |
| M10 | A build exception does not advance the key | K13 (the drain times out) |
| M15 | No stall report | K12 |

Not mutation-tested, with the reason:
- **Per-key FIFO itself:** its "mutant" is the unkeyed baseline, `run_harness.sh reverse 2`, which overwrites a confirmed purchase.
- **The CAS trigger:** it's harness schema, not header code; it's exercised by K11b, K15 and K23.
- **K22:** it runs only in `netloss` mode, and its outcome logic is the same as K5 and K4.

## 5. Limits and what this does NOT show
- **Layered, not in-pool.**
  - The prototype orders ops *before* they reach Core's queue. The design puts the state machine inside `DatabaseWorkerPool`.
  - Thread-safety of that move, multi-key ops (trade, mail, auction house, guild bank) and the full writer inventory (§8.1, 70 statements) belong to the Core lane under CS01.
  - Unkeyed writers don't use the marker, so fencing only covers keyed writers.
- **One worker.** A reconciliation claim that waits on a lock occupies the only async worker, so all async DB work stalls until the lock clears or times out (K12 is skipped at 1 worker).
  - In-pool, reconciliation should run on a dedicated connection.
  - With `WorkerThreads = 1` the interim mitigation still orders writes, but an uncertain key can stall the whole async queue for up to the lock-wait timeout per retry.
- **Production schema is an owner decision.** The marker, its CAS trigger, the write log (and its retention: the harness never prunes it) and the journal are harness-only. No SQL is proposed for any real DB.
- **The real network loss is a local proxy:**
  - TLS is disabled on that path only.
  - It covers the reply lost after COMMIT, and COMMIT never sent.
  - A loss in the middle of the server's own COMMIT processing, or a server crash, was not injected.
  - The marker-before-first-lock race is closed by the claim, not by mutation evidence: K15 and K23 show that an old-fence write can't commit after a claim.
- **Coalescing needs lazy builders.** It depends on whole-state builders running at dispatch time on the owner thread. Core's `SaveToDB` builds eagerly, so in-pool coalescing would need the writer audit to classify partial or non-idempotent writes first.
- **Saves before a purchase.** A save queued before a purchase may write the purchase's staged state (debit and receipt) before the purchase's own transaction (K21). That matches IP's rule that the receipt in settings is the truth; the journal only dedupes purchase ids.
- **Fixed timings.** Fence timeout 300 ms, stall 1 s, drain 1.5 s, lock wait 1 s and retry 50 ms are test values, not proposals.

## 6. CS01 review 6894208993: findings and answers
| Finding | Answer | Evidence |
|---|---|---|
| HIGH: `dbSeq >= seq` declared committed; a later marker alone isn't proof | Committed requires this op's own log row (fence + token) and, for a purchase, its own journal row. The marker is compare-and-set with fencing; reconciliation claims before it reads. | K14, K15, K17, K23; mutants M3, M4, M22 |
| HIGH: journal existence only; a duplicate id on the wrong guid | The journal is bound to guid, seq, fence and op. Another character's row gives **IdConflict**. | K18; mutant M7b |
| HIGH: monotonic fencing across writers and restart; conditional marker advance | Claim on first session, restart, re-take and reconciliation; the CAS trigger refuses old-fence writes. | K11b (old-fence write refused after restart), K15, K23; mutants M22, M23 |
| HIGH: `Finish`/`Pump` before waiters; a queued lazy builder captures pre-callback state | Waiters run first (`Delivering`), then the key advances | K16; mutant M17 |
| HIGH: coalescing before cap checks; unlimited waiters; cross-session or purchase coalescing | Caps first; per-op and global waiter caps; same-epoch saves only; purchases never coalesce | K19, K20, K21; mutants M18–M21 |
| Real COMMIT network loss unproven | Injected through a local proxy (reply lost, and COMMIT not sent) | K22 |
| Marker-before-first-lock race | Closed by the claim: once it commits, no transaction of the old fence can commit, whatever it had executed | K15, K23 |
| One worker blocks all async work | Unchanged; documented (§5). In-pool design: a dedicated reconciliation connection. | — |
| Multi-key writer inventory | Unchanged; Core lane | — |
