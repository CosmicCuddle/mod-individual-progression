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
