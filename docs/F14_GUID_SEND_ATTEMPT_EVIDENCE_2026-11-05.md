# F14 exact per-GUID Core send-attempt evidence adapter (PURE/OFF)

**Owner:** CosmicCuddle
**Maintainer:** CS01 Maintenance during Claude's owner-confirmed break
**Date:** 5 November 2026
**Status:** isolated DRAFT, unwired model, no live server, client, source deployment or database changes

## What the code actually does

New `src/IndividualProgressionF14GuidSendAttemptEvidence.h` is a **pure adapter** between the existing [#93 OUT-OF-RANGE](https://github.com/CosmicCuddle/mod-individual-progression/pull/93) and [#95 CREATE](https://github.com/CosmicCuddle/mod-individual-progression/pull/95) *per-GUID, F14-source-attributed, after-SendDirectMessage-call* facts, and **Claude's original byte-identical [#94](https://github.com/CosmicCuddle/mod-individual-progression/pull/94)** prior-hidden viewer restore protocol, already imported to our real #92 branch by [#97](https://github.com/CosmicCuddle/mod-individual-progression/pull/97).

The adapter accepts a future caller's **independently verified F14 source tag, actual send-call completion, owning map thread, exact viewer GUID/login epoch, creature map+instance+GUID, live incarnation and exact reported GUID**, all of which default to missing/false. It verifies the original protocol has an **exact prior F14 suppression record**, and checks the direction:

- **OUT-OF-RANGE:** only an original F14-linked/unlinked *batched* suppression may upgrade `ImpliedPendingBatchFlush` to `HandedToSessionBatched`.
- **CREATE:** only an exact previously F14-hidden pair with a **pending restore obligation** and a prior same-session/incarnation **relink** may upgrade the batched restore-send attempt. Generic Core creations cannot be called restores.
- A wrong GUID, relogged viewer, wrong map/instance, recycled creature, wrong direction/stage, duplicate attempt, an untagged Core update, missing send-call evidence and an unknown prior record all refuse.
- **Delayed OUT after stop, death, viewer logout or a rights/era change is refused** whenever the exact pair already owes restoration. A late outgoing packet cannot upgrade old suppression evidence once reappearance is required, even if the global restore flag is not frozen.
- **Global restore already required** (config disabled, failed source proof or overflow) freezes evidence upgrades, so late packets cannot silently appear to resolve a full restore requirement.

It calls only the original `ObserveProposedBatchFlush` after validating everything. **It NEVER clears any prior-hidden ledger state** and cannot grant a positive hide. Both `HasIndependentlyVerifiedClientPresentation()` and `IsRuntimeF14ConcealmentAuthorized()` return hardcoded `false`.

## Evidence is NOT client display

WoW client 3.3.5a does not acknowledge receipt/rendering of an individual CREATE or OUT-OF-RANGE object. The Core send-attempt hooks prove at most that a tagged GUID was handed to the session send API. This adapter is **NOT** proof of a client-visible creature returning, selectable target, quest/loot sparkle, safe logout/death reappearance or successful restoration.

**No Worldserver source includes this header, registers a Core send-attempt callback or runs a positive F14 creature concealment policy.** #93/#95 callback hooks remain default false. The existing human quest, combat, group, pet, aura, charm and guardian negative vetoes from #99–#102 remain isolated, and full independent real-player rights are not yet proven. All experimental F14 and paid features remain OFF.

## Tests and constraints

The new C++ test uses actual unmodified #94 protocol code and simulated matched/mismatched source events (not a running server) to exercise source-tagged OUT and CREATE, direct-versus-batched stages, stop/death/retire, correct re-link, viewer logout, wrong map, incarnation reuse, duplicate notifications, configuration-disabled/global-frozen behaviour and the permanent prior-hidden obligation. The source suite pins exact #92 registry and original #94 ledger/protocol Git blobs, six-path scope, OFF configs and no runtime inclusion.

CI compiles GCC/Clang C++17/20 with and without NDEBUG and checks the pinned original #93/#95 source hook patcher blobs read-only. **Do not claim successful CI until exact-head workflow completes.**

No native Worldserver hook is wired by this addition; its pure C++ compilation is not a Worldserver build and does not warrant a gameplay test. Remaining gates: complete human rights, positive selected BE/Draenei headless RandomBot-only identity policy, real Core event/thread coupling, client-visible restoration and targetability, installed source/DB/config/DBC/client inventory, tested backup and rollback, and CosmicCuddle's express approval before any integrated in-game installation.

Rollback: close/reject this isolated DRAFT; the original Claude branch, Core, modules and live host remain untouched.
