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
