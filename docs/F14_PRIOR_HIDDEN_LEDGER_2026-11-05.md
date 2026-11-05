# F14 per-viewer prior-hidden restoration ledger — stage-4 pure experimental model
**Owner:** CosmicCuddle. **Developer:** CS01 Maintenance. **Date:** 5 November 2026.
**Base:** IP F14 #83 exact 1f71b99c2cfff79b2ad42019769da539c89ec61b.
**Status:** DRAFT, UNWIRED pure C++ header + tests. Not a server/client implementation or approved gameplay change.

## Purpose
Future F14 creature presentation must not strand a Vanilla observer with a hidden creature when a selected BE/Draenei RandomBot switches target, dies, logs out without AttackStop, or when the target despawns, respawns, map unloads, config disables, or rights/era change. The current #83 map-thread queue requests a *visibility pass* but holds no per-observer record of who previously received F14-specific suppression. This model begins that distinct ledger without claiming a positive hide or packet delivery.

## Invariants
- A suppression record is accepted **only** if the future Core dispatch positively attributes a hide to F14. Ordinary Core link removal, phase, WHO, a race label, an unverified identity, missing packet, or global visibility change is NEVER sufficient. There is currently **no caller** supplying that proof.
- Identity is (viewer player GUID **and session epoch**, creature map ID + instance ID + full raw GUID, and exact creature incarnation token). No pointers. Relogging the viewer or recycling the creature does not acknowledge an old suppression.
- Stop/logout/corpse/map unload/era/config invalidation keeps affected records until explicitly restored, even if the live creature token was cleared. Capacity overflow or different incarnation for the same pair freezes future suppressions and requires global restoration; existing entries are never silently evicted.
- A server link update or `UpdateObjectVisibility` is **not** client acknowledgement. Only an independently verified restoration-completion source could call the model's completion API. There is **no such verified source** in current 3.3.5a implementation. Therefore this is NOT safe to wire, activate, or clear during runtime yet.
- Model includes a global post-restoration clear gate that refuses early reset with pending entries. This does not run in the server.

## Scope and dependencies
New pure header, C++ model test, Python source contract, one isolated hosted source-model workflow and this note. No code modifies `IndividualProgressionF14LifecycleScript.cpp`, Core, Playerbots, Naxx, Character Services, config, SQL, DB, DBC or client. Existing `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION` remains UNDEFINED and all F14/paid flags OFF.

## Remaining implementation blockers
Source-tagged positive Core per-viewer suppression/restore hooks, complete human target/threat/assist/DoT/quest/tap/loot/group/pet rights, proven per-viewer client restoration delivery and response to overflow/map unload/relog, cross-module build, actual owner-approved 3.3.5a gameplay test. Do NOT treat an internally simulated `true` proof in pure tests as a real server/client proof.

**Rollback:** close the unmerged draft. No live or production state touched. A future integrated change needs installed source/config inventory, backups, regression plan and CosmicCuddle's approval.
