# Paid TBC/Wrath: durable purchase acknowledgement (14 Nov 2026)

## Status and scope
- **What it is:** an isolated DRAFT on `feature/cs03-ip-paid-durable-purchase-20261114`, built from the synced IP `master` `05ff032b625f6e8abb6b2b5be496bb8a3ee65c96`.
- **Defaults:** the paid feature stays **OFF** (`IndividualProgression.PaidExpansion.Enable = 0`). The new commissioning gate is OFF too.
- **Evidence so far:** source contracts, native model tests, mutation testing and a compile/link against the synced stack. No DB failure test or gameplay test has been run.
- **Requests:** CS01 in Issue #13 comment 6884694721; the owner's objective in Coordination #61; the owner rules in Coordination #18.

## Inventory: historical paid work against current master
| Historical | Result |
|---|---|
| IP #33, #36, #37, #38, #39, #41, #42, #43, #46 | ancestors of master; fully present |
| IP #44 `ffb8070` (test-only) | not an ancestor, but its exact test lines are already in master |
| Runtime blobs | `IndividualProgressionExpansion.cpp` `3b5c8666`, `IndividualProgressionExpansionPolicy.h` `0801d578` and `IndividualProgressionReceiptPolicy.h` `9ef84c77` are byte-identical in #43, #46 and master; no other branch has unpromoted paid runtime commits |
| Core | synced Core `21ff1973` already preserves a malformed `ip_paid_expansion` row (skipped on load, read, write and save) |

**Conclusion:** nothing was lost, so this work is not a rebase. It closes the open release blockers B1–B4 below.

**Stale pins:** `tests/test_ip_paid_expansion_source.py` already fails 6 historical provenance and text checks on master, because later customisations changed `IndividualProgressionSpells.cpp`, the loader and `IndividualProgressionPlayer.cpp`. This branch leaves those 6 failures as they are.

## B1: durable acknowledgement (API v2)
Core's ordinary `Player::SaveToDB(bool, bool)` commits fire-and-forget. Before this change, `PurchaseExpansion` set the paid bit and returned `Success` before anything was durable.

### New sequence
1. **Validate.** `CanPurchaseExpansion` checks everything it did before. It also requires a session, and refuses with **`PurchasePending`** while an earlier purchase is unresolved.
2. **Reserve.** Under the IP lock, the purchase is re-checked and recorded as `Pending`, with a unique token and the session's identity. This happens before any debit.
3. **Debit and stage.** The gold is debited, and all eight receipt slots are written to the Player settings map and to the IP cache. The entitlement bit is **not** set.
4. **One transaction.** The code runs `SaveToDB(trans)`, then `session->AddTransactionCallback(CharacterDatabase.AsyncCommitTransaction(trans))`. This is the same Core pattern as `WorldSession::HandleTC9PrepareForRedirect`. The purchase returns **`Submitted`**.
5. **Callback `true` (same token):** the purchase becomes `Confirmed`, the entitlement bit is set (TBC unlocks the 10259 hand-in; Wrath opens effective stage 13), and the player sees a confirmation. For the same in-world character, `CheckAdjustments` and `checkIPPhasing(player, GetAreaId())` then run, as after any legitimate progression change (CS01 6885129881). They run only here, never at Submitted, Pending or Unconfirmed.
6. **Callback `false`:** the purchase becomes `Unconfirmed`. **There is no refund, retry, second charge or receipt reset.** The player is told not to buy again and that the purchase will be checked at the next login.

### Why gold and receipt can't diverge
Core's `_SaveCharacter` writes money, and `_SavePlayerSettings` re-appends **every** settings source on **every** save. So any transaction that persists the receipt also persists the debit, and the reverse.

- If a first commit fails and a later ordinary save succeeds, both values persist together.
- If every save fails, neither persists.

The next login reads DB truth through the strict decoder.

The purchase never writes quest status, so the quest dirty-list asymmetry from Coordination #18 doesn't apply to it.

### Edge cases
| Case | Behaviour |
|---|---|
| Duplicate click, or the other era, while Pending/Unconfirmed | `PurchasePending`; no debit |
| Far teleport or reconnect reload (`OnPlayerSendInitialPacketsBeforeAddToMap`) while unresolved | the staged cache is kept, because the DB row may not hold it yet |
| Reconnect (a different session) while Pending | the callback died with the old session, so the purchase becomes `Unconfirmed` |
| Logout | only the unresolved record is forgotten (the bits and the cached receipt are kept, as before). A late callback is ignored, and the next login reads the DB |
| Feature OFF at load | the record is erased along with the bits and the ledger |

### New API for Character Services
- **`GetPaidPurchaseState(player, era)`** returns `None`, `Pending`, `Unconfirmed` or `Active`. Only **`Active`** may be presented as "purchased". `Active` means the entitlement bit is set: either this purchase was confirmed, or the entitlement was loaded from a stored receipt (paid or grandfathered).
- **`COSMIC_IP_PAID_EXPANSION_API_VERSION`** is now `2`. `Submitted` and `PurchasePending` are appended to `PurchaseResult`, so existing values keep their numbers.
- `PurchaseExpansion` never returns `Success` now. An old CS build therefore can't print "purchased" for an unacknowledged write.

## B2: first-use commissioning (release gate)
- **Gate OFF (the default):** a character with no `ip_paid_expansion` row still can't buy (`EntryUnavailable`), as on master.
- **Gate ON (`IndividualProgression.PaidExpansion.CommissionMissingReceipt = 1`, and the store is saving):** a **truly absent** row (strict raw read `Missing`) gets the canonical empty v1 receipt (`0 0 0 1 0 0 0 0 `), staged at login and saved with the character.
- **Never commissioned or overwritten:** malformed, legacy, wrong-length, non-canonical, unknown-version and inconsistent rows. Core also keeps such a row out of every save.
- **Residual risk (owner gate):** after a very fast relog, the login read could run before the previous logout save commits. Commissioning would then stage an empty receipt over a just-made purchase. The window is the async commit latency. Owner approval and a DB test are needed before enabling the gate.

### Read-only receipt audit (before any commissioning decision)
Run all three exports, read-only, against the **same backup or replica** of the characters DB:

```
mysql -B --skip-column-names <copy> < tools/ip_paid_receipt_audit.sql     > receipts.tsv
mysql -B --skip-column-names <copy> < tools/ip_paid_receipt_anomalies.sql > anomalies.tsv
mysql -B --skip-column-names <copy> < tools/ip_paid_receipt_schema.sql    > schema.tsv
python3 tools/ip_paid_receipt_audit.py receipts.tsv --anomalies anomalies.tsv --schema schema.tsv --release --json report.json
```

- **Main query (`ip_paid_receipt_audit.sql`):**
  - one row per matching settings row, with **no `GROUP BY` or `DISTINCT`**, so duplicates can't collapse;
  - each row carries `HEX(source)` and `HEX(data)`, so case and trailing spaces survive export;
  - the highest marker comes from a subquery.
- **Anomalies (`ip_paid_receipt_anomalies.sql`, three `SELECT`s):**
  - **duplicate:** more than one matching row per character, grouped by the integer `guid` only;
  - **orphan:** a receipt row with no character;
  - **source_variant:** a row matched only through the case- and trailing-space-insensitive collation (`BINARY` compare).
  - Any of these rows **fails** the release audit.
- **Schema (`ip_paid_receipt_schema.sql`):**
  - `information_schema` reads of the `character_settings` primary key and the source/data collations;
  - the release audit requires the key to be exactly **`(guid, source)`**, as in Core's base schema (`utf8mb4_unicode_ci`).
- **Classifier (`ip_paid_receipt_audit.py`):** offline, with no DB connection. Categories match the strict loader:
  - **absent:** the only commissioning candidates;
  - **valid**;
  - **malformed, wrong_slot_count, unknown_version, inconsistent**;
  - **duplicate:** any character with more than one main row, never valid or commissionable;
  - **source_variant**.
  - `--release` requires the anomaly and schema reports and prints **two separate verdicts** (CS01 6886368231):
    - **`STRUCTURAL AUDIT: PASS/FAIL`:** no duplicate, orphan or variant rows, and the key is `(guid, source)`. FAIL exits **2**.
    - **`COMMISSIONING READINESS: READY/BLOCKED`:** every refused row (malformed, wrong slot count, unknown version, inconsistent) needs an explicit **owner disposition** in `--owner-dispositions` (`guid<TAB>text`; recorded only, **never repaired**). BLOCKED exits **3**. A disposition for a guid that isn't a refused row also blocks.
    - **A structural PASS alone is not permission to enable commissioning.**
- **Privacy:** reports stay local. The optional `--json` report is written only where you point it.
- **Parity and tests:** the Python port of `DecodeRaw` is checked against the **real C++ header** by `tests/test_ip_paid_receipt_audit_tool.py`, a differential test over about 30,000 strings covering all six statuses.
  - The same file covers fixtures (duplicates, including identical rows; collation variants; orphans; bad keys), the release exit codes, and read-only contracts for every statement.
  - Mutations A1–A7 and B1–B11 are all caught.
- **Out of scope:** account names are in the auth DB, so bot and excluded accounts are listed by account id.

### Injected persistence-failure harness (real Core DB layer)
`tests/db_harness/` (`paid_persistence_db_harness.cpp`, `harness_setup.sql`, `run_harness.sh`).

**What's real and what's modelled:**
- **Real:** Core `21ff1973`'s `DatabaseWorkerPool` (`CharacterDatabase`), `Transaction`/`TransactionWithResultTask`, `AsyncCommitTransaction`, `AsyncCallbackProcessor<TransactionCallback>` and the prepared `CHAR_REP_CHAR_SETTINGS`, all linked from the synced build's `libdatabase`/`libcommon`. Also real: the IP `IndividualProgressionReceiptPolicy.h` and `IndividualProgressionPaidPurchaseState.h`, and a **MySQL 8.0.46** server loaded with Core's base `db_characters` schema.
- **Modelled:** the Player object, and the parts of `Player::SaveToDB` the rule relies on: one transaction holding `characters.money`, then the settings `REPLACE`, then later statements.

**Failure injection:** `BEFORE UPDATE`/`BEFORE INSERT` triggers `SIGNAL` while `harness_ctl.fail_stage` is set. Stage 1 fails the first statement; stage 2 fails a statement **after** the receipt `REPLACE`.

**Results (14 Nov 2026; MySQL 8.0.46, throwaway local DB): 26/26.**

| Scenario | Result |
|---|---|
| S1 success | callback `true`; DB money 1000→600 and receipt `1 0 0 1 400 1800000000 0 0 ` together; a duplicate submit while pending was refused |
| S2 first commit fails, later ordinary save succeeds | callback `false` → Unconfirmed, no refund and no access; DB unchanged (1000, no row); a second purchase was refused; the later fire-and-forget save persisted money 600 **and** a paid receipt together; relog reads paid |
| S3 failure after the receipt statement | callback `false`; the `REPLACE` was **rolled back** (DB 1000, no row) |
| S4 both fail | DB unchanged. After relog a purchase succeeds, and the DB was charged **exactly once** (600) |
| S5 legacy `1 0 0 ` row | refused; the row is byte-for-byte unchanged |

**Release audit over the resulting DB:**
- `STRUCTURAL AUDIT: PASS` (key `guid,source`);
- `COMMISSIONING READINESS: BLOCKED (1 unresolved refused row(s) need an owner disposition)`, exit 3. That's the S5 legacy row.
- 3 valid paid, plus S3's rolled-back row (absent) and the S6 ordering character.

**Harness mutations:** D1 (receipt written outside the transaction) 19/26; D2 (refund on failure) 23/26; D3 (grant on failure) 25/26. All three are caught.

**Fast relog: stale read depends on `CharacterDatabase.WorkerThreads` (CS01 6886368231).**
- **How Core loads a character:** `HandlePlayerLoginOpcode` reuses an in-world Player on reconnect (no DB read; IP marks a pending purchase Unconfirmed). Otherwise it loads through `CharacterDatabase.DelayQueryHolder`, on the **same async queue** as the fire-and-forget logout `SaveToDB`. IP's strict receipt read is a sync query that runs after that holder completes.
- **Harness mode `ordering <threads>`:** a slow logout save (trigger `SLEEP(2)`) is queued, then the login read is queued immediately.

| `WorkerThreads` | Login read | IP sync read after it |
|---|---|---|
| **1** (Core's default) | **FRESH** (money 600, paid receipt) | **FRESH** |
| **2** | **STALE** (money 1000, no row) | **STALE** |

- With one async worker, the queue is FIFO, so the save commits before the login read runs. With two or more, the login can overtake an in-flight save and load the character as it was before that save.
- That's a general Core property, not paid-specific. For paid it matters most with **commissioning ON**: a stale "absent" read would stage an empty receipt that the next save writes over the real one.
- **New release prerequisite:** confirm the live `CharacterDatabase.WorkerThreads = 1` before enabling paid or commissioning. Otherwise an owner decision is needed.

**Ambiguous commit, from Core source:** `MySQLConnection::ExecuteTransactionAttempt` logs "Lost the connection during COMMIT. The transaction may or may not have been written" and returns an error, so the callback reports `false`.
- That's the Unconfirmed path: no refund, no retry, and the next login reads DB truth.
- A loss **before** COMMIT is rolled back by the server and retried up to 3 times.
- This harness doesn't inject a real connection loss.

**Environment note (scoped):** with Core `21ff1973`, `DatabaseIncompatibleVersion` parses the server version one character at a time. The MariaDB build I tried (10.11.14) was read as older than "8.0.0", and the pool refused to open. That's only this exact parser against that exact version; it's **not** a general compatibility statement. The harness ran on MySQL 8.0.46.

**Outstanding gates (not covered here):**
- the full `Player::SaveToDB` statement list inside a running world session;
- a real connection-loss failure;
- **the live `CharacterDatabase.WorkerThreads` value**;
- gameplay.

All results are developer-reported until someone runs them independently.

## B3: account bypass
- The purchase path is per-character: `CanPurchaseExpansion`, `PurchaseExpansion`, the callback, the entitlement and effective-stage APIs, and `GetPaidPurchaseState` read no account data. Contracts and a model test now pin this.
- `GetAccountEffectiveProgression` is unchanged. It is used only by the established upstream **Blood Elf/Draenei/Death Knight creation unlock** and the **TBC-race starting-zone teleport**. It never opens an era for an existing character.

## B4: tests
- **`tests/paid_purchase_durability_model.cpp`:** 1,786 checks, using the real policy, receipt and state headers to model Player memory plus the DB. Run under GCC 13 and Clang 18 × C++17/20 × asserts/NDEBUG. Cases:
  - TBC; Wrath; insufficient gold; exact single debit; duplicate clicks;
  - first save fails then a later save succeeds; both saves fail;
  - an exhaustive commit-ordering invariant (persisted gold ⇔ persisted receipt);
  - relog, teleport and reconnect while pending; stale tokens;
  - missing, malformed and legacy rows, with the gate OFF and ON;
  - 10259 sequencing (no stage write; the stage-8 grant is allowed only after confirmation);
  - bots, Altbots and excluded accounts are not applicable; no account bypass.
- **`tests/test_ip_paid_durable_purchase_source.py`:** 13 contracts.
- **Mutations:** M1–M17 are all caught.
- **Existing contracts updated minimally:** `test_ip_paid_expansion_source` (purchase sequence, logout hook, loader, cache store, API v2) and `test_f14_social_isolation_source` (landed-commit audit, identical to #183).

## Not done / release gates
- a real DB failure test (an injected commit failure on a staged realm);
- the Character Services v2 consumer (CS01);
- the owner's decision on commissioning;
- a backup and rollback plan;
- gameplay testing.

**Rollback:** keep both switches at 0, or revert the branch. No SQL, schema or data change.
