# F14 verified RandomBot stage-write policy — C++ test candidate

**Owner:** CosmicCuddle
**Created:** 31 October 2026
**Status:** Review-only. The policy is not connected to Worldserver, Playerbots, saved characters, or database operations.

This branch builds on combined [IP draft #10](https://github.com/CosmicCuddle/mod-individual-progression/pull/10). The original Hunter and grouping implementation files remain unchanged.

## What has been implemented

The new pure C++17 header `src/IndividualProgressionRandomBotStagePolicy.h` defines an explicit pre-write decision. It reuses race and stage boundaries from `IndividualProgressionGroupPolicy.h`. The policy receives identity, eligibility, route, recorded stage, requested stage, and progression limit **from a future verified caller**. It does not determine identity or perform any mutation.

Only positively verified Blood Elf/Draenei RandomBots are eligible for the F14 decisions. Unknown identity is held rather than presumed to be a RandomBot. A separately verified human, Altbot, Selfbot or another non-RandomBot defers to established rules, without granting permission to write.

For verified TBC-eligible RandomBots, existing saved stages 8–18 are preserved without a no-op rewrite; a requested downgrade to 0–7 or an out-of-range stage is rejected. Saved characters with missing or invalid stages are held for a separate, owner-approved migration choice. Ineligible bots stay dormant. A positively verified, truly new and unsaved RandomBot may initialize at stage 8 only when eligibility and the configured progression limit permit it. `ProgressionLimit=0` means unlimited, but does not by itself prove TBC eligibility.

## Test plan

- Compile the actual header with both C++17 and C++20.
- Exercise a large cross-product of identities, races, stages, routes, caps and entitlement cases.
- Verify only six explicitly reviewed files changed from combined PR #10: the header, test, F14 workflow, this memo, a register entry, and a **testing-workflow-only** adjustment. That adjustment freezes the earlier Hunter+grouping five-file comparison at its verified parent commit instead of misclassifying stacked F14 files as Hunter changes. Both original runtime implementations remain byte-identical.
- Confirm the Hunter executable source and grouping implementation files are byte-identical to the already reviewed branches.
- Confirm no Worldserver `.cpp` calls this policy yet; gameplay implementation awaits a compatible source interface.

## Why not wire it yet?

In the historical Core source, the player login script callback occurs inside `WorldSession::HandlePlayerLoginFromDB`. Historical Playerbots performs its own later bot-login steps after that call returns. Therefore, a safeguard added exclusively to a late Playerbots callback would not protect an earlier IP login stage reset. The positive RandomBot identity and saved state must be established before the first potentially destructive write.

The existing `.ip set` command can change achievements before calling the shared stage writer, so command-side approval must eventually occur before those effects as well.

References: [F14 coordination #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [Python policy research #6](https://github.com/CosmicCuddle/mod-individual-progression/pull/6), and [combined source #10](https://github.com/CosmicCuddle/mod-individual-progression/pull/10).

**Still required:** a source-matched early identity handoff, saved-bot restore integration, actual gameplay implementation, F14 rndbot-only visibility integration with existing Naxx owners, backups, rollback planning, and CosmicCuddle's separate approvals for merge and production installation. No live server files or characters are affected here.
