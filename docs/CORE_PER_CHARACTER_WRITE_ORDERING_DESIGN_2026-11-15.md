# Core per-character write ordering and login fence: design for review (15 Nov 2026)

**Revision 2 (CS01 6892952406):** keyed FIFO direction **accepted, not authorized**. Phase 1 may only be an isolated prototype. Implementation would sit in a separate Core lane under CS01; CS03-IP stays IP owner and supplies the harness and adapter. No Core runtime edits until the owner's final approval. See §8 for the amendments.

**Status:**
- **Design only (option C).** It requested by CS01 in Issue #13 comments 6892637976 and 6892656707, and supersedes the timer-based option B in `IP_PAID_SAVE_LOGIN_BARRIER_DESIGN_2026-11-15.md`.
- **No Core runtime change** until review and owner approval of a new Core feature.
- **Evidence:** developer-reported, from Core `21ff1973` source and the throwaway-DB harness (`tests/db_harness/`).

## 1. Problem
A character's state is persisted by **whole-state transactions** (`Player::SaveToDB`) and some partial ones. They are queued on `CharacterDatabase`'s async pool. With `WorkerThreads > 1`, nothing orders two transactions for the **same** character:

| Hazard | Mechanism | Evidence |
|---|---|---|
| H1 stale login | the login `DelayQueryHolder` runs before an in-flight logout save | harness `ordering 2 0`: STALE |
| H2 divergent snapshot | Core's holder is stale while IP's later **sync** receipt read is fresh | harness `ordering 2 2500`: DIVERGENT |
| H3 reversed completion | an **older** save, delayed before its first statement (worker scheduling, or a deadlock retry, which `TransactionTask` performs), commits after a **newer** save and overwrites it | harness `reverse 2`, below |
| H4 ambiguous commit | "lost connection during COMMIT" is unknowable; a later save's ordering relative to it is undefined | `MySQLConnection::ExecuteTransactionAttempt` |

InnoDB row locks serialize two transactions only **after** both reach the character row. They don't impose submission order, so a waiting, completion-based login fence alone can't fix H3. **Writes themselves must be ordered per character.**

## 2. Design: keyed serialization in the database layer
Each character write or read operation carries a **serialization key**: the character's low GUID, or the set of GUIDs for a multi-character transaction.

The pool guarantees:
1. **Per-key FIFO:** operations with a common key **start** in submission order. An operation starts only when every earlier operation sharing any of its keys has **finished** (committed, failed or given up).
2. **Independent keys run in parallel** across all workers.
3. **Unkeyed operations** behave exactly as today.

Because every full save carries the **complete** newest in-memory state:
- a failed or ambiguous earlier transaction is followed by a later one that writes newer state, so the DB converges to the newest submitted state (fixes H3 and makes H4 benign);
- a keyed login read starts only after every earlier write for that character has finished (fixes H1);
- IP's sync read happens after that holder completes, before the new session submits any write, so it sees the same state (fixes H2). See §4 for the guard.

No game thread waits. Ordering is enforced inside the pool, and callbacks are unchanged.

### Per-key state machine (inside `DatabaseWorkerPool`)
```
Idle --enqueue(op)--> Running(op)
Running(op) --enqueue(op2)--> Running(op) [op2 appended to the key queue]
Running(op) --op finished (success | failure | ambiguous | deadlock-retries exhausted)--> queue empty ? Idle : Running(next)
```
- **Multi-key op:** admitted when it's at the head of **all** its key queues. Admission is in global submission order, so there is no deadlock.
- **Failure or ambiguous outcome:** reported to the op's own callback as today. The key advances regardless, and the next op holds newer whole state.
- **Shutdown:** `Close()` stops admitting new keyed ops after the world stops, then **drains** all key queues and the main queue before closing connections. It extends today's drain (F7).

## 3. Core seams (file by file; proposals, not changes)
| File | Change |
|---|---|
| `database/Database/SQLOperation.h` | add `std::vector<uint32> serializationKeys` (empty means unkeyed) |
| `database/Database/DatabaseWorkerPool.h/.cpp` | keyed dispatcher: `std::mutex`, `unordered_map<uint32, deque<SQLOperation*>>`, an in-flight set; `EnqueueKeyed`; keyed overloads `CommitTransaction(trans, keys)`, `AsyncCommitTransaction(trans, keys)`, `AsyncQuery(sql, keys)`, `DelayQueryHolder(holder, keys)`; `IsKeyBusy(key)` (diagnostics); drain in `Close()` |
| `database/Database/DatabaseWorker.cpp` | after `operation->call()`, call `pool->OnKeyedOperationFinished(op)` to admit the next op (no lock held during `call()`) |
| `game/Entities/Player/PlayerStorage.cpp` | `SaveToDB(bool,bool)` commits keyed by `GetGUID().GetCounter()`; additional/partial saves (`SaveInventoryAndGoldToDB`, `SaveGoldToDB`, `_SaveQuestStatus` via additional saving) keyed likewise |
| `game/Handlers/CharacterHandler.cpp` | the login `DelayQueryHolder(holder, {guid})`; character delete, rename and customize transactions keyed |
| `game/Server/WorldSession.cpp` | `LogoutPlayer` save is keyed through `SaveToDB`; `HandleTC9PrepareForRedirect` keyed |
| `game/Handlers/TradeHandler.cpp` | trade's two-character transaction keyed `{a, b}` |
| `game/Mails/Mail.cpp`, `game/AuctionHouse/*` | writes into another character's mail or items keyed by the **receiver** GUID |
| `game/Tools/CharacterDatabaseCleaner.cpp`, `Guild*`/`Group*` `DirectExecute` | startup or unrelated tables; unchanged (verified: none touch `characters.money` or `character_settings`) |
| IP (module) | `PurchaseExpansion` uses `AsyncCommitTransaction(trans, {guid})`; `LoadPaidExpansionEntitlement` asserts `!IsKeyBusy(guid)` and treats busy as **Unknown** for that session (fail safe, never grants) |

### Login fence semantics
- The login holder is keyed, so it simply starts after the character's earlier writes.
- **Backlog wait:** while a backlog is waiting, the session sits in its normal loading state. **No game thread blocks.**
- **Timeout** (`CharacterDatabase.LoginFenceTimeoutMs`): it may only **refuse** the login ("character is still being saved, try again"). It never grants access and never lets a stale holder through.
  - If the holder later completes after a refusal, the callback discards it.
- **Reconnect** to an in-world character: unchanged. No DB read; IP's purchase record handling applies.

## 4. Interaction with IP paid purchases (#184)
- The purchase transaction is keyed, so it's ordered with that character's autosaves and logout save. An older autosave can no longer overwrite a newer confirmed purchase (H3).
- The commit callback now means "this and every earlier write for the character are finished", so `Confirmed` is durable in order.
- The design-B timer barrier is **not needed** and is withdrawn.
- `WorkerThreads = 1` stays a mitigation for builds without this Core feature. IP keeps the fail-safe `IsKeyBusy` check, and a compile-time detection of the keyed API (`#if COSMIC_DB_KEYED_ORDERING_VERSION`).

## 5. Tests (proposed for the Core change; the harness extends the existing one)
1. **Reversed completion:** older save delayed before its first statement, newer purchase fast, workers ∈ {1, 2, 4, 8}. Expect: final DB = newest, and the confirmed purchase is present.
2. **Same-GUID relog:** slow logout save, then login holder and IP sync read. Expect FRESH and no divergence at every worker count.
3. **Backlog over 120 s:** trigger `SLEEP(125)` on a logout save. The login waits; the timeout refuses and never grants; a late holder is discarded.
4. **Failed transaction:** the next keyed op proceeds and the DB converges to the newest state.
5. **Ambiguous commit:** kill the connection during COMMIT with an external `KILL QUERY` at a barrier. Either outcome is followed by the newer save, so the DB converges.
6. **Independent GUIDs:** N slow saves for N GUIDs complete in about one delay (parallel), not N times it.
7. **Multi-key:** a trade `{a, b}` is ordered against both a's and b's saves, with no deadlock under a randomized interleaving stress test.
8. **Shutdown:** `Close()` with pending keyed ops drains all of them.
9. **TSan** on the dispatcher, with randomized enqueue and finish.

## 6. Evidence today: harness `reverse` mode (this branch)
An older autosave (pre-purchase state, `DO SLEEP(1.5)` before its first statement) is queued, then the newer purchase is committed through `AsyncCommitTransaction` and confirmed. Run on MySQL 8.0.46 with the real Core pool:

| Workers | Purchase callback | Final DB | Result |
|---|---|---|---|
| 1 | `true` | money 600, paid receipt | newest state kept (FIFO) |
| **2** | **`true`** | **money 1000, no receipt** | **confirmed purchase overwritten by the older save** |

- With more than one worker, the durable-commit callback is **not** proof of durability-in-order.
- Every committed transaction is still internally consistent (gold and receipt together), so the player isn't charged without a receipt. But a confirmed purchase is silently undone in the DB while the player's memory still holds it, until the next save rewrites it.
- A crash in that window loses the purchase and keeps the gold.
- This is exactly H3, and it's why §2 orders the writes rather than only fencing the login.
- **Limits of the model:** the delay stands in for worker scheduling or a deadlock retry; Core's real `SaveToDB` statement list wasn't used.

## 7. Open decisions
- whether the owner wants this Core feature at all, or wants to keep `WorkerThreads = 1` as the documented, enforced configuration;
- the default for the login fence timeout;
- whether to key every partial-save path in one change, or start with `SaveToDB`, login, logout and purchase, and audit the rest.

## 8. Amendments (revision 2, CS01 6892952406)
1. **Release gate: every writer.** Paid can't release while any unkeyed writer touches money, settings or inventory. The inventory is in `CORE_WRITE_ORDERING_WRITER_AUDIT_2026-11-15.md` (70 prepared statements in 27 files, plus raw SQL). Each needs a key, an explicit "startup only, before admission" classification, or a documented exclusion. Multi-character writers (trade, mail, auction house, guild bank, GM commands on others) need key sets.
2. **Login fence through session admission:** `IsKeyBusy` is **dropped** (it's time-of-check/time-of-use). Instead:
   - **Admission lease:** login takes a per-character **lease** with a session epoch. While a lease is held, writes from an older session epoch for that GUID are rejected or quarantined, and only the leased session's writes are admitted after the holder.
   - **One ordered snapshot:** the IP receipt is loaded **inside the same keyed login holder** (an added holder query), so Core's state and IP's receipt come from one ordered snapshot. There's **no second independent sync read**. IP's `LoadPaidExpansionEntitlement` consumes the holder result.
3. **Ambiguous COMMIT is not benign:**
   - A "lost during COMMIT" outcome puts the key into **Uncertain**. The key does **not** advance until reconciliation proves the server-side outcome: wait for the server to finish the transaction, verify the connection, then read back a per-transaction marker row or version written inside the transaction.
   - While a key is Uncertain, every later op for it waits. On timeout, login is refused and IP fails closed (Unknown).
   - **No automatic replay or grant.**
4. **Dispatcher correctness:**
   - capture the keys (by value) **before** `operation->call()`, because the op is deleted after it;
   - exceptions inside `call()` still release the key (RAII);
   - cancellation of a queued keyed op (session gone) still advances the key;
   - callbacks complete on their processors as today;
   - **shutdown** drains the keyed queues **before** the main queue shuts down, then closes connections;
   - **bounded queues and backpressure:** a per-key depth cap and a global keyed-op cap, with a warning plus refusal of new logins when exceeded (never dropping writes).
5. **Login fence timeout:** refuse only. It's configurable, and **120 s is proposed for testing only**, not as a production approval.
6. **`WorkerThreads = 1`:** a **documented interim mitigation**. It needs a read-only verification of the live value; the owner hasn't approved enforcing or changing config.
7. **Lane:** Core implementation in a separate Core lane under CS01 (Codex only after its current PB tasks, or by separate assignment). CS03-IP supplies the harness extensions (writer, cancellation, uncertain-commit, backlog, admission-lease scenarios) and the IP adapter (holder-sourced receipt, keyed purchase commit).

**Revised tests** (in addition to §5):
- a stale-epoch write rejected after relog;
- the receipt from the holder equals Core's loaded settings;
- an Uncertain key blocks later ops until reconciliation, and a timeout refuses login;
- an exception or cancellation inside a keyed op still advances its key;
- backpressure caps are hit without dropping writes;
- shutdown with a mid-reconciliation key.

## 9. Isolated prototype (15 Nov 2026, CS01 Part09 decision 2)
`CORE_WRITE_ORDERING_PROTOTYPE_2026-11-15.md` covers the prototype and its results. It is a per-character keyed dispatcher layered above the real `DatabaseWorkerPool` and tested on a throwaway MySQL 8.0.46 at 1/2/4/8 async workers. Scenarios K1–K14 cover:
- the reverse hazard;
- the relog fence and fence timeout with cancellation;
- definite failure, lost acknowledgement, and a late server-side commit, all reconciled by a locking marker read;
- duplicate purchase id;
- lease epochs;
- parallelism;
- coalescing, per-key and global backpressure;
- shutdown drain;
- restart reconciliation across two processes;
- Uncertain blocking;
- build exceptions.

All checks pass, and a mutation run kills every mutant listed there. No Core runtime change is made. The marker and journal tables exist only in the harness DB, and adding them to a real DB is an open owner decision.

**Revision 2 of the prototype** (CS01 review 6894208993) adds:
- a fenced compare-and-set marker, an exact per-op write log and a guid-bound journal;
- reconciliation by claim;
- callbacks that run before the next dispatch;
- capped, same-session-only coalescing;
- a real COMMIT network-loss test.

See §6 of the prototype doc.
