# F14 v5 source follow-up — exact social Register and group-floor population

Owner CosmicCuddle. This is a source-only peer record under [coordination #51](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51), not a composed branch, runtime eligibility decision, installed-source assertion or deployment plan. F14/Paid remain OFF.

## Exact social snapshot and overlap

Claude's social branch advanced from v4's `146dbdfccedbf1e4096f158502a5a7aff7a65117` to `c4e0c7a9920bf65e6b636b0f625a287f87bf9095`. The T1 child `34bdc7524bcf947bd8fc551a44a6ead3847db50e` was fast-forwarded onto the social branch with an owner-selected fail-open behavior and a documentation commit. CS01 then marked the current T1 implementation **CHANGES REQUIRED**: its off-world-thread guard calls a foreign `Player::FindMap()`, which reads a plain `m_currMap` before ownership is proven ([CS01 review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6860967009)). This is Claude-owned source. The earlier `std::atomic<std::thread::id>` type objection was [retracted](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6859632602); it is not the blocker. Current social runtime safety is therefore **not accepted** on this evidence.

At exact `c4e0c7a9`, local `git merge-base` confirms IP172 `aed05a92f262362608f5d643c8570b504189aae0` as the common base. The changed-path set remains 12 paths, with only `docs/CUSTOM_MODIFICATIONS_REGISTER.md` intersecting each Codex #174–#178 candidate delta. This is path evidence only; it does not certify thread, hook, ABI or gameplay compatibility. A moving branch must be rechecked before any later trial.

## Exact Register blob audit

The v5 [manifest](F14_REGISTER_RECONCILIATION_MANIFEST_V5_2026-11-13.json) pins the external social `parent` and `head`. The enhanced read-only [Register checker](../tests/f14_register_reconcile.js) now compares that exact local blob to IP172 when present; a partial clone without the blob reports **UNKNOWN**, rather than accepting supplied headings as verification. The [composition checker](../tests/f14_composition_preflight.js) similarly derives changed paths and all three added config defaults from the exact Git diff, then flags any mismatch against supplied metadata. It runs no fetch, patch, merge or worktree command.

At the locally available `c4e0c7a9` blob, social adds exactly one Register section and removes/modifies no earlier section. Its dated heading lacks a stable `IP-*` feature ID. The row contains nested New/Changed file bullets but no explicit **Affected paths** field, so the strict Register field check reports that format gap. There is no duplicate local stable ID among the five Codex rows; a cross-branch ID collision cannot be fully resolved until Claude/CS01 assigns the social row a stable ID. The exact social config diff adds isolation `0`, reply text, and cooldown `60`; the v4 supplied metadata had omitted the reply value. This audit fills that metadata gap without touching Claude's branch. A Register/source snapshot at `c4e0c7a9` does not imply the T1 candidate is accepted.

## Temporary group-floor population decision matrix

The newer owner rule specifies an **effective grouped-era** minimum TBC for BE/Draenei bots and restoration of exact earned era on departure. [Information #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10#issuecomment-6858551237) must reconcile its target population with older OFF-mode group-sync behavior. This matrix states implementation consequences, not a selected eligibility predicate:

| Character in an existing group-sync context | Positive identity evidence needed | Temporary race10/11 TBC floor | Existing behavior to preserve / decision needed |
| --- | --- | --- | --- |
| Verified RandomBot | Exact login/session proof from the verified PB/IP cache, not race or account regex | Owner's new bot rule points to **yes**, subject to event-time producer proof | Keep earned quests unchanged; refuse unknown/stale identity; social isolation has its own RandomBot-only rule |
| Positively identified Altbot | Independent Altbot-specific source/epoch; `!normal` is insufficient | **BLOCKED ON #10** | Decide whether older excluded/non-normal OFF sync is superseded for this population |
| Excluded/non-normal account without positive bot control | Account category alone is not bot proof | **BLOCKED ON #10** | Preserve older OFF exact-stage behavior until Information/owner resolves it; do not silently apply a bot-only temporary floor |
| Ordinary human, including race10/11 | Normal-human proof and current raw stage | No bot floor is selected | Existing expansion-band group admission and race restrictions remain distinct; never rewrite human earned stages merely to allow grouping |
| GM, disconnected, unknown or race-only classified actor | No sufficient positive bot proof | No override while UNKNOWN | Fail to earned-only/stock path; do not use race to infer a bot |
| Positively identified bot of another race | Positive bot source/epoch | No **race10/11** minimum by this rule | Existing bot group-sync behavior may still apply, separately from the BE/Draenei floor |

Even after population is decided, #176 cannot be wired safely until a complete event-time session/map/membership/leader issuer, callback ownership and old-context capture, same-leader group-era change event, tombstone/quiescence rule and restart/GUID-reuse policy exist. In particular, its model does not authorize `SyncBotsProgressionToLeader` to write a temporary effective era into persistent quest progression. EnforceGroupRules ON/OFF and failed-accept pre-writes need explicit preservation tests. The population decision alone does not close those engineering gaps.

## Verification and next boundary

`node --test tests/test_f14_composition_preflight.js tests/test_f14_register_reconcile.js`: **10/10 PASS** locally with exact social `c4e0c7a9` objects available. The tests cover a missing external head returning UNKNOWN and a supplied config default that disagrees with the exact diff. In a fresh partial clone, the external checks stay UNKNOWN until the pinned objects are present; the suite checks that fallback. No C++ compiler, full Worldserver link, composite runtime, gameplay or host test was run here.

The authorized disposable source-composition rehearsal remains **NOT RUN** because the managed worktree service did not create an attachment on two attempts ([status](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6859876333)). Do not upgrade the 15 Register-only path intersections to a composite build result. Next read-only work is to recheck Claude's T1 correction at its exact new head, obtain #177 exact-head native evidence, and coordinate the social Register ID and Information #10 population rule before any integration proposal.
