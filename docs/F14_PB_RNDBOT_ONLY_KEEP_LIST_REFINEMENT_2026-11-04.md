# F14 Playerbots earned-marker reset — selected RandomBots only

**Owner:** CosmicCuddle. **Maintenance branch:** `research/cs01-pb-reset-rndbot-only-20261104`.
**Status:** DRAFT / DISABLED / **NO HOST** / **NO LIVE APPLICATION**.
**Prerequisite:** [Original tested optional reset keep-list #49](https://github.com/CosmicCuddle/mod-individual-progression/pull/49), exact head `e9252a4f37248d8727cb96004006a741a6c6c263`.
**Playerbots source pin:** `mod-playerbots/mod-playerbots@e88cc4099e69f35126b28942a412ca702d20453b`.

## Problem

The original #49 test-only patch protects already-rewarded IP markers 66001–66018 from `PlayerbotFactory::ResetQuests`, when `Playerbots.ResetQuestsPreserveQuestIds` is explicitly filled. Its default is still empty/OFF. However, the Factory can also be used for Altbots. The original optional behavior would therefore preserve earned markers on Altbots too, whether or not this was intended.

## Narrow code change

This branch stores **one additional second-layer patch file**, not a replacement for #49. Applied **after** the exact original #49 patch in a disposable Playerbots checkout, it changes **only** `src/Bot/Factory/PlayerbotFactory.cpp`:

- Compute `preserveForVerifiedRandomBot = preserveMask != 0 && sRandomPlayerbotMgr.IsRandomBot(bot)` once at the beginning of `ResetQuests`. An empty mask avoids even the positive identity lookup.
- Require that positive selected RNDBOT result in the existing preserve guard, *in addition to* already-rewarded quest status, ID 66001–66018 and whitelisted mask bit.
- Keep all other reset actions intact. Do not grant stages, reward quests, change normal quests, change IP/SQL, or make a Playerbots fork.

The pinned Playerbots `RandomPlayerbotMgr::IsRandomBot(Player*)` rejects `IsSelfBot` and delegates to `IsRandomBot(lowGuid)`, which checks the RandomBot account allowlist **and** `currentBots.contains(lowGuid)`. Therefore **race or bot account is insufficient**. Normal players, Altbots, excluded humans and bots not currently selected are NOT eligible for this optional preservation.

## Tests and boundaries

The CI-only job applies both immutable-original and refined patches to a fresh pinned Playerbots checkout and runs original #49 source tests after the first patch, then refined source tests after the second; four GCC/Clang x C++17/C++20 pure-model runs cover 676 cases each. All `src/`, `conf/`, `sql/` and `data/` paths in the **IP fork** remain untouched.

This gate does **not** establish whether `currentBots` is populated when every possible Factory `ResetQuests` call occurs; if a selected RandomBot isn't recognised at that point, stock reset behavior applies and markers may be removed. Confirm with actual rerandomize/relog and positive identity telemetry on the separate owner-approved test server. Also needs a **newest Grim Core + joint F14 + both Playerbots patches native Worldserver build**, gameplay/quest/loot regression, and explicit owner acceptance of the final reset policy.

## Rollback and release

Discard the child PR; original #49 remains intact. Do **not** set `Playerbots.ResetQuestsPreserveQuestIds` on a live host, merge into protected IP master, or deploy patches before CosmicCuddle approves the **single complete combined release**, with restorable backups and rollback.
