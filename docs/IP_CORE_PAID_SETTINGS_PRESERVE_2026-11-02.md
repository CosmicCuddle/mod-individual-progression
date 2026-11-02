# Pinned Grimfeather Core paid receipt no-launder patch — disposable CI only

**Owner:** CosmicCuddle. **Date:** 2 November 2026. **Source Core pin:** `8daffdabe9202c228e45d833c9a1d399597a15cd`; exact `PlayerSettings.cpp` blob `f1eb86367ce69111867ef7187125f5fe456c5367`. **Status:** draft, OFF, no Core fork/installation, no SQL/production/character data writes. Created in response to Grok [HIGH-1](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6575653080).

## The actual loss/counterfeit bug

Core's `PlayerSettingsStore::ParseSettingsData` silently skips malformed numeric tokens. `Player::_LoadCharacterSettings` caches the shifted vector; `_SavePlayerSettings` then serialises **all** rows, turning an initially invalid raw `ip_paid_expansion` row into potentially valid paid v1. An IP-only strict `DecodeRaw` refuses it **on first login**, but it can be paid **on the next login** after Core autosave. Do not use a pretend-fail-closed parser alone.

## Patch (scoped, not deployed)

One guarded Core file patch `tests/core_patches/preserve_paid_receipt_row.patch` adds `IsCanonicalPaidReceiptRow(source,data)` to `PlayerSettingsStore`. For **every other settings source** it returns true and leaves Core's original behaviour intact. For `ip_paid_expansion`, it accepts only exactly Core's numeric token serialization (including canonical single trailing space) and a nonempty parsed vector; malformed/misaligned/noncanonical raw rows are **not inserted into the player's settings map at login**, therefore ordinary `_SavePlayerSettings` never rewrites them. Likewise the standalone offline `PlayerSettingsStore::UpdateSetting` refuses malformed rows **before REPLACE**. It logs only the character counter, never the raw receipt payload. It does **not** erase the row, set it to zero, grant entitlement, write a receipt, or alter SQL. Original invalid raw data remains in the DB for owner-approved manual investigation/restoration.

## Why separate

The connected GitHub repositories do **not** include an owner-controlled AzerothCore base fork; changing Grimfeather upstream or silently dropping this patch into `Mod-Naxxramas-Core` (a module, not a Core source fork) would be inappropriate. This PR stores a narrowly scoped reviewable patch **in the IP source repository**, applied **only on a throwaway CI Core checkout**. This is a preservation plan for a future owner-approved Core fork/build, not deployed code.

## Known residuals and acceptance gates

- If a different custom module explicitly calls `Player::UpdatePlayerSetting(\"ip_paid_expansion\",...)` on a malformed-row character, that in-memory write can still produce a new DB value. IP #37 itself never calls the Core permissive setting API on invalid/missing paid ledger, and purchases/grandfathering are blocked. Cross-module source audit and native staging needed.
- Rows containing valid canonical numbers but invalid **receipt semantics** (wrong version, wrong amount/time or extra count) are still parsed/serialized; their serialized value stays canonical and does not become a valid receipt by token shifting. IP #37 rejects them at entitlement and purchase checks.
- Core's older `UpdateSetting` for a **missing** `ip_paid_expansion` row can still create a partial settings vector. That is not a paid receipt; missing-row commissioning remains an owner-approved task, and other direct writers should be audited.
- Exact Core SHA/blob guard, git patch application, negative-model source tests and native CI must pass. The model is **not a live Core/DB/Worldserver demonstration**. Standalone/real-game login, Core save/logout/relogin, valid v1 purchase, malformed originals, zero/noncanonical rows, PB owner alts, transfer `.pdump` and rollback need staging.
- Must preserve Feature OFF, unrelated settings sources, custom player settings and existing realm data. Do not merge this Core patch into an arbitrary Core branch or apply to owner host without explicit CosmicCuddle approval and restorable backups.

**Rollback:** discard isolated PR; for a future staged Core application reverse only this pinned patch, restore snapshots/backups as approved. No change to live Core/SQL/host has occurred.


## Grok test-strength follow-up (2 Nov 2026)

Grok's [#37 L2/L4 findings](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6575653080) identified a version-mutant that bypassed the original helper tests and a C++ test driver that treated every check as a no-op under `-DNDEBUG`. The isolated #38 child now adds **direct** `ValidLedger` version-zero refusal and denies `RecordPurchase` on that invalid copy, replaces `assert()`-only test calls with an always-executing exit-on-failure helper, and extends the portable C++ CI matrix to g++/clang++ C++17/C++20 with and without `-DNDEBUG`. These are **test-only** changes, no production receipt arithmetic or Core patch change. Exact-head CI must be checked; no native/DB/gameplay approval is implied.


## Stronger compiled Core unit harness (2 November 2026)

The existing Python counterexample model is supplemented by `tests/core_patches/core_settings_harness.cpp` and tiny fake `Player.h`/Core helpers. The CI job first applies the exact pinned patch to Core's actual `PlayerSettings.cpp`; copies that **actual patched Core translation unit** into an isolated temporary directory (so the fake `Player.h` is selected), compiles it with GCC and Clang under C++17/C++20 `-Werror`, and runs first load → ordinary save → re-login → ordinary save → offline `PlayerSettingsStore::UpdateSetting` for seven synthetic malformed rows. Every original raw byte must remain identical. It additionally checks valid paid and grandfather source rows, and that unrelated Core settings retain their old permissive parser behaviour. The fake DB has no MySQL connection, real character identities, gold or realm data. This improves source behaviour evidence but is **not** Worldserver compilation, real MySQL persistence, gameplay or host acceptance.


## Exact Core/IP canonical format agreement (2 November 2026)

A compatibility edge was caught during source review: the first strict ledger decoder accepted valid numeric v1 records without Core's trailing space (or with leading-zero spelling), but the Core no-launder patch intentionally skips *non-canonical* paid rows on login. That mismatch risks other in-memory `GetPlayerSetting` calls creating an empty vector and later overwriting an otherwise valid user-supplied raw row. This follow-on code tightens **IP `DecodeRaw` to accept exactly the same canonical 8-slot, single-trailing-space, decimal text as Core `SerializeSettingsData`**, retaining semantic version/cost/time checks. It rejects no-final-space or leading-zero rows as `Malformed` rather than silently rewriting them; a future owner-approved migration can deliberately convert such records with backup/restore. No automatic correction, player DB read, or gold/entitlement write. Added negative portable tests. The Core patch remains byte-identical and all live settings remain OFF. Exact-head source/CI and real combined native link still need verification.
