# F14 viewer rights + restoration joint safety gate (PURE, OFF)

**Owner:** CosmicCuddle
**Developer:** CS01 Maintenance during Claude's owner-confirmed break
**Date:** 5 November 2026
**Status:** Isolated DRAFT, no runtime consumer, no positive concealment, no server/client/DB changes.

## Actual implementation

New `src/IndividualProgressionF14ViewerRightsGate.h` **composes two already-present source models** in the #97 combined IP source tree:
- `IndividualProgressionF14CombatVisibilityPolicy.h` — conservative creature + viewer pair safety facts, including current attack, alive, full participant/rights, group quest/loot/tap/pet/assist/spells and encounter exclusions.
- Claude's original byte-for-byte `IndividualProgressionF14ViewerRestoreProtocol.h` / `IndividualProgressionF14PriorHiddenLedger.h` — owner-session and creature incarnation proof with frozen prior-hidden restore obligations.

`CheckHypotheticalSuppressionOnly` supplies a testable fail-visible gate. A model candidate requires: nonzero exact viewer/creature/incarnation keys; confirmed owning map thread; matching map+instance; separately verified selected Blood Elf/Draenei headless RNDBOT actor and creature incarnation; **independent authoritative complete human pair-rights proof**; conservative combat/observer model approval; and no outstanding restoration ledger obligations. All independent authority flags default false. `pairRightsProven` from an untrusted `DecisionFacts` input is **ignored**; the gate sets it true **only after** the full independent model checks. It never modifies the protocol/ledger.

**A synthetic positive result is named `HypotheticalModelOnly` intentionally.** It is NOT evidence that any live Core can supply the facts, and is NOT permission to hide. `IsLiveCreatureConcealmentAuthorized()` always returns `false`. No runtime hook registers the gate and the headers are not included by Worldserver gameplay sources.

## Negative tests

The new original-source C++ test verifies:
- Missing/invalid viewer or creature identity, wrong owning map, human/Altbot/unverified actor, relogged viewer, recycled incarnation.
- Fake prefilled `pairRightsProven=true` cannot bypass absent real-player rights authority.
- Quest/loot/kill/tap/group, owned-pet/DoT/assist/threat, in-flight spells, current attacker stop, death, pending threat and changed creature identity/revision all fail visible.
- An old F14-only prior-hidden record followed by corpse/stop invalidation blocks future hypothetical concealment; no ledger completion/ACK is supplied.
- Even a fully synthetic positive model cannot activate a live hide.

The Python source contracts verify exactly six new/append-only branch paths, original Claude #94 blobs unchanged, existing #92/97 stagefloor and actual lifecycle source untouched, config/SQL/DBC/client/host unchanged and all paid/F14 options OFF.

## What blocks a working feature

**The missing source authority is concrete:** existing `IndividualProgressionF14LifecycleScript.cpp::CollectSnapshot` enumerates threat, PvE combat, attackers and tap, but explicitly says it does not establish complete player/group quest/loot/pet/DoT/assist/kill-credit rights or every attacker/owner lifecycle. There is no positive source for the gate's full-rights flag. No Core/3.3.5a client object ACK is available; source-attributed #93/#95 send attempts are not client restoration. Positive concealment and ledger clearance must remain disabled.

Before any gameplay activation, prove the actual installed Core/Playerbots/IP/Services/Naxx revisions, supply complete map-thread pair-rights policy, fix safe viewer-specific restoration across death/logout/relog/maps/overflow, test real 3.3.5a client targetability and credit, back up host/DB, prepare rollback and obtain CosmicCuddle's explicit production approval.

## Source, tests, rollback

Branch child of [DRAFT #97](https://github.com/CosmicCuddle/mod-individual-progression/pull/97) at `d8aa20cb4f59df7caceab7842247f38d003867d5`; Claude's separate original [#94](https://github.com/CosmicCuddle/mod-individual-progression/pull/94) was not edited. C++ GCC/Clang × C++17/20 × NDEBUG/non-NDEBUG tests and source guards run in GitHub Actions. **Exact-head results must be checked before claiming passed.**

Rollback: close this isolated DRAFT; no deployed changes to restore.
