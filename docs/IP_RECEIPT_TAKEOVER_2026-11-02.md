# Paid expansion receipt takeover — strict ledger foundation (2 November 2026)

**Owner:** CosmicCuddle. **Temporary developer:** CS01 Maintenance while Claude/CS03-IP is unavailable, as explicitly directed by the owner. **State:** source-only first slice / DRAFT / default paid mode OFF. [Coordination Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13). This is *not a finished payment system*.

## Implemented now

Added `src/IndividualProgressionReceiptPolicy.h`, a standalone **strict RAW row decoder** plus versioned paid-era receipt policy. Added `tests/receipt_ledger_policy.cpp` and a small compiler matrix CI workflow. This deliberately does **not** wire into `PurchaseExpansion`, player login, entitlement cache, DB SQL or saved gold yet. No DB migration or owner action. Code does not activate with paid mode OFF or ON: it has not been integrated.

**Provisional ledger layout** in existing source `ip_paid_expansion`, `character_settings.data`, preserving old slot meanings: `0=TBC flag`, `1=Wrath flag`, `2=grandfather evaluated`, `3=nonzero presence/version (1)`, `4=TBC charged copper`, `5=TBC epoch`, `6=Wrath charged copper`, `7=Wrath epoch`. The two era flags allow historic `0=None`, `1=Paid`, `2=Grandfathered`; grandfather never fabricates a paid receipt. A *paid* flag requires positive, int32-safe charged amount and nonzero timestamp. No receipt is written for denied purchases, existing grant, duplicate charge or failed/refunded operation in the policy model. **Actual transaction wiring is still to be implemented and reviewed.** A free price is not supported by this provisional paid-receipt format; owner vendor prices must be verified before activation.

**Fail closed:** missing rows are `Missing`, not valid zero-cost entitlement. Incomplete/legacy 3-slot rows fail slot count. Unsupported version, malformed token, double/leading spaces, negative/plus/junk, extra tokens, overflow, malformed flag, inconsistent paid amount/time are not authorised. Decode uses `std::from_chars`, never indexes a permissively parsed cache. The parser preserves original slots 0–2. `Serialize()` uses the same canonical trailing space convention as pinned Core. Source/portable tests do not inspect or change real characters.

## Critical confirmed Core blocker — NOT solved by this header

Pinned Grimfeather Core `8daffdabe9202c228e45d833c9a1d399597a15cd` `PlayerSettings.cpp::ParseSettingsData` **drops invalid numeric tokens** and shifts later slots, while `Player::_LoadCharacterSettings` reads *every* row into the permissive cache and `_SavePlayerSettings` serialises that cache on the next save. Even if IP's own receipt parser refuses the invalid RAW data, **Core may still silently rewrite the malformed original bytes** when the character saves. The IP-only module cannot honestly guarantee `never mutate malformed row` without a separate protected Core parser/store solution or an approved read-only/archive/restore process. Do not enable paid mode or migrate data until that and `.pdump` omission of `character_settings` are resolved. A missing row could mean a new character OR a pdump of a previously paid one; do not silently seed new receipts to erase this ambiguity.

## Remaining owned development work

1. Design an owner-reviewable strict Core parser/preservation branch or prove an IP-only equivalent that *actually* preserves malformed rows on save. Do not silently alter the pinned Core.
2. Define an **approved bootstrap** for new/legacy/no-row characters without treating lost `.pdump` receipts as unpaid. No automatic guess or bill twice. Initial commissioning may need migration/backups and a Core transfer policy.
3. Wire raw-row validation before login entitlement cache and account-level effective progression; prevent cross-character Wrath grants from malformed data. Validate existing `FlagPaid` only with valid signed version + matching amount/time, while existing grandfather and start privilege rules stay intact.
4. Wire actual **in-memory** receipt fields into `PurchaseExpansion` before one `SaveToDB(false,false)` transaction with money and flags; rollback all changes on failure, same transaction, no `PlayerSettingsStore::UpdateSetting`; preserve TBC raw7 and Wrath effective13/no stage14.
5. Build native joined Worldserver and run isolated staging of first paid purchase, insufficient gold, duplicate, crash/restart, malformed data, character transfer, offline GM, quest10259 and rollback before requesting installation permission.

**Evidence boundary:** source helper and portable CI tests are not live account, native integration or transaction evidence. No live SQL, owner DB, host, charge, config change, merge or deployment was performed. Close/discard this child to return to IP #35 unchanged.
