# Keyed save-ordering prototype: results (15 Nov 2026)

**Scope.** This is the isolated prototype CS01 asked for in Part09, decision 2: "durable transaction ID, ambiguous commit/restart reconciliation, duplicate purchase, queue backpressure and shutdown; no shared Core runtime changes until approval". It tests design C rev2 (`CORE_PER_CHARACTER_WRITE_ORDERING_DESIGN_2026-11-15.md`, §2 and §8).

What it changes:
- **Test-only.** Nothing here is compiled into the module or the worldserver.
- **No Core change.** The dispatcher sits *above* Core's real `DatabaseWorkerPool` and doesn't modify it.
- **No owner database.** It ran against a throwaway MySQL 8.0.46 only.
- **No config, SQL or schema change** to any real database.

## 1. Files
- `tests/db_harness/keyed_dispatcher_prototype.h`: the dispatcher under test. It's header-only and uses Core's `CharacterDatabase`, `AsyncCommitTransaction`, `AsyncQuery` and `AsyncCallbackProcessor`.
- `tests/db_harness/keyed_ordering_harness.cpp`: scenarios K1–K14. Saves use the real `CHAR_REP_CHAR_SETTINGS` statement and the real IP receipt policy and purchase-state headers.
- `tests/db_harness/harness_setup.sql`: adds three tables for the harness database only:
  - `harness_write_seq(guid PK, seq)`: the per-character marker;
  - `harness_purchase_journal(purchase_id PK, guid, seq)`;
  - `harness_fail`: a trigger that always fails, used for definite failures.
- `tests/db_harness/run_keyed_mutations.sh` (same arguments) and `keyed_mutants.py`: the mutation check (§4).
- `tests/db_harness/run_keyed_harness.sh <core_src> <core_build> <socket> <compile_prefix>`. It:
  - builds the harness against the Core build;
  - sets `innodb_lock_wait_timeout = 1` **on the disposable server**;
  - runs `keyed` at 1, 2, 4 and 8 async workers;
  - runs the two halves of the restart test as two separate processes.

## 2. How the prototype works
**Per-key FIFO.** Each character has a queue. The head op starts only when the previous op for that character has **finished**: committed, failed, reconciled or cancelled. Different characters run in parallel across all workers.

**Durable transaction id.** Every keyed write gets the next per-character sequence number.
- Its **first** statement is `INSERT ... ON DUPLICATE KEY UPDATE seq = <n>` on the marker row. That row lock is therefore held for the whole transaction.
- A purchase also inserts its unique `purchase_id` into the journal, in the same transaction.

**Ambiguous outcome means Uncertain.** Core reports `false` for a definite failure, for a connection lost during COMMIT, and for exhausted deadlock retries, and these can't be told apart. Any `false` therefore puts the key in **Uncertain**.
- The key doesn't advance until a **locking** read resolves it: `SELECT seq ... FOR SHARE`. That read waits for any still-running server-side transaction on the row, then reads the latest committed version.
  - `seq >= n` means Committed.
  - Otherwise the result is NotCommitted. If the journal already holds this purchase id, the purchase is a DuplicatePurchase instead.
- A failed reconciliation read is retried. A null result here can only mean an error, because session init guarantees the marker row exists.
- The key is never replayed, and access is never granted without proof.

**Stall.** If a key stays Uncertain past `stallMs`, `onStalled` fires and `LoginBlocked(guid)` turns true, so new logins are refused. Reconciliation keeps retrying.

**Admission lease.**
- `BeginSession` issues a new epoch. Submits from an older epoch get **StaleEpoch**, and submits after `EndSession` get **NoSession**.
- Ops the old session already queued, such as its logout save, still run in order, *before* the new session's login read.
- The first session of a key in a process runs a locking init read of the marker, so sequence numbers continue across restarts.

**Bounded queues.**
- **Coalescing.** A whole-state save merges into that character's pending, not-yet-sent write. Builders run at dispatch time on the owner thread, so the merged write carries the newest memory, and its callbacks move to the merged write.
- **Caps.** A per-key depth cap and a global cap refuse new ops with **Backpressure**, and refuse new logins (`BeginSession` returns 0). Nothing already admitted is dropped; the caller keeps memory dirty and retries.
- **One purchase at a time.** Only one purchase may be queued or running per key; a second gets **PurchaseInFlight**.

**Cancellation and exceptions.**
- Cancelling a queued read removes it. Cancelling a running read discards its late result.
- An exception while building a write reports **BuildFailed** on the next `Process()`, never re-entering caller code.
- In every case the key advances. Writes are never cancelled.

**Shutdown.** `Shutdown(drainMs)` stops admission and drains every key. A key still Uncertain at the deadline, and the ops queued behind it, report **Unresolved**; the next process reconciles them from the marker and the journal.

## 3. Results (Core 21ff1973 DB layer, MySQL 8.0.46, async workers 1/2/4/8)

| # | Scenario | Result (every worker count unless noted) |
|---|---|---|
| K1 | Reverse hazard: an older autosave sleeps 1.5 s in the DB, then the newer purchase is submitted | Purchase waits for the older save. Final money 600, paid, marker 2. **Newest state kept at 1/2/4/8.** (The unkeyed baseline `run_harness.sh reverse 2` overwrites the confirmed purchase.) |
| K2 | Relog fence: the old session's logout save is slow, then the new session's login read is queued | Login read is **FRESH** (money 600 plus the new receipt) at 1/2/4/8 |
| K3 | Fence timeout | Login read not delivered within 300 ms, so login is refused. The queued read is cancelled (never delivered). A running read is cancelled and its late result discarded. The next read is delivered. |
| K4 | Definite failure (trigger) | NotCommitted after reconciliation; DB unchanged, marker 0. The next save is Committed with money 800 and marker 1. |
| K5 | Lost ack of a **committed** purchase | Reconciled to Committed with one reconciliation read. Exactly one transaction (one tail row), one journal row, **no replay** |
| K6 | Same purchase id replayed | Journal primary key rejects it, so the result is **DuplicatePurchase**. DB money 600 and one journal row (**charged once**). The replayed in-memory debit is undone and the next save converges. |
| K7 | Lease | Old epoch gets StaleEpoch, closed lease gets NoSession, unknown character gets NoSession |
| K8 | Parallelism | 4 keys × 1 s took 4016 / 2009 / 1006 / 1005 ms at 1/2/4/8 workers. One key with 2 × 1 s took ≈2010 ms at every count (strictly serial). |
| K9 | Coalescing and backpressure | With a per-key cap of 1 and a far-away global cap, a second queued op is refused. With caps of 2/2: 51 saves gave 2 queued plus 48 coalesced. The second read was refused, the per-key save was refused, a global-cap save on an idle key was refused, and a login was refused. After retry, **52/52 callbacks Committed**, 3 DB writes, final money 51. |
| K10 | Shutdown | Drained 4 keys × 2 ops: 8/8 Committed and every final state persisted. After that, submits get ShuttingDown and logins are refused. |
| K11 | Restart (two **processes**) | **Process 1:** a purchase's ack is lost while an external session holds the marker row and runs a further write (seq 2). `Shutdown(1.5 s)` can't reconcile, so the result is **Unresolved** and no access is granted. **Process 2:** the locking init waits 1.4 s for that transaction and sees seq 2. The journal shows the purchase committed, relog has access, and the next save is seq 3 with money 600. |
| K12 | Uncertain blocks (2/4/8 workers) | A lost ack while an external session holds the marker row (3 s). The queued save for that key does **not** run, and the DB stays at the first save. An independent key commits in 6 ms. Stall fires and new logins are refused. After release: first save Committed, then queued save Committed, final money 500, exactly 2 writes. |
| K13 | Exception while building a queued write | BuildFailed; the read and the save behind it still run (key advances) |
| K14 | **Server commits after the client gave up.** The dispatcher's own attempt fails; an external session holding the marker row commits seq 1 and money 700 after 2.5 s. | The locking read waits and reports **Committed**. The next save is seq 2, money 650. A plain read would report NotCommitted and reuse the sequence (mutant M3). |

Checks per run: 85 at 1 worker (K12 is skipped, see §5), 95 at 2 workers, 96 at 4 and 8 workers, plus 3 and 5 for the restart halves. **All pass.** Full log: `run_keyed_harness.sh` output.

## 4. Mutation check (each mutant must make the harness fail)
Tooling: `tests/db_harness/run_keyed_mutations.sh` applies each mutant in `tests/db_harness/keyed_mutants.py` to a copy of the header, rebuilds, and runs `keyed 4`. M4 instead runs `restart1` then `restart2`. **15/15 mutants are killed.**

| Mutant | What it breaks | Killed by |
|---|---|---|
| M2 | Advance the key on any `false` (rev1 behaviour, no reconciliation) | K5: a lost ack reports NotCommitted; K12, K14 |
| M3 | Plain (non-locking) reconciliation read | K12 (the queued save runs too early and its outcome is wrong); K14 (NotCommitted, sequence reused) |
| M4 | Plain (non-locking) init read after restart | K11b: seq after init is 1, not 2 |
| M6 | No epoch check | K7: an old epoch is accepted |
| M7 | No duplicate detection | K6: a replay reports NotCommitted |
| M8 | No shutdown drain | K10 |
| M9 | A cancelled running read is still delivered | K3 |
| M10 | A build exception does not advance the key | K13: the drain times out |
| M11 | No per-key depth cap | K9: per-key-only cap check |
| M12 | No global cap (writes, reads and logins) | K9: global-cap refusal |
| M13 | No coalescing | K9: counts and depth |
| M14 | Treat `false` as committed (auto-grant) | K4: a definite failure is reported Committed |
| M15 | No stall report | K12: login is not refused while Uncertain |
| M16 | Closed lease ignored | K7 |

The numbering skips M1, and M5 is not mutation-tested. M1 is per-key FIFO itself: its "mutant" is the unkeyed baseline, `run_harness.sh reverse 2`, where a confirmed purchase is overwritten. M5, marker-first placement, is covered by argument only (§5).

## 5. Limits and what this does NOT show
- **Layered, not in-pool.**
  - The prototype orders ops *before* they reach Core's queue: one op per key is in Core's queue at a time. The design puts the same state machine inside `DatabaseWorkerPool` under its mutex.
  - Thread-safety of that move, multi-key ops (trade, mail, auction house, guild bank), and the full writer inventory (§8.1, 70 statements) are **not** covered. They belong to the Core lane under CS01.
- **One worker.** A reconciliation read that waits on a lock occupies the only async worker, so all keyed and unkeyed async DB work stalls until the lock clears or times out. This is why K12 is skipped at 1 worker. With `WorkerThreads = 1` the interim mitigation still orders writes, but an Uncertain reconcile blocks the whole async queue.
- **The marker and journal need a schema change in production.** They are harness-only tables here. Adding them (or an equivalent per-character version column) to the real characters DB is an owner decision. No SQL is proposed for any real DB in this change.
- **Simulated ambiguity.** A lost ack is modelled by a test hook (K5, K11, K12) and by an external session that commits after the client's own attempt failed (K14). A real network loss during COMMIT wasn't injected.
  - The marker-first placement is argued, not mutation-tested: only a real in-flight transaction killed between statements would tell first from last apart.
- **Coalescing needs lazy builders.** It depends on whole-state builders running at dispatch time on the owner thread. Core's `SaveToDB` builds its transaction eagerly, so in Core coalescing would mean "drop the older pending whole-state transaction when a newer one is queued", which is only valid when the pending transaction carries no non-idempotent partial writes. The writer audit must classify that before coalescing is enabled.
- **Fixed timings.** Fence timeout 300 ms, stall 1 s, drain 1.5 s and lock wait 1 s are test values, not proposals.
