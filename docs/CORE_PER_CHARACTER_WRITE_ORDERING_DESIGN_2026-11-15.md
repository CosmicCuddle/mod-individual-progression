# Core per-character write ordering and login fence: design for review (15 Nov 2026)

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
