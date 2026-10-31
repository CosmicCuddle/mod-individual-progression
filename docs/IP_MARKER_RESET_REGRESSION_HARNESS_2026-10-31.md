# IP progression-marker reset regression harness (C1) — 2026-10-31

**Author:** CS03-IP (Claude, IP coding developer). **Coordination:** [Naxxramas-Server-Coordination Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13); C1 design decision stays in [Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9).
**Type:** TEST-ONLY. No change to `src/`, `data/`, `conf/`, `optional/`, Playerbots, the character DB, F14 or any live server.
**Base:** `develop/cosmic-ip` @ `edd639b0a6eba858830876e987def510450f438a`.
**Register entry:** `IP-TEST-C1-001`.

## What it checks

Playerbots' full randomisation can delete the hidden rewarded quests that Individual Progression (IP) uses to store a character's progression stage. This harness turns that chain into a repeatable automated test. It goes red when any link in the chain changes, and the harness itself is proven to notice deliberate breaks.

Pinned inputs (public, read-only):

| Source | Revision |
| --- | --- |
| mod-playerbots | `e88cc4099e69f35126b28942a412ca702d20453b` |
| AzerothCore (Grimfeather fork) | `8daffdabe9202c228e45d833c9a1d399597a15cd` |
| IP | the checkout under test (this branch) |

## The chain (verified from pinned source)

1. **Entry points into a full reset** (`RandomPlayerbotMgr.cpp`)
   - `RandomPlayerbotMgr::Randomize(bot)` → `RandomizeFirst` → `PlayerbotFactory::Randomize(false)` when level < 3, when a Death Knight is below level 56, or when `!(level < RandomBotMaxLevel || !DowngradeMaxLevelBot)`. Otherwise it calls `Randomize(true)`, which is incremental and does **not** reset quests.
   - Console handler `handlers["init"]` → `RandomizeFirst` (full).
   - Console handler `handlers["clear"]` → `RandomPlayerbotMgr::Clear` → `PlayerbotFactory::ClearEverything()` → `ResetQuests()`. This route was not in earlier notes.
   - `RandomizeMin` is also full, but its `initmin` handler is commented out.
2. **`PlayerbotFactory::Randomize(bool incremental)`** calls `ResetQuests()` only inside `if (!incremental)`.
3. **`PlayerbotFactory::ResetQuests()`** loops over **every** loaded quest template. It skips only `GetQuestStatus(entry) == QUEST_STATUS_NONE`, and otherwise calls `RemoveRewardedQuest(entry)` and `RemoveActiveQuest(entry, false)`. There is no ID filter.
4. **Core `Player::GetQuestStatus`**: for a quest with no `m_QuestStatus` entry (true after `RewardQuest`, which ends with `RemoveActiveQuest` + `SetRewardedQuest`), it returns REWARDED when the quest is not seasonal, not repeatable (`SpecialFlags & 0x0001`) and is in `m_RewardedQuests`.
5. **IP SQL** `data/sql/world/base/player_progression.sql` defines 66001–66018 with `QuestSortID = 0`, `Flags = 1024` (TRACKING) and `quest_template_addon.SpecialFlags = 256` (NO_LOREMASTER_COUNT).
   - After the pinned `ObjectMgr::LoadQuests` normalisation (DB-allowed mask; daily, weekly and monthly flags force the repeatable bit), the markers are **non-repeatable and non-seasonal**. Their status is therefore REWARDED, and `ResetQuests` removes them.
   - The SQL column-to-field mapping (`QuestSortID` → `ZoneOrSort` at [4], `Flags` at [22], addon `SpecialFlags` at [18]) is itself checked against the pinned loader.
6. **Persistence:** `RemoveRewardedQuest` sets `m_RewardedQuestsSave[id] = false`. The next `_SaveQuestStatus` then appends `CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST`. **The deletion is written to the character DB on the next save; it is not in-memory only.**
7. **IP** `GetPlayerProgressionFromQuests` reads `66000 + i` for `i = PROGRESSION_MOLTEN_CORE (1) .. PROGRESSION_WOTLK_TIER_5 (18)`, which is exactly the SQL set. After the reset, the stage reads 0.

### Exposure depends on owner configuration (unverified)

| Pinned fact | Source |
| --- | --- |
| `playerbots.conf.dist` ships `RandomBotMaxLevel = 80`, `DowngradeMaxLevelBot = 0` | `conf/playerbots.conf.dist` |
| **The code default when the key is absent is `DowngradeMaxLevelBot = true`** | `PlayerbotAIConfig.cpp` |
| `RandomBotMaxLevel` is **clamped to worldserver `MaxPlayerLevel`** | `PlayerbotAIConfig.cpp` |

Trigger matrix, computed by **evaluating the condition text extracted from pinned `RandomPlayerbotMgr::Randomize`**:

| Bot | Effective RandomBotMaxLevel | DowngradeMaxLevelBot | Periodic randomise |
| --- | --- | --- | --- |
| level 60 | 80 | 0 | incremental (markers kept) |
| level 60 | 60 | 0 | incremental (markers kept) |
| **level 60** | **60** | **1 / key absent** | **full reset (markers deleted)** |
| level 2 | any | any | full reset |
| DK level 55 | any | any | full reset |

**Consequence:** on a Vanilla server with `MaxPlayerLevel = 60` and either `DowngradeMaxLevelBot = 1` or the key absent from `playerbots.conf`, every level-60 RandomBot's periodic re-randomise erases its IP stage. Operator commands `rndbot init` and `rndbot clear` erase it regardless of these settings. **The owner's effective `MaxPlayerLevel`, `RandomBotMaxLevel` and `DowngradeMaxLevelBot` have not been verified.**

### Counterfactual recorded by the harness

Making the markers repeatable would hide them from `ResetQuests`. It would **also** hide them from IP itself, because IP reads them through `GetQuestStatus`, so stage 8 would read as 0. **"Set the repeatable flag" is therefore not a mitigation.** This is recorded for the Issue #9 design discussion only; no option is chosen here.

## Evidence levels (do not conflate)

| Level | What runs | What a pass means |
| --- | --- | --- |
| **A — source contract** | `tests/ip_marker_reset_extract.py` `check_*`: comment-aware C++ function and enum extraction, MySQL tuple parsing of IP SQL, evaluation of extracted branch conditions | The pinned text still has exactly the recorded chain. Commented-out code never satisfies a contract. |
| **B — compiled extracted bodies** | The generator splices the **verbatim** pinned bodies of `ResetQuests`, `GetQuestStatus`, `RemoveActiveQuest`, `RemoveRewardedQuest`, `SetRewardedQuest`, the rewarded loop of `_SaveQuestStatus`, Core's `Quest::IsRepeatable`/`IsSeasonal`/`Player::IsQuestRewarded`, and IP `GetPlayerProgressionFromQuests` into a translation unit. It compiles that against minimal stand-in types (`tests/marker_reset_harness/harness_stubs.h`), feeds in the SQL-derived flags and runs 86 expectations. | Those exact bodies, executed, wipe stages 1–18 to 0 and queue one DB delete per marker. Controls pass: stage 0, no-reset save, repeatable quest kept, PvP and ordinary collateral. |
| **Not covered** | Worldserver, a real DB, Playerbots scheduling and timing, the owner's installed revisions, configs, SQL overrides | — |

### Negative mutation fixtures (harness sensitivity)

`NegativeMutationFixtures` applies 17 in-memory edits to the real pinned text and requires the named contract to fail. Examples:
- `ResetQuests` skips 66001–66018
- `ResetQuests` moved out of `if (!incremental)`
- `handlers["clear"]` commented out, or `initmin` enabled
- `!DowngradeMaxLevelBot` negation flipped
- `conf.dist` downgrade set to 1, or the max-level clamp removed
- `GetQuestStatus` ignoring the repeatable bit
- `RemoveRewardedQuest` without the DB delete
- the save loop no longer deleting
- the monthly normalisation removed
- marker 66008 `SpecialFlags = 257`, 66003 daily `Flags`, or 66005 seasonal `QuestSortID`
- IP reading `67000 + i`

Two further fixtures check foreign SQL:
- a foreign `UPDATE quest_template_addon … WHERE ID = 66004` must be caught;
- a control on unrelated IDs must not be flagged.

Layer B also builds a mutated `ResetQuests` (skips the IP range) and requires the binary to exit 1.

## Reproduce locally

```bash
git clone https://github.com/mod-playerbots/mod-playerbots historical-playerbots
git -C historical-playerbots checkout e88cc4099e69f35126b28942a412ca702d20453b
git clone https://github.com/Grimfeather/azerothcore-wotlk historical-core
git -C historical-core checkout 8daffdabe9202c228e45d833c9a1d399597a15cd
PINNED_PLAYERBOTS_ROOT=historical-playerbots PINNED_CORE_ROOT=historical-core REQUIRE_COMPILED_HARNESS=1 \
  python3 -I -m unittest discover -s tests -p 'test_ip_marker_reset_chain_source.py' -v
```

A sparse or shallow checkout of only the paths listed in the workflow is enough. Standalone use:

```bash
python3 tests/ip_marker_reset_extract.py check
python3 tests/ip_marker_reset_extract.py generate --out build/marker-reset
g++ -std=c++17 -Wall -Wextra -Werror -I build/marker-reset -I tests/marker_reset_harness \
  tests/marker_reset_harness/harness_main.cpp -o build/marker-reset/harness && build/marker-reset/harness
```

CI: `.github/workflows/ip-marker-reset-regression-harness-20261031.yml` (`contents: read`). It guards the allowed paths, keeps `src/`, `data/`, `conf/` and `optional/` byte-identical, and requires an append-only register. It checks out sparse pins, verifies their SHAs, then runs the unittest (g++ C++17) plus clang++ C++17 and g++ C++20 builds.

## How to use it when C1 is decided

- **Owner picks a Playerbots-side preservation (Option A):** point `PINNED_PLAYERBOTS_ROOT` at the candidate PB fork. `check_pb_reset_quests_removes_every_non_none_quest` and the layer-B stage sweep should then fail exactly where markers are now preserved. Invert those expectations deliberately in that PR, and keep the controls.
- **Owner picks an extra canonical stage record (Option B):** keep this harness as the proof of *why* the record exists, and add tests for the new store.
- **Upgrading Core, Playerbots or IP:** bump the pins in the workflow and module constants. Any drift in the chain surfaces as a named contract failure.

## Risks and limits

- **Not runtime.** The stand-ins (`ObjectMgr`, `CharacterDatabase`, `Player` containers) are minimal, and behaviour outside the extracted bodies is not exercised.
- **Text-based.** Contracts assert specific source shapes, so a pure refactor upstream will fail the harness. That is intended (re-verify, then update), but it means a red result is not automatically a regression.
- The IP SQL scan covers `data/sql` and `optional/sql` in this repository only. Live DB overrides and owner-local SQL are unknown.

## Rollback

Delete the seven files listed in the workflow guard and remove the `IP-TEST-C1-001` register section. Nothing else references them, and no runtime, config or data change has to be reversed.
