# CosmicCuddle F14 — unified RandomBot identity publication for PLAYER and WHO (8 Nov 2026)

**Owner:** CosmicCuddle. **Review-only IP120 child:** `38d6844af45eb1d89679299c9a8d466ae64375d5`. No code or config activated, no host/DB/client work, no new runtime callback. This is an *interface/policy consistency model*, not an executable concealment feature or owner-approved permission.

## Why this is necessary

The owner Core now has independently prepared **default-OFF** per-viewer client actor ([Core #6](https://github.com/CosmicCuddle/azerothcore-wotlk/pull/6)), cached `/who` GUID ([Core #5](https://github.com/CosmicCuddle/azerothcore-wotlk/pull/5)) and creature ([Core #4](https://github.com/CosmicCuddle/azerothcore-wotlk/pull/4)) queries, all returning FALSE without a future IP script. The older IP120 experimental `PlayerScript` observer and WHO method use its **read-only GUID + process-local login epoch cache**, but were built for different optional Core version macros and are NOT automatically the same hook as Core6's new `WorldObjectScript` APIs.

Core6's player actor pre-query checks target is headless race10/11, which **also potentially describes Altbots**. WHO offers only target GUID and cached race, not target Player*, login epoch or live AI. A positive script must *not* assume that all race10/11 targets or a stale GUID are headless RandomBots.

This review-only pure C++ header models a **single revocable publication** from a future audited IP/Playerbots login producer. It requires current positive RNDBOT origin, exclusion of human/Altbot, exact full GUID + nonzero monotonically new session epoch + account + race and active non-retired lease, and a normal human viewer with current nonzero epoch and verified **effective** Vanilla stage0–7. For live Player actor it also cross-checks current session GUID/epoch/account/race; for WHO it only compares cached full GUID/race against a *current* producer lease because WHO cannot dereference the target. When a logout/reuse/identity/era transition makes publication stale, producer must revoke atomically before any query may return positive; this producer/consumer invariant is **not yet implemented**. No false certainty from the old GUID-only cache.

## Strict limitations

- Synthetic positive model results are **not** authorization to hide anyone: `AuthorizesRuntimeActorOrWhoHiding()`, `AuthorizesNpcHidingOrRightsDenial()`, `AuthorizesStage8OrPaidEntitlement()` all explicitly return FALSE. No actual script, database receipt, source-to-map proof, client ACK, event refresh or previously-hidden restoration is added.
- Existing `IndividualProgression.cpp`, `IndividualProgressionPlayer.cpp`, Character Services, PB9/PB10, the original IP120 WHO observer, stage/floor/group/paid logic, and frozen Claude #94/#103/#104 stay **byte-identical**.
- This only addresses **WHO versus actual player actor parity**. NPC combat-only temporary concealment is independently blocked on positive current actor rights, observer human quest/tap/loot/pet/DoT/assist, owning map, corpse/death/respawn/map/era restoration. Never infer NPC permission from an actor or WHO query.
- The pure native C++17/C++20 GCC/Clang test covers all 128 combinations of seven producer proofs plus stale epoch/GUID/account, logout, human/Altbot, GM, TBC/Wrath and stage-change cases. It proves only synthetic consistency; no actual PB login, database readback, runtime race freedom, native Worldserver ABI or gameplay.
- Keep all F14, paid, WHO, creature and player query macros **OFF**, and never deploy without CosmicCuddle's explicit review and a tested offline full-tuple backup/restore. Protect all tracked/untracked/**ignored custom SQL cleanup/migration files** and actual owner installed modules/config/DB/DBC/client/binaries.

**Rollback:** discard this separate IP-only test/docs draft; no SQL or source runtime migration.
