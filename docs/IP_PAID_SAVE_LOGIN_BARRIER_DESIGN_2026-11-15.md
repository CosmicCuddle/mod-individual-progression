# Paid receipt save/login ordering barrier: design for review (15 Nov 2026)

**Status:**
- **Design only.** Runtime is unchanged until design review (CS01, Issue #13 comment 6891956654).
- **Evidence:** Core `21ff1973` source plus the throwaway-DB harness in `tests/db_harness/`. All results are developer-reported.
- **Goal:** CS01 asked that receipts be preserved even with several `CharacterDatabase` async workers. `WorkerThreads = 1` is a mitigation, not a proof.

## 1. Facts (Core `21ff1973`)
| # | Fact | Source |
|---|---|---|
| F1 | `Player::SaveToDB(bool, bool)` builds one transaction (money, every settings source, everything else) and queues it fire-and-forget | `PlayerStorage.cpp:7238-7245`, `PlayerSettings.cpp:182-196` |
| F2 | `LogoutPlayer` queues that save (`SaveToDB(false, true)`) **before** it calls `OnPlayerLogout`, so IP's logout hook always runs with the save already queued | `WorldSession.cpp`, LogoutPlayer: save at +125, hook at +152 |
| F3 | A fresh login loads through `CharacterDatabase.DelayQueryHolder`, the **same async queue**. IP's strict receipt read is a **sync** query run later, from `OnPlayerSendInitialPacketsBeforeAddToMap` | `CharacterHandler.cpp` `HandlePlayerLoginOpcode`/`FromDB`; `IndividualProgressionExpansion.cpp` |
| F4 | A reconnect to an in-world character reuses the Player and reads no DB (#184 marks a pending purchase Unconfirmed) | `HandlePlayerLoginOpcode` offline-session branch |
| F5 | One async worker means FIFO order. With N > 1 workers, queued operations run concurrently, so ordering is lost | `DatabaseWorkerPool` / `DatabaseWorker::WorkerThread` |
| F6 | A failed statement rolls back the whole transaction. A connection lost before COMMIT is retried (up to 3 times). A connection lost **during** COMMIT is logged "may or may not have been written" and returns false | `MySQLConnection::ExecuteTransactionAttempt`, `Transaction.cpp` |
| F7 | Graceful shutdown drains the async queue before closing connections, so **no save is in flight across a restart** | `DatabaseWorkerPool::Close` (`_queue->Shutdown()`, then worker join) |
| F8 | No synchronous Core write touches `characters.money` or `character_settings`. The `DirectExecute` users are guild, group, instance and the cleaner | `grep CharacterDatabase.Direct*` |

## 2. Measured (harness `ordering <threads> <ipReadDelayMs>`, MySQL 8.0.46)
A slow logout save (trigger `SLEEP(2)`) is queued, then the login read is queued immediately, and IP's sync read follows after a delay.

| Workers | IP read delay | Core login read | IP sync read | Verdict |
|---|---|---|---|---|
| 1 | 0 ms | FRESH | FRESH | consistent |
| 1 | 2500 ms | FRESH | FRESH | consistent |
| 2 | 0 ms | STALE | STALE | consistent (whole character stale; a Core-wide issue) |
| 2 | 2500 ms | **STALE** | **FRESH** | **DIVERGENT** |

**Why the divergent case matters for paid:**
1. Core's memory has the pre-purchase gold (1000) and no receipt.
2. IP sees the paid receipt and sets the TBC bit.
3. The player hands in 10259 and earns stage 8.
4. Core's next save writes 1000 gold and no receipt over the committed purchase.
5. The result is stage 8, unpaid, and the gold kept: a **free expansion**.

The same divergence would also defeat commissioning, which could stage an empty receipt over a just-committed purchase.

**When the hazard exists:** only if a receipt write is **still in flight at relog**. That's when the purchase is Pending or Unconfirmed at logout (F2: its logout save is queued, not acknowledged). A purchase **confirmed** before logout is already committed, so no later read can miss it.

## 3. Options
| Option | Where | Covers | Cost / limits |
|---|---|---|---|
| **A. `CharacterDatabase.WorkerThreads = 1`** | owner config (read-only verification) | all queued-save/login ordering (FIFO) | depends on config. CS01: necessary but not proof |
| **B. IP in-flight receipt barrier (recommended)** | IP only, process-local | the exact paid hazard, for any worker count | a short per-character "being confirmed" window after an unconfirmed-purchase logout |
| C. Core per-character save/login barrier | Core lane | **all** character data (Core's own stale-load risk with N > 1) | Core change: track an in-flight logout save per GUID via `AsyncCommitTransaction`, and defer or refuse `HandlePlayerLoginOpcode` until it's acknowledged |
| D. Core read-only settings accessor | Core lane | lets IP check that Core's loaded snapshot equals IP's read | a small Core API; doesn't fix Core-wide staleness |

## 4. Proposed B: IP in-flight receipt barrier
- **State:** `_paidReceiptInFlight`, keyed by low GUID → `{ era, loggedOutAtMs }`. Process memory, under the paid lock.
- **Enter:** in `OnPlayerLogout`, if the purchase record is `Pending` or `Unconfirmed`. This **replaces** "forget": the receipt is inside the queued logout save (F2) and isn't acknowledged.
- **At login** (`LoadPaidExpansionEntitlement`), if an entry exists and `now − loggedOutAtMs < IndividualProgression.PaidExpansion.InFlightBarrierSeconds` (proposed default **120**):
  - load every other fact from DB truth as today (a purchase confirmed earlier was committed before this logout);
  - **for the in-flight era only:** keep the entitlement bit **OFF** for this session, and refuse purchases (`PurchasePending`);
  - never commission;
  - send the message "Your purchase is still being confirmed; log in again shortly."
  - Neither IP's sync read nor Core's possibly stale holder can then grant that era, so the divergent case can't produce access.
- **After the window:** erase the entry and load from DB truth as normal. By F7 there's nothing in flight across a restart; with a healthy pool the queue drains in milliseconds.
- **Ambiguous commit (F6):** it lands here as Unconfirmed, so it's covered.
- **Reconnect (F4):** no DB read is involved; the existing CallbackLost → Unconfirmed handling applies.

**Why every end state stays consistent:** every transaction carries its own gold+receipt pair (F1). Whichever save commits last, the DB holds a consistent pair; at worst that's "not bought, gold kept". The barrier only prevents granting access from a snapshot that Core's memory doesn't share.

**Residual risk:** if the async queue backlog exceeds the window (a DB outage), a divergent read is possible after it.
- **Mitigation:** make the window configurable, and log a warning if `CharacterDatabase.QueueSize() > 0` when the window expires.
- **Recommendation:** keep A as a release prerequisite as well. B makes correctness independent of A in normal operation; C is the general fix if the owner ever wants N > 1.

## 5. Tests planned after design approval
- **Pure barrier model:**
  - enter only for Pending/Unconfirmed;
  - the window boundary;
  - other eras unaffected;
  - no commissioning during the window;
  - erase after it.
- **Harness:**
  - logout with the purchase in flight;
  - relog at 2 workers with the divergent timing;
  - assert **no TBC bit, purchase refused, no commissioning**;
  - after the window, DB truth.
- **Contracts:**
  - the logout hook moves Pending/Unconfirmed into the barrier;
  - the loader checks the barrier before granting the era;
  - the barrier never refunds or writes.
- **Mutations.**

No runtime change, owner DB or config change, merge or deploy has been made.
