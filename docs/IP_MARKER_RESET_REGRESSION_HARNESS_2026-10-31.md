# IP progression-marker reset regression harness (C1) — 2026-10-31

**Author:** CS03-IP (Claude, IP coding developer). **Coordination:** [Naxxramas-Server-Coordination Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13); C1 design decision stays in [Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9).
**Type:** TEST-ONLY. No change to `src/`, `data/`, `conf/`, `optional/`, Playerbots, the character DB, F14 or any live server.
**Base:** `develop/cosmic-ip` @ `edd639b0a6eba858830876e987def510450f438a`.
**Register entry:** `IP-TEST-C1-001`.
**Revision 2** (same date): addresses the [CS02-IP review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6543427658) as scoped by [CS01-IP](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6543457181). See [Revision 2 changes](#revision-2-changes).

## What it checks

Playerbots' full randomisation can delete the hidden rewarded quests that Individual Progression (IP) uses to store a character's progression stage. This harness turns that chain into a repeatable automated test. It goes red when any link in the chain changes, and the harness itself is proven to notice deliberate breaks.

Pinned inputs (public, read-only):

| Source | Revision | Role |
| --- | --- | --- |
| mod-playerbots | `e88cc4099e69f35126b28942a412ca702d20453b` | **primary pin**; every PB contract and Layer B |
| mod-playerbots | `037c01418b5d01506917a3db9b44fd56ac5f965c` | **review-only** config key-name comparison (an ancestor of the primary pin by 53 commits); never used as chain evidence |
| AzerothCore (Grimfeather fork) | `8daffdabe9202c228e45d833c9a1d399597a15cd` | Core contracts and Layer B |
| IP | the checkout under test | IP contracts and SQL |

## The chain (verified from pinned source)

1. **Routes into a full reset** (`PlayerbotFactory::Randomize(false)` or `ClearEverything()`; each one ends in `ResetQuests()`)

   | Route (pinned file) | Who / when | Gate at the pin | Contract |
   | --- | --- | --- | --- |
   | `RandomPlayerbotMgr::Randomize` → `RandomizeFirst` (`RandomPlayerbotMgr.cpp` L2001–2021) | periodic RandomBot re-randomise | level < 3; DK level < 56; or `!(level < RandomBotMaxLevel \|\| !DowngradeMaxLevelBot)`. Otherwise `Randomize(true)` runs, which is incremental and does **not** reset quests. | `check_pb_trigger_matrix`, `check_pb_full_reset_entry_points` |
   | console `rndbot init` → `RandomizeFirst` | operator | none beyond console access | `check_pb_full_reset_entry_points` |
   | console `rndbot clear` → `Clear` → `ClearEverything()` (L2180–2184) | operator | none beyond console access | `check_pb_clear_everything_route` |
   | `RandomizeMin` | — | its `initmin` handler is commented out | `check_pb_full_reset_entry_points` |
   | **GM `.playerbot bot initself`**, `initself=uncommon\|rare\|epic\|legendary\|<gs>` (`PlayerbotMgr.cpp` L967–1058) | **the calling player's OWN character** | only `master->CanBeGameMaster()` | `check_pb_gm_initself_full_reset_routes` |
   | `RandomBotLevelMgr::AdjustBotToRange` (`RandomBotLevelMgr.cpp` L294, call L347) ← `ProcessPendingLevelResets` ← `Update` | level-bracket redistribution | `Playerbots.LevelBrackets.Enabled`: code default **false**, conf.dist **0** | `check_pb_level_mgr_full_reset_routes_default_off` |
   | `RandomBotLevelMgr::ResetBot` (L829, call L842) ← `OnBotLogin`, `OnBotLevelChanged`, `RunResetPlayedTimeCheck` | max-level reset on login, level-up or played time | `Playerbots.ResetBotLevel.Enabled`: code default **false**, conf.dist **0**; PlayerScript hooks L1144/L1151 return early when OFF | same |
   | `RandomBotLevelMgr::SkipBotLevel` (L852, call L865) ← `OnBotLogin`, `OnBotLevelChanged` | level skip | same flag | same |

   - The `initself` row is a **historical source risk for a real GM character's saved IP stage**. It is not runtime evidence and not proof that any GM has used it. Do **not** run `initself` on a character with saved progression.
   - The bot `init=*` variants in `PlayerbotHolder::ProcessBotCommand` (defined at L722; `Randomize(false)` at L815–857) and the bot-login path at L650 are restricted to AddClass bots (the L782 "You can only use this command on addclass bots" return, and the `addClassBot` check at L639). They are **not** inventoried as contracts because the owner reports no AddClass use; that report is unverified.

2. **`PlayerbotFactory::Randomize(bool incremental)`** calls `ResetQuests()` only inside `if (!incremental)`.
3. **`PlayerbotFactory::ResetQuests()`** loops over **every** loaded quest template. The loop body is exactly five top-level statements:
   1. the quest pointer;
   2. the entry;
   3. skip only `GetQuestStatus(entry) == QUEST_STATUS_NONE`;
   4. **unconditional** `RemoveRewardedQuest(entry)`;
   5. `RemoveActiveQuest(entry, false)`.

   There is no ID or flag filter.
4. **Core `Player::GetQuestStatus`**: for a quest with no `m_QuestStatus` entry, it returns REWARDED when the quest is not seasonal, not repeatable (`SpecialFlags & 0x0001`) and is in `m_RewardedQuests`. Markers have no such entry after `RewardQuest`, which ends with `RemoveActiveQuest` + `SetRewardedQuest`.
5. **IP SQL** `data/sql/world/base/player_progression.sql` defines 66001–66018 with `QuestSortID = 0`, `Flags = 1024` (TRACKING) and `quest_template_addon.SpecialFlags = 256` (NO_LOREMASTER_COUNT).
   - After the pinned `ObjectMgr::LoadQuests` normalisation (DB-allowed mask; daily, weekly and monthly flags force the repeatable bit), the markers are **non-repeatable and non-seasonal**, so their status is REWARDED.
   - The SQL column-to-field mapping (`QuestSortID` → `ZoneOrSort` at [4], `Flags` at [22], addon `SpecialFlags` at [18]) is itself checked against the pinned loader.
6. **Persistence:** `RemoveRewardedQuest` sets `m_RewardedQuestsSave[id] = false`. The next `_SaveQuestStatus` then appends `CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST`. **The delete is queued for the character DB on the next save.** Layer B proves the bodies *queue* it; it does not prove a Worldserver commits it.
7. **IP** `GetPlayerProgressionFromQuests` reads `66000 + i` for `i = 1..18`, exactly the SQL set. After the reset, the stage reads 0.

## Exposure — what decides it (owner configuration UNVERIFIED)

### Three different "caps" (do not conflate)

| Cap | Where | What it does | Contract |
| --- | --- | --- | --- |
| worldserver `MaxPlayerLevel` | Core config | Hard world level cap. **This is the input to PB's clamp.** | (Core) |
| IP Vanilla / bot caps | IP `OnPlayerGiveXP` | **XP gates only.** Pre-TBC (stage < 8) human XP stops at 60; bot accounts stop at `IndividualProgression.BotAccountsMaxLevel` (default **80**). IP never reads or writes `CONFIG_MAX_PLAYER_LEVEL`. | `check_ip_level_caps_are_xp_gates_not_world_cap` |
| effective PB `RandomBotMaxLevel` | `PlayerbotAIConfig.cpp` | Configured value (default 80), **clamped to worldserver `MaxPlayerLevel`**. Drives the trigger matrix below. | `check_pb_config_exposure_defaults` |

"Vanilla level cap 60" describes IP gameplay intent. It is **not** evidence that `MaxPlayerLevel = 60`.

### Trigger matrix
Computed by evaluating the condition text extracted from pinned `RandomPlayerbotMgr::Randomize`:

| Bot | Effective RandomBotMaxLevel | DowngradeMaxLevelBot | Periodic randomise |
| --- | --- | --- | --- |
| level 60 | 80 (e.g. `MaxPlayerLevel` 80, PB value 80) | any | **incremental (markers kept)** |
| level 60 | 60 | 0 | incremental (markers kept) |
| **level 60** | **60** (PB value ≤ 60, or `MaxPlayerLevel` = 60) | **1, or effectively absent** | **full reset (markers deleted)** |
| level 2 | any | any | full reset |
| DK level 55 | any | any | full reset |

A full reset therefore needs **bot level ≥ effective `RandomBotMaxLevel` AND downgrade on**. If IP holds bots at 60 while the effective PB max stays 80, the periodic path is incremental.

### Config namespace rename (CS02-IP finding 1)

| PB revision | Keys read | `DowngradeMaxLevelBot` code default | conf.dist ships |
| --- | --- | --- | --- |
| `037c0141` (review-only) | `AiPlayerbot.RandomBotMaxLevel` (L412), `AiPlayerbot.DowngradeMaxLevelBot` (L631) | `true` | `AiPlayerbot.DowngradeMaxLevelBot = 0` (L825) |
| `e88cc409` (primary) | `Playerbots.RandomBotMaxLevel` (L413), `Playerbots.DowngradeMaxLevelBot` (L634) | `true` | `Playerbots.DowngradeMaxLevelBot = 0` |

- At `e88cc409`, **no source file under `src/` (1,423 files scanned) reads any `"AiPlayerbot.*"` option**. There is no compatibility fallback (`check_pb_no_legacy_config_namespace`).
- **Consequence:** a `playerbots.conf` still written with `AiPlayerbot.DowngradeMaxLevelBot = 0`, running against a PB at or after the rename, is **silently ignored**. Downgrade becomes **true**, so level ≥ effective max takes the full-reset path. `ConfigNamespaceAudit` derives this from source.
- The `RandomBotLevelMgr` flags are new `Playerbots.*` keys with default **false**, so a stale config leaves those routes OFF.

**Read-only owner inventory (for CS01-IP; no change requested):**
- the installed PB commit;
- whether `playerbots.conf` uses the `AiPlayerbot.*` or `Playerbots.*` spelling for `RandomBotMaxLevel` and `DowngradeMaxLevelBot`, and their values;
- `Playerbots.LevelBrackets.Enabled` and `Playerbots.ResetBotLevel.Enabled`;
- worldserver `MaxPlayerLevel` and `IndividualProgression.BotAccountsMaxLevel`.

Pinned Core `ConfigMgr::GetValueDefault` (`src/common/Configuration/Config.cpp`) also:
- accepts **environment-variable overrides** (upper-snake key, e.g. `PLAYERBOTS_DOWNGRADE_MAX_LEVEL_BOT`), so check the service environment;
- logs `Missing property <key>` at the configured missing-option severity, so the worldserver startup log may show a missing `Playerbots.*` key.

These are inventory hints, not harness contracts.

### Counterfactual recorded by the harness

Making the markers repeatable would hide them from `ResetQuests`. It would **also** hide them from IP itself, because IP reads them through `GetQuestStatus`, so stage 8 would read as 0. **"Set the repeatable flag" is therefore not a mitigation.** This is recorded for the Issue #9 design discussion only; no option is chosen here.

## Evidence levels (do not conflate)

| Level | What runs | What a pass means |
| --- | --- | --- |
| **A — source contract** | 19 `check_*` contracts in `tests/ip_marker_reset_extract.py`. Comment-aware C++ function and enum extraction (fails closed on conditional preprocessor directives inside an extracted body); top-level statement splitting; call-site inventory; MySQL tuple parsing (fails closed on unparseable inserts on marker tables); evaluation of extracted branch conditions; whole-`src/` scans | The pinned text still has exactly the recorded chain. Commented-out or `#if 0` code never satisfies a contract. |
| **A′ — review-only comparison** | `ConfigNamespaceAudit` reads the `037c0141` checkout for key names, defaults and the clamp only | Documents the rename. **Not** evidence about the owner's installed revision. |
| **B — compiled extracted bodies** | The **verbatim** pinned bodies of `ResetQuests`, `GetQuestStatus`, `RemoveActiveQuest`, `RemoveRewardedQuest`, `SetRewardedQuest`, the rewarded loop of `_SaveQuestStatus`, Core `QuestFlags`, `Quest::HasFlag`/`IsRepeatable`/`IsSeasonal`, `Player::IsQuestRewarded` and IP `GetPlayerProgressionFromQuests`, compiled against minimal stand-ins (`tests/marker_reset_harness/harness_stubs.h`) with SQL-derived flags; 86 expectations | Those exact bodies, executed, wipe stages 1–18 to 0 and queue one DB delete per marker. Controls: stage 0, no-reset save, repeatable quest kept, PvP and ordinary collateral. |
| **Not covered** | Worldserver, a real DB, Playerbots scheduling and timing, the owner's installed revisions, configs, environment and SQL overrides, actual GM command use | — |

### Negative fixtures (harness sensitivity)

- **24 in-memory source mutations** of the real pinned text, each required to fail its named contract. These include the original 17 plus:
  - `initself=rare` made incremental;
  - an extra gate on `initself=uncommon`;
  - `ResetBot` made incremental;
  - the login hook ungated;
  - a new unconditional `SkipBotLevel` caller;
  - LevelBrackets code default ON;
  - the human XP cap moved off 60.
- **3 guarded-removal fixtures** (CS02-IP finding 4): braced `if (!quest->HasFlag(QUEST_FLAGS_TRACKING))`, braced `IP_STAGE_MARKER_FIRST/LAST` range, and a braceless guard. Each must fail with exactly `MSG_REMOVE_NOT_UNCONDITIONAL`. All three passed every rev-1 contract.
- **Fail-closed fixtures:**
  - `#if 0` and `#ifdef`-style wrappers inside extracted bodies;
  - a column-less `INSERT` on a marker row (detected) and on an unrelated row (not flagged);
  - `INSERT … SELECT` on a marker table (UNPARSEABLE finding);
  - a foreign `UPDATE … WHERE ID = 66004` (detected) and an unrelated-ID control (not flagged).
- **Tree-scan fixtures:**
  - a legacy `"AiPlayerbot.*"` option anywhere under PB `src/` (detected) versus one only in a comment (not flagged);
  - a partial PB `src/` checkout (fails closed);
  - IP code touching `CONFIG_MAX_PLAYER_LEVEL` (detected).
- **Layer B, 2 compiled mutations:**
  - `ResetQuests` skipping 66001–66018;
  - **`ResetQuests` guarding removal with `!quest->HasFlag(QUEST_FLAGS_TRACKING)`**. This compiles with the verbatim `HasFlag`/`QuestFlags` and fails *on behaviour*: markers (`Flags = 1024`) survive, while ordinary quests are still deleted.

## Reproduce locally

```bash
git clone https://github.com/mod-playerbots/mod-playerbots historical-playerbots
git -C historical-playerbots checkout e88cc4099e69f35126b28942a412ca702d20453b
git clone https://github.com/mod-playerbots/mod-playerbots legacy-playerbots-037c0141
git -C legacy-playerbots-037c0141 checkout 037c01418b5d01506917a3db9b44fd56ac5f965c
git clone https://github.com/Grimfeather/azerothcore-wotlk historical-core
git -C historical-core checkout 8daffdabe9202c228e45d833c9a1d399597a15cd
PINNED_PLAYERBOTS_ROOT=historical-playerbots PINNED_PLAYERBOTS_LEGACY_ROOT=legacy-playerbots-037c0141 \
  PINNED_CORE_ROOT=historical-core REQUIRE_COMPILED_HARNESS=1 REQUIRE_LEGACY_CONFIG_AUDIT=1 \
  python3 -I -m unittest discover -s tests -p 'test_ip_marker_reset_chain_source.py' -v
```

Sparse checkouts of the paths listed in the workflow are enough. The primary PB pin needs its whole `src/` tree. Pinned checkouts must be clean (`git status --porcelain` empty). Without `PINNED_PLAYERBOTS_LEGACY_ROOT` the namespace audit is skipped locally; CI requires it. Standalone use:

```bash
python3 tests/ip_marker_reset_extract.py check
python3 tests/ip_marker_reset_extract.py generate --out build/marker-reset   # add --mutation NAME for a negative build
g++ -std=c++17 -Wall -Wextra -Werror -I build/marker-reset -I tests/marker_reset_harness \
  tests/marker_reset_harness/harness_main.cpp -o build/marker-reset/harness && build/marker-reset/harness
```

## CI: two jobs (`contents: read`; no `pull_request_target`, secrets, merge or deploy)

| Job | Runs when | Purpose |
| --- | --- | --- |
| `pr17-frozen-scope-guard` | **only** for head branch `claude/azerothcore-ip-fork-hoqu8m` of this repo (PR or push); otherwise **skipped**, not red | The original 7-path allowlist against `edd639b0`, `src/`/`data/`/`conf/`/`optional/` byte-identical, append-only register |
| `marker-reset-chain` | any PR or push run touching the harness files, `src/IndividualProgression*`, `data/sql/**` or `optional/sql/**` | Pinned + legacy checkouts with SHA and clean-tree verification, the full unittest (g++ C++17), then clang++ C++17 and g++ C++20 builds and `check` |

**Which future PRs actually run the reusable job:** GitHub uses the workflow file from the PR's own head. So it runs only on PRs whose branch **contains this workflow**: branches stacked on PR #17, or any branch after an owner-approved merge of #17 into `develop/cosmic-ip`. Until then, PRs on other existing branches (#1, #3, #10, #13–#16) do **not** run it. A future PR that *intentionally* changes the chain (for example an approved C1 Option A) turns `marker-reset-chain` red **by design**, and must update the named expectations in the same reviewed PR.

## How to use it when C1 is decided

- **Owner picks a Playerbots-side preservation (Option A):**
  - Point `PINNED_PLAYERBOTS_ROOT` at the candidate PB fork.
  - `check_pb_reset_quests_removes_every_non_none_quest` (now exactly the five-statement loop) and the Layer-B stage sweep will fail exactly where markers are preserved.
  - Invert those expectations deliberately in that PR, and keep the controls.
  - Cover `initself` and the `RandomBotLevelMgr` routes too; they share `Randomize(false)`.
- **Owner picks an extra canonical stage record (Option B):** keep this harness as the proof of *why* the record exists, and add tests for the new store.
- **Upgrading Core, Playerbots or IP:** bump the pins in the workflow and module constants. Any drift surfaces as a named contract failure.

## Risks and limits

- **Not runtime.** The stand-ins (`ObjectMgr`, `CharacterDatabase`, `Player` containers) are minimal, and behaviour outside the extracted bodies is not exercised.
- **Text-based.** Contracts assert specific source shapes, so a pure upstream refactor will fail the harness. That is intended (re-verify, then update), but a red result is not automatically a regression.
- The IP SQL scan covers `data/sql` and `optional/sql` in this repository only. Live DB overrides and owner-local SQL are unknown.
- **Deferred (CS02-IP finding 6; not in this revision; neither occurs at the pins):**
  - `!` precedence in `cpp_condition_to_python` (`!a < b`). Unmodelled symbols are still rejected.
  - C++14 digit separators (`1'000`) in the literal skipper.

## Revision 2 changes

| CS01-IP item / CS02-IP finding | Change |
| --- | --- |
| 1 / F2 route inventory | `initself` × 6 contract; **new** `RandomBotLevelMgr` × 3 routes with default-OFF gates and call-site inventory; PB sparse checkout widened to `src/` |
| 2 / F1, F5 namespace + caps | `ConfigNamespaceAudit` against `037c0141`; `check_pb_no_legacy_config_namespace`; `check_ip_level_caps_are_xp_gates_not_world_cap`; exposure section rewritten |
| 3 / F4 mutation loophole | top-level five-statement loop contract with a specific failure message; 3 guarded-removal fixtures; compiled TRACKING-flag mutation |
| 4 / F3 reusable CI | workflow split into a PR17-only frozen guard and a reusable chain job; IP `src/` and SQL path triggers |
| 5 / F6, F7 (narrow subset) | clean-tree pin checks; fail closed on conditional preprocessor directives in extracted bodies and on unparseable or column-less marker-table inserts |

## Rollback

Delete the seven files listed in the `pr17-frozen-scope-guard` allowlist and remove the `IP-TEST-C1-001` register section(s). Nothing else references them, and no runtime, config or data change has to be reversed.
