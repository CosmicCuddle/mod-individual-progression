# F14 RandomBot earned-progress markers — Playerbots ResetQuests release blocker

**Owner:** CosmicCuddle  
**Source-only test branch:** `test/cs01-f14-playerbots-reset-blocker-20261103`, stacked on Individual Progression DRAFT #46, exact source parent `26d5b7a1de30ad92a595aaf251fc14754cf54f32`.  
**Status:** `BLOCKED_CORE_PB_HOOK`; regression/negative tests only. This is not a working preservation fix. The exact pinned `PlayerbotFactory::ResetQuests` behaviour is documented below. No hook, Playerbots edit, fork, game feature, migration, live data action or deployment. This file is **not** permission to change an installed bot.

## Confirmed pinned source chain

- Historical *test-dependency* Playerbots `mod-playerbots/mod-playerbots@e88cc4099e69f35126b28942a412ca702d20453b`, [`src/Bot/Factory/PlayerbotFactory.cpp:1687-1705`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/Factory/PlayerbotFactory.cpp#L1687-L1705): `ResetQuests()` clears all quest-log slots, then iterates quest templates and calls **both** `RemoveRewardedQuest(entry)` and `RemoveActiveQuest(entry, false)` for every non-NONE quest. There is no exempt range for the IP stage marker quests **66001–66018**.
- Same pinned Playerbots [`Randomize(bool incremental):844-852`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/Factory/PlayerbotFactory.cpp#L844-L852): a **full** (`!incremental`) reset calls `ResetQuests()`; [`Randomize:1077-1085`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/Factory/PlayerbotFactory.cpp#L1077-L1085) then persists the altered bot with `SaveToDB(false, false)`.
- Current IP #46 login code [`src/IndividualProgressionPlayer.cpp`](https://github.com/CosmicCuddle/mod-individual-progression/blob/26d5b7a1de30ad92a595aaf251fc14754cf54f32/src/IndividualProgressionPlayer.cpp) preserves an **already earned** stage 8–18 only for a positively verified headless Blood Elf/Draenei RandomBot, if the F14 partial safeguard is enabled and the F19 level/progression-limit check allows it. If the marker is gone and the raw stage is 0, the safeguard cannot reconstruct its history; the original non-normal-account level-stage write takes effect (level ≤60: 0, 61–70: 8, 71+: 13).
- IP's F14 group floor protects only **existing recorded stages** from downward group sync. It neither restores erased quest markers nor grants initial stage 8 to newly created bots.
- Core `Player::RemoveRewardedQuest` at the pinned historical Grimfeather Core lacks a module veto callback (see independent source review in [coordination #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6604947427)). An `OnPlayerSave` re-reward workaround is explicitly **rejected**: nested `RewardQuest` → `SaveToDB`, recursion/race potential and lost historical state. No implementation is attempted here.

**Consequence:** a verified RandomBot at level 50 with earned stage 8 may relog at stage 0 after a full Playerbots reset. At level 65 a bot at earned stage 12 can fall back to 8, and at level 75 stage 18 can fall back to 13. These are source-derived/model outcomes, **not a demonstration on owner's live character DB**. Other full-reset entry points may exist; the above path suffices to block a stage-changing rollout.

## Narrow eventual preservation requirement — no code approval

1. Preserve quest IDs **66001 through 66018** **only when they are already marked rewarded** on that bot, and only through the Playerbots reset path. Do not fabricate completion, auto-reward TBC/Wrath, alter other quest logic, infer from race or grant levels. Other quests must still reset normally.
2. A possible minimal Playerbots-side solution is an explicit `ResetQuests` exclusion list, **default empty** and scoped to that reset operation, with the source identity and itemised IDs verified before setting it. Any actual config name, loading, validation and module/fork location must be reviewed against installed Playerbots; `Playerbots.ResetQuestsPreserveQuestIds` is only a **proposed key**, not an installed setting. A narrow Core hook is an alternative after review.
3. Make a disposable source-only test for the **unset/empty list** (unchanged stock reset), the explicit 18-ID list (only previously rewarded marker quest IDs preserved), other rewarded/active quests still cleared, no invented stage 8 for a new bot, and no overlapping or unknown quest IDs.
4. Verify reachability through full randomisation, `ClearEverything`, first login/creation, GM/admin initialisation, repeated rerandomisation, bot relog, actual IP limit and group sync. Test with Human/Altbot and ordinary-race RandomBot controls. Verify positive identity only on a safe owning thread.
5. Run pinned **combined Core + PB + IP + Naxx + Services Worldserver compile/link**, then stage on a separate restorable server with disposable bot identities and explicit owner permission. Character DB backup, source/config/binary backup, rollback and gameplay results are mandatory before production activation.

**Do not implement:** `OnPlayerSave` stage re-mark, `OnPlayerLevelChanged` recovery, `ForceUpdateProgressionState(8)` or `RewardQuest(66008)` for every Blood Elf/Draenei, blanket RandomBot quest preservation, regex-only bot identification, live SQL patch or unapproved Playerbots fork.

## Regression/test scope

- `tests/f14_playerbots_reset_blocker.cpp`: portable C++ negative model for pre-reset preservation, post-reset loss and non-fabrication of progress. The test intentionally **passes** when it documents current unsafe loss; it does NOT repair that loss.
- `tests/test_ip_f14_playerbots_reset_blocker.py`: protected IP source/config invariants and release-blocking documentation/source ownership. It must fail if an unreviewed IP save-path reconstruction is introduced.
- Dedicated isolated CI tests only; no AzerothCore executable, Playerbots binary, bot creation, real quest update, server launch or production configuration changed.

**Disposition:** leave F14 stage-changing rollout **blocked** until an owner-approved PB/Core preservation seam and complete cross-module tests exist. Grok/CS02 is on owner-confirmed break; CS01 Maintenance is covering the current review duties. Further work is staged on a draft branch. Owner makes final decisions.
