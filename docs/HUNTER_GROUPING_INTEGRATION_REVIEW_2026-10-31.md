# Integration candidate — Hunter pet autocast and expansion-banded grouping

**Owner:** CosmicCuddle
**Date:** 31 October 2026
**Status:** **DRAFT review candidate only**. Neither implementation merged to `develop/cosmic-ip`, installed in the owner Worldserver, or declared gameplay accepted.
**Base:** frozen [IP grouping #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3) source `e0cd785f6b2090c8f099c824811077d0ab2a9de5`.
**Hunter:** frozen [IP Hunter autocast #1](https://github.com/CosmicCuddle/mod-individual-progression/pull/1), exact original compile-tested `src/IndividualProgressionSpells.cpp` Git blob **`6ee93e1174b5aef6c5d61c9ea31526506c9d08da`**. This blob is identical in original Hunter compiled source `f0044987cc2d6f0b73c1cd25d17012a26a764beb` and current Hunter PR #1 head `b93040b50cf8197b62a7914b4e17741ed35e8a79`.

## What this branch really changes

1. Commit the exact unchanged Hunter autocast executable source `src/IndividualProgressionSpells.cpp` on top of the frozen grouping C++ source. **No new Hunter algorithm or grouping logic.**
2. Copy the original Hunter `tests/check_hunter_autocast_source.py` byte-for-byte, so rank mappings, no stale autocast entries, current pet identity, ON/OFF, delayed load callback, and no A02 saved-settings replay retain a reproducible source regression gate.
3. One lightweight GitHub workflow verifies all exact Git blobs, unchanged grouping `.cpp` and policy header, strict five-file PR scope, original Hunter source checks and actual grouped C++17/C++20 matrix. This test is **not** a real gameplay test or a new Worldserver compile job; the exact *executable source combination* was already separately compiled in an isolated test.
4. This memo and a new entry in the **existing** Custom Modifications Register. The Register is preserved from the grouped branch; do not copy the earlier Hunter draft's register wholesale and accidentally revert grouping entries.

No Playerbots/Core/Naxx module files, SQL, DB schema, DBC, client patches, distributed config defaults, live group, saved pets or worldserver binary change in this PR. Existing Hunter and grouping code branches remain untouched.

## Native compile evidence for this exact C++ combination

[**GitHub run #41145398854 — completed/success, 31 Oct 2026**](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41145398854), source integration experiment [CI-only DRAFT #8](https://github.com/CosmicCuddle/mod-individual-progression/pull/8), job `124369183446`: checked **five frozen source commits**, copied **only exact Hunter spell translation unit** onto the **identical frozen grouping source**, required all three IP translation units in the actual CMake compilation graph, then **full Worldserver compile/link SUCCESS** and linked executable verification **SUCCESS**. Historical Core `8daffdabe9202c228e45d833c9a1d399597a15cd`, Playerbots `e88cc4099e69f35126b28942a412ca702d20453b` with two narrowly guarded runner-only compatibility substitutions, Naxx module `80e9de1c39a6dc995188a80c31a5e785d0f2a6f9`. Isolated build never launched, uploaded, deployed or read the owner's host.

Independent full grouping [run #41133729455](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41133729455) and original Hunter code [run #41084770943](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41084770943) also succeeded. A **native compiler success cannot establish runtime grouping, pet-state correctness or deployed build identity**.

## Mandatory unimplemented changes (release blockers)

- **Direct bot group joining**: Historical Playerbots master-login `GroupInviteOperation` creates or converts a group before direct `Group::AddMember`, bypassing ordinary IP invitation/acceptance hooks. [Source audit #5](https://github.com/CosmicCuddle/mod-individual-progression/pull/5) and [13/13 source-order contract tests #9](https://github.com/CosmicCuddle/mod-individual-progression/pull/9) specify *pre-side-effect* rejection, but **do not implement it**. Preserve existing ordinary bot invitations and OFF pathway.
- **Offline party members**: Draft grouping ON fails closed on new joins when an existing member's tier is unavailable; this may block legitimate parties. Owner has not chosen a final offline policy. Stage transitions within established groups and arena/BG/BF/LFG exceptions are also undecided. No automatic group ejection, unconditional offline DB scan or unapproved arena changes.
- **F14 race10/11 RandomBot TBC floor**: Currently only the *unwired* `IsAllowedRandomBotStage` policy header exists. Positively verified RandomBot identity before destructive IP login/command writes and saved `add` restoration is not implemented; existing stage0 force-write can remove awarded stage8+ markers on certain level<=60 non-normal-account login paths. [F14 research #4](https://github.com/CosmicCuddle/mod-individual-progression/pull/4) and [synthetic model #6](https://github.com/CosmicCuddle/mod-individual-progression/pull/6) explicitly distinguish this. Do not grant saved bots stage8 while TBC is unavailable or hide human/Altbot/Selfbot/Unknown.
- **Hunter gameplay**: Cosmetic/source/native code behavior must still be checked with real 3.3.5a client for rank upgrades preserving ON **and** OFF; different pet switching, stable/relog, delayed pet-load, no stale ghost rank and correct bar refresh. Owner's earlier Wolf Bite Rank8 observation is encouraging but not the complete acceptance matrix. Distinct A02 saved-setting restoration is not part of this implementation.
- **Source/config/backups**: Owner installed Core, PB, IP, Naxx versions and dirty local patches/effective config remain unverified. Historical non-dist config included `EnforceGroupRules=1`, `ProgressionLimit=0` **(means unlimited IP stage cap, not stage7)**; no live config assumptions. Verify backups/restore plan and exact build provenance before any owner-approved deployment.
- **Upstream**: Separate [audit #7](https://github.com/CosmicCuddle/mod-individual-progression/pull/7) blocks unapproved Chromaggus Time Lapse script removal, destructive SQL and silent loss of register entries.

## Release / rollback protocol (not executed)

1. Preserve all owner-installed source diffs and restorable character/world/auth DB backups and known-good binaries before any actual staging or production change. No backup has been verified by this PR.
2. Resolve the direct-add enforcement, offline/special group decision and F14 floor independently with exact installed source interfaces; preserve source refs and write all approved modifications into **one reconciled** Custom Modifications Register.
3. Build the final exact approved combined candidate with the **actual** matching Core/Playerbots/Naxx modules in disposable staging; verify executable. Test switch ON/OFF, human/bot group paths, pet ON/OFF and relogs plus saved progression marker continuity.
4. Request **CosmicCuddle's explicit approval** for merge and separately for deployment, SQL/config changes and any live restarts. Revert reviewed commits/rebuild and restore known-good binary/config if rollback needed; restore saved character state only under separate owner-reviewed recovery plan.

**No production file, process, DB, player character, config, SQL or live Worldserver touched by this branch.**
