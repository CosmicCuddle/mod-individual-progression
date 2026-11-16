# Core database transaction-boundary review artifacts (review only)

Independent tests of the CS01 Core branches. Nothing here is built into the module.

| Item | Core commit |
|---|---|
| Baseline | `ad02036c` (database sources identical to `5d7819e7`) |
| Draft 1 | `d7c61592` (`feature/cs01-pb59-db-transaction-boundaries-20261116`) |
| Latest child | `fac74c8c` (`feature/cs01-pb59-db-reconnect-retry-20261116`) |

**Throwaway MySQL 8 only. Never point these at an owner, live or backup database.**
`run_all.sh` creates the disposable databases `tx_boundary` and `tx_pool`, using a local test-only user.
It also adds and removes an `/etc/hosts` entry `txdbhost` to simulate a DNS failure, so run it only in a disposable container.

## Files
- `err_proxy.py`: local MySQL protocol proxy (no TLS). On tagged transactions it does one of:
  - answers `START TRANSACTION` / `COMMIT` / `ROLLBACK` with a fake ERR 1180 while keeping the connection open;
  - fakes ERR 1213 on one statement;
  - drops a forwarded COMMIT's reply (the server committed);
  - closes its listener for 7 s, or for good;
  - removes the DNS entry for 7 s.
- `tx_boundary_test.cpp`: sync `MySQLConnection` matrix:
  - R1 COMMIT error + reuse,
  - R5 BEGIN error,
  - R8 ROLLBACK error + reuse,
  - prepared handle after reset,
  - A4 deadlock then lost COMMIT through `TransactionTask`.
- `tx_pool_test.cpp`: `DatabaseWorkerPool` typed API modes:
  - `direct`: `DirectCommitTransactionWithResult`: Committed / Empty / NotStarted / Ambiguous / FailedUnverified, plus the deadlock loop;
  - `async`: `AsyncCommitTransactionWithOutcome` with one worker, FIFO order, Ambiguous then reuse, NotEnqueued after `Close`;
  - `notready`: submission before `PrepareStatements`;
  - `drain`: `Close()` drains the queue;
  - `cancel`: pool destroyed without `Close` (queued tasks give CancelledBeforeExecution);
  - `outage refuse|dns|dead`: COMMIT error, then a reconnect outage. `dead` = exhaustion, fail-stop expected.
- `mklib_full.py`: rebuilds `libdatabase.a` from a Core worktree using an existing build's `compile_commands.json` (no PCH; optional sanitizer flags).
- `logs/`: outputs of the runs reported in Coordination #13. Scratch paths are replaced by `<scratch>`.

## Round 3 (native builds, module pool, observer)
- `tx_module_test.cpp`: `ModuleDatabasePool::DirectCommitTransactionWithResult` (Core `10b0ec9a`), using the statement shapes of `PlayerbotRepository::Save`:
  - normal save;
  - COMMIT error;
  - BEGIN error (no DELETE ran);
  - deadlock then lost COMMIT (exactly once);
  - recovery after the faults.
- `observer_race_test.cpp`: 4 threads standing in for map threads (reserve/seal/attach), racing a world-thread `Drain` on `PB59::CharacterSaveObserver<256>` (Core `28110cd`), under ASan/UBSan and TSan.
- `logs/build_*_tail.log`: worldserver build outcomes for
  - Core `28110cd` + PB `07d0180` + IP `4a604378` + CS `f43bc2c`;
  - Core `10b0ec9a` + PB `3d0f5529` (same IP/CS).

## Round 4 (N1 audit, N2 patch, more native builds)
- `tx_pool_test.cpp lateclose`: after `CharacterDatabase.Close()`, the legacy `CommitTransaction` is **silently not executed**, while the typed path returns **NotEnqueued**.
- `patches/n2_saturation_on_28df5c5.patch`, proposed for CS01:
  - `CharacterSaveObserver::Reserve` reclaims settled Committed/Empty slots when `Full` (bounded O(Capacity), non-blocking). Every other outcome stays for `Drain` to report.
  - On saturation, `SaveToDB` still builds bounded stack evidence and logs IP marker quests 66001–66018 immediately.
  - It compiled inside the full worldserver build.
- `observer_race_n2_test.cpp`: the stress test, with an assertion that every non-committed outcome is still reported.
