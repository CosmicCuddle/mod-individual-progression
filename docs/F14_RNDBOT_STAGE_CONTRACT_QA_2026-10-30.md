# F14 synthetic stage-write contract — standalone test gate

**Owner:** CosmicCuddle  
**Status:** TEST-ONLY DRAFT. Not an installed IP fix, an approved bot migration, an in-game test, or an authorisation to merge/deploy.  
**Parent:** existing docs-only [F14 research PR #4](https://github.com/CosmicCuddle/mod-individual-progression/pull/4) at `bf09eccfc757fd8aaa7b034e3422787825040b9a`; separate stacked DRAFT so the original PR stays exactly two Markdown files.

## Why this exists

F14 research established a conditional data-loss hazard: on the pinned IP source `e0cd785...`, a non-normal-account character at level <= 60 can be sent to `ForceUpdateProgressionState(0)`, which removes previously rewarded hidden progression quests before adding only those through stage 0. An eligible, positively identified Blood Elf/Draenei RandomBot previously at stage 8 or later must never be sent through that reset merely because its character level is 60.

This test suite **does not patch** that behaviour. It makes the owner-approved *desired policy* independently executable as a pure model so that a future implementation can be tested against the intended invariant before real quests are touched.

## Contract boundaries

- The model **requires an explicit** `Identity.RANDOM_BOT` plus race 10/11. Human, Altbot, Selfbot, Unknown and Vanilla-race random bots defer to existing game behaviour. It contains **no identity-detection implementation** and therefore cannot classify real Worldserver players or override Naxx's existing source-only F14 classifier.
- It requires explicit `TbcEligibility.AVAILABLE`, `UNAVAILABLE` or `UNKNOWN`. `ProgressionLimit=0`, character level 60, and global Core WotLK expansion alone are *not sufficient* to infer per-character TBC eligibility. Unknown means protected bots remain dormant.
- Newly selected, genuinely unsaved, TBC-eligible positive rndbots may be initialized at stage 8; **saved** stage-0..7 or missing/invalid stage is held for **separately owner-approved migration**, not silently rewritten. Ineligible saved bots are retained dormant and cannot have quest markers rewritten.
- Stage 8..18 on existing positive rndbots is preserved on relog/restore; idempotent stage requests perform no rewrite. Any requested change below 8 or above 18 is rejected; approved stage changes in 8..18 remain representable.
- The decision is applied in model tests to *all* named ingress routes: new selection, saved-event restore, relog, manual bot login, group acceptance/sync, GM tier writes, progression events. **No actual playerbot hooks are wired.**
- The pure model never loads SQL, reads effective config, accesses `currentBots`, starts processes, rewrites quests, or touches the owner host. It also does not resolve positive identity snapshot thread safety or observer-specific WHO/world visibility.

## Additional exact IP command-write call sites (source audit, no fix)

Source-pinned to grouping draft `e0cd785f6b2090c8f099c824811077d0ab2a9de5`:

- [`src/cs_individualProgression.cpp::HandleSetIndividualProgressionCommand`](https://github.com/CosmicCuddle/mod-individual-progression/blob/e0cd785f6b2090c8f099c824811077d0ab2a9de5/src/cs_individualProgression.cpp#L121-L155) accepts an explicit stage, invokes `CheckProgressionAchievements` **before** `ForceUpdateProgressionState(target, stage)` when lowering an already recorded stage, then updates phasing. `CheckProgressionAchievements` can remove earned achievements. **A guard placed only inside `ForceUpdateProgressionState` would be too late to prevent earlier command-side achievement changes.** Any future F14 positive-rndbot invalid-downgrade rejection needs a reviewed *pre-side-effect* command check, without changing normal GM/human/excluded-account permissions.
- [`HandleSetBotIndividualProgressionCommand`](https://github.com/CosmicCuddle/mod-individual-progression/blob/e0cd785f6b2090c8f099c824811077d0ab2a9de5/src/cs_individualProgression.cpp#L155-L194) loops **all non-normal-account group members**, despite reporting them as 'RND bots', then directly forces them to the group leader's recorded stage. This pathway can bypass ordinary invite/accept hooks, and does **not** positively distinguish RandomBots from Altbots/Selfbots/Unknown merely by the account check. F14 policy must be applied only when identity is positively verified, before each protected individual write; other existing command behaviour is a regression/no-change control.
- [`IndividualProgression.cpp::SyncBotsProgressionToLeader`](https://github.com/CosmicCuddle/mod-individual-progression/blob/e0cd785f6b2090c8f099c824811077d0ab2a9de5/src/IndividualProgression.cpp#L437-L485) can also call `ForceUpdateProgressionState(member,...)` after grouping. The grouping switch OFF intentionally retains legacy sync; the mandatory positive-rndbot floor must protect stage markers **regardless of switch setting**.
- Other `RemoveRewardedQuest` sites in `IndividualProgression.cpp` include **Vanilla PvP rank quest cleanup** and must **not** be mistakenly classified as removing the IP expansion-stage marker set. The relevant protected expansion markers are `66000 + stage`; do not globally suppress PvP title cleanup.

**Minimum extra regression:** simulate a level-60, saved stage-8+ *verified* race-10/11 RandomBot; issue a modelled `.ip set 0` or `.ip setbots` stage-0 write, and assert the proposed policy rejects it *before* any achievement mutation, quest-marker removal, or phase update. Repeat with a human/Altbot/Selfbot negative control to ensure the F14-only guard does not override existing commands. This is a test *specification*; no server command was executed.

## Tests and status boundary

Files: `tests/f14_rndbot_stage_contract.py` (model), `tests/test_f14_rndbot_stage_contract.py` (standard-library deterministic cases), and an isolated CI workflow. The CI strict four-file gate ensures no new runtime C++, SQL, config, or original F14 research files change on this child branch.

A successful synthetic run establishes **only model consistency**. Before any real stage-floor code: match the installed owner Core, Playerbots and IP versions; resolve exact player identity before the first `ForceUpdateProgressionState` write; test new/random/saved/restored/manual routes; preserve stage-marker sets and other excluded-account behaviour; document backups and rollback; obtain explicit owner staging and deployment approvals. Keep F14 WHO/world visibility work with its existing Naxx owners until a reviewed one-policy integration is approved.

**References:** [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [grouping Issue #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10), [F14 research section 3E](https://github.com/CosmicCuddle/mod-individual-progression/blob/research/f14-rndbot-eligibility-20261030/docs/F14_RNDBOT_TBC_ELIGIBILITY_RESEARCH_2026-10-30.md#3e-confirmed-logins-stage-0-force-update-can-remove-existing-tbc-progression-markers).
