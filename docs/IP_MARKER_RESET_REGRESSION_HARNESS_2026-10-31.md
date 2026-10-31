# IP progression-marker reset regression harness (C1) — 2026-10-31

**Project owner:** CosmicCuddle. Review and testing are coordinated in [Naxxramas-Server-Coordination Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13); the C1 design decision stays in [Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9).
**Type:** TEST-ONLY. No change to `src/`, `data/`, `conf/`, `optional/`, Playerbots, the character DB, F14 or any live server.
**Base:** `develop/cosmic-ip` @ `edd639b0a6eba858830876e987def510450f438a`.
**Register entry:** `IP-TEST-C1-001`.
**Revision 2** (same date): addresses the [CS02-IP review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6543427658) as scoped by [CS01-IP](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6543457181). See [Revision 2 changes](#revision-2-changes).
**Revision 3** (same date): addresses the [CS02-IP rev-2 re-review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6544136049) as scoped by [CS01-IP](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6544205778). See [Revision 3 changes](#revision-3-changes).
**Revision 4** (same date): addresses the [CS02-IP rev-3 re-review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6544624580) (N1–N4) and the [`drop` finding](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6545502850), as scoped by [CS01-IP](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6544844601). See [Revision 4 changes](#revision-4-changes).
**Revision 5** (same date): closes CS02-IP rev-4 findings F1/F2 and self-review S1–S3, as scoped by [CS01-IP](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6546542325). See [Revision 5 changes](#revision-5-changes).
**Revision 6** (same date): closes CS02-IP rev-5 findings G1/G2, as scoped by [CS01-IP](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6547206443). See [Revision 6 changes](#revision-6-changes).

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
   - **AddClass-only routes** (the owner reports no AddClass use; unverified):
     - The bot `init=*` variants in `PlayerbotHolder::ProcessBotCommand` (defined at L722; `Randomize(false)` at L815–857) sit behind the L782 "You can only use this command on addclass bots" return.
     - The bot `random` command (L883–887) also sits behind that return. It calls `sRandomPlayerbotMgr.Randomize(bot)`, so the trigger matrix above decides whether it is a full reset.
     - **Automatic after Core login:** `PlayerbotHolder::OnBotLogin` L639–650 fully randomises when the account is AddClass (`IsAccountType(accountId, 2)`), a master exists, and `abs(master level − bot level) > 3`. Contract: `check_pb_addclass_login_full_reset_route`, plus a derived matrix (type 2 with gap 5 or −10 → full reset; gap 3, no master, type 1 or type 0 → no).
   - **Tree-wide inventory:** `check_pb_tree_wide_full_reset_call_inventory` maps every call of `.Randomize(false)`, unqualified `Randomize(<player>)`, `sRandomPlayerbotMgr.Randomize(`, `RandomizeFirst(`, `RandomizeMin(` and `ClearEverything(` across all 1,423 PB `src/` files to (file, enclosing function). The multiset must equal a frozen table, which includes header declarations on purpose. A new caller in **any** file, or a removed one, turns it red. Both `.` and `->` call forms are counted (revision 4).
   - **Non-reset deletion route — master `drop <quest link>`** (`src/Ai/Base/Actions/DropQuestAction.cpp`, `DropQuestAction::Execute`).
     - Control flow: the quest ID is parsed from the link, and the log-slot loop only overrides `entry` on a match. If no slot matches, `entry` stays non-zero, and `if (!entry) return false;` is followed by an **unconditional `bot->RemoveRewardedQuest(entry)`**. So a rewarded marker with no log slot is removed **without any reset**.
     - The removal is ID-agnostic, so it reaches stage markers 66001–66018 and PvP rank markers 66101–66114 alike; they are tracked separately, and no preservation policy is implied.
     - Contract: `check_pb_drop_quest_removes_rewarded_marker_without_log_slot`.
     - [SRC] control flow only. Whether the chat layer accepts a crafted hidden-quest link, and who can send it, is [UNTESTED].
     - A `ResetQuests()`-only protection would miss this path.
   - **Rewarded-removal caller inventory:** `check_pb_rewarded_removal_caller_inventory` freezes every `RemoveRewardedQuest(` call across PB `src/`:
     - `PlayerbotFactory::ResetQuests`;
     - `DropQuestAction::Execute`;
     - `CleanQuestLogAction::Execute` and `CleanQuestLogAction::DropQuestType`. These two only touch quests that occupy a log slot, per CS02-IP; not separately contracted.

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
| **A — source contract** | 23 `check_*` contracts in `tests/ip_marker_reset_extract.py`. Comment-aware C++ function and enum extraction (fails closed on conditional preprocessor directives inside an extracted body); top-level statement splitting; call-site inventory; a marker-table SQL static gate (see [SQL static gate](#sql-static-gate-revision-3)); evaluation of extracted branch conditions; whole-`src/` scans | The pinned text still has exactly the recorded chain. Commented-out or `#if 0` code never satisfies a contract. |
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
- **Revision 3 fixtures:**
  - 9 SQL static-gate negatives, each asserting its reason text:
    - `UPDATE` without `WHERE`;
    - `ID > 66000`;
    - `ID IN (SELECT …)`;
    - lower-case `id` insert;
    - `DELETE … WHERE QuestSortID = 0`;
    - `TRUNCATE`;
    - `WHERE` only inside a string literal;
    - a multi-table `JOIN` update writing `Flags`;
    - an insert without an ID column.
  - 6 SQL controls that must stay clean: `ID = 9999`, PvP `BETWEEN 66101 AND 66114`, PvP `IN (66101, 66102)`, the real broad `RewardFactionOverride1` baseline statement, a marker table that is only read in a sub-select, and `NOT … BETWEEN 66001 AND 66018`.
  - `ResetQuests` early return and post-loop restore, each rejected with exactly `MSG_RESET_FUNCTION_CHANGED`.
  - The `initself` contract tolerates a variable rename but still counts any extra factory built on `master`.
  - Tree-wide inventory: a new caller in another file and a removed caller are detected; a caller only inside a comment is not counted.
  - AddClass login condition: a changed threshold or account type is detected, and the 6-row outcome matrix is evaluated.
- **Revision 4 fixtures:**
  - 4 same-file dependency negatives, each also proven invisible to the per-statement gate:
    - `quest_template` `RewardFactionOverride1=123` → `Flags=5120`;
    - `quest_template_addon` `MaxLevel=77` → `SpecialFlags=0`;
    - a three-step chain;
    - a dependent `DELETE`.
  - A **cross-file** split, where each file alone is clean and the tree is flagged with both file names.
  - 5 negatives for N2/N4a: executable comment, `CREATE TRIGGER`, `PREPARE`, `SET ID = 66001`, `SET ID = ID + 1`.
  - 6 controls: the broad baseline update plus an ID-9999 chain write; mutators limited to ID 9999 or PvP IDs; `SET ID = 66102`; an ordinary comment; "Create"/"prepare" inside quest text.
  - Pointer-style `f->Randomize(false)` and `sRandomPlayerbotMgr->Randomize(b)` in a new file are detected.
  - A new `RemoveRewardedQuest` caller is detected.
  - 3 `drop` control-flow mutants, each failing with `MSG_DROP_QUEST_CHANGED`: the removal call dropped; `entry` zeroed when no slot matches; a stage-marker exemption added.
- **Revision 5 fixtures:**
  - 9 negatives with asserted reasons: `LOAD DATA`, `LOAD XML`, `WITH … UPDATE` with and without a sub-select, ODKU `ID = 66001`, ODKU `ID = ID + 1`, ODKU chain column, ANSI-quoted `UPDATE`, ANSI-quoted `INSERT`.
  - 9 controls: `SET @var := (SELECT …)` (a real baseline form), `SELECT`, `HANDLER`, `EXPLAIN`, a marker name only inside a string (twice), a non-chain ODKU with `VALUES(c)`, a plain unrelated insert, a PvP insert.
- **Revision 6 fixtures:**
  - 10 negatives with asserted reasons: Grok's 5 G1 statements, plus `"world"."quest_template"`, and 4 G2 statements, including `EXPLAIN UPDATE` and `EXPLAIN FORMAT=TREE ANALYZE DELETE`.
  - 6 controls: `EXPLAIN SELECT`, `EXPLAIN UPDATE` of a non-marker table, `DESCRIBE`, a single-quoted marker name, double-quoted quest text, and a read-only `SELECT … FROM "quest_template"`.
  - A baseline test bounds G1 to marker-name tokens.
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

## SQL static gate (revision 3)

`check_no_other_ip_sql_redefines_markers` applies a **narrow static gate** to every SQL file under `data/sql` and `optional/sql` except `player_progression.sql`. It is **not a SQL engine and not a game or DB test.**

- **Tokens are quote-aware.** A `WHERE` inside a string literal is never treated as a clause.
- **Chain-relevant columns** (case-insensitive) are `ID`, `Flags` and `QuestSortID` on `quest_template`, and `ID` and `SpecialFlags` on `quest_template_addon`. These are the only columns the contracts depend on.
- **`UPDATE`:**
  - If the `SET` list writes **no** chain-relevant column, it passes. This is the documented allowlist. It is needed because the real baseline statement `UPDATE quest_template SET RewardFactionOverride1 = 0 WHERE RewardFactionId1=0` matches all 18 marker rows.
  - Otherwise the `WHERE` clause is evaluated with three-valued logic against the **actual 18 stage-marker rows parsed from `player_progression.sql`**.
  - The grammar is limited to columns, integer and string literals, `= != <> < > <= >=`, `[NOT] IN (literal list)`, `[NOT] BETWEEN … AND …`, `IS [NOT] NULL`, `AND`/`OR`/`NOT` and parentheses.
  - It reports a finding for: no `WHERE`; any marker row TRUE; anything undecidable (sub-query, other operator or unknown column), which fails closed; or a multi-table/`JOIN` update that writes a chain column.
- **`DELETE`:** the same `WHERE` evaluation applies, and there is no column allowlist. Multi-table forms fail closed.
- **`INSERT`/`REPLACE`:** the ID column is matched case-insensitively.
  - A literal stage ID is a finding.
  - A non-literal ID, a missing ID column, `INSERT … SELECT` or unreadable rows fail closed.
  - Column-less `VALUES` use ID in column 0.
- **`TRUNCATE`, `DROP`, `ALTER` and `RENAME`** on a marker table are findings.
- **Stage markers only.** PvP markers 66101–66114 are intentionally *not* frozen, so legitimate PvP changes stay allowed.
- **Revision 4 — cross-statement and cross-file dependencies** (`marker_sql_dependency_findings`, CS02-IP N1). This is a whole-tree, two-pass, order-insensitive analysis, not statement execution.
  - **Pass A (fixpoint).** An `UPDATE` on a marker table *may touch stage-marker rows* if its `WHERE`:
    - is absent;
    - is TRUE or undecidable for any of the 18 rows; or
    - references a column that **another** touching `UPDATE` writes.

    Its `SET` columns become *mutable* for that table.
  - **Pass B.** A chain-writing `UPDATE`, or any `DELETE`, whose `WHERE` references a column that another touching `UPDATE` writes is a finding. The reason text names the column and the first mutating file and statement, plus the count of others.
  - Same-file, cross-file and multi-step chains are covered.
- **Mutable columns in the real baseline:** `quest_template.RewardFactionOverride1..5` (`vanilla_quest_reputations.sql`) and `AllowableRaces` (`zz_optional_custom_races.sql`; its `& N` predicates are undecidable, so they are treated as touching). None is a chain column, so the baseline stays clean. **A future chain write filtering on one of these columns fails closed by design**, even where values would not actually match. This is a deliberate false positive, asserted by `test_baseline_mutable_columns_are_documented`.
- **Revision 4 — other fail-closed rules:**
  - `SET ID = <stage literal>` is a finding regardless of `WHERE` (N4a), and `SET ID = <expression>` fails closed. Literal non-stage or PvP IDs stay clean.
  - MySQL executable comments `/*! … */` naming a marker table are findings (N2). Ordinary comments are ignored.
  - Statements containing the keywords `CREATE`, `PREPARE`, `TRIGGER` or `PROCEDURE` (outside string literals) that name a marker table anywhere, including inside a string, are findings.
- **Revision 5 — verb allowlist, ODKU and quoted identifiers:**
  - A statement that names a marker table **as an identifier** is analysed only if its verb is modelled: `INSERT`/`REPLACE`/`UPDATE`/`DELETE`, the DDL rule, or the stored/dynamic SQL rule.
  - The read-only verbs `SELECT`, `SET` (variable assignment), `SHOW`, `HANDLER` and `DESCRIBE`/`DESC`/`EXPLAIN` pass.
  - **Every other verb fails closed** with `unsupported statement form '<VERB>' …`. This covers `LOAD DATA`, `LOAD XML`, a leading `WITH … UPDATE` (a read-only `WITH … SELECT` also fails closed, conservatively), and any future syntax. Mentions only inside strings or comments are not identifiers and stay clean.
  - `INSERT`/`REPLACE … ON DUPLICATE KEY UPDATE` on a marker table:
    - assigning `ID` a stage literal or an expression is a finding;
    - assigning any other chain column fails closed, because ODKU rewrites whichever row already holds the key;
    - rows are parsed only from the part before ODKU, so `c = VALUES(c)` no longer raises a parser error.
  - **Double-quoted marker-table names (revision 6, G1):** under `sql_mode=ANSI_QUOTES` a double-quoted token is an identifier, and the owner's effective SQL mode is unverified.
    - A double-quoted token whose content **is** a marker-table name, or whose last dotted segment is one (`"world"."quest_template"`), fails closed **anywhere** in a statement whose verb is not read-only: JOIN alias, `DELETE q FROM`, `LOAD`, `TRUNCATE`, `RENAME` and so on.
    - Ordinary double-quoted text values never match. The real tree has 74 `UPDATE`s with double-quoted quest text and 0 double-quoted marker names, which is asserted.
    - **Accepted conservative false positive:** a write statement whose double-quoted *value* is literally `"quest_template"` or `"quest_template_addon"` fails closed.
  - **`EXPLAIN`/`DESCRIBE`/`DESC` (revision 6, G2):** these fail closed when followed (after an optional `FORMAT=…`) by `ANALYZE` or a write verb in a statement naming a marker table. The vendor and version (MySQL 8 vs MariaDB) are unverified and are not inferred. `EXPLAIN SELECT` and `DESCRIBE <table>` stay clean.
- **Known conservative false positive (CS02-IP F3, accepted):** Pass B flags a chain write whose `WHERE` references a baseline-mutable column even when another conjunct pins a non-stage row (e.g. `WHERE ID = 12345 AND AllowableRaces = 1`). Narrowing conjuncts are not used to clear a dependency.
- **Limits and non-guarantees (CS02-IP F4):**
  - Statement **values** are not modelled, only which columns may change.
  - Dynamic SQL **built** without naming a marker table literally (e.g. `CONCAT('UPDATE quest_', 'template …')`, hex-encoded names) is invisible.
  - So are `CALL` of a procedure defined elsewhere and writes through a **view**. That is why the analysis is conservative.
  - `CALL` of a procedure defined outside these files, `EXECUTE` of a statement prepared elsewhere, and dynamic SQL that never names a marker table cannot be seen.
  - Only IP's `data/sql/**` and `optional/sql/**` are inputs. Core custom SQL, other modules, manual overlays and live DB history are **not** audited.
  - Live-DB or owner-local SQL is out of scope.
  - MySQL string/number coercion is treated as undecidable, which fails closed.
  - The baseline IP tree produces **0 findings**.

## CI: two jobs (`contents: read`; no `pull_request_target`, secrets, merge or deploy)

| Job | Runs when | Purpose |
| --- | --- | --- |
| `pr17-frozen-scope-guard` | **only** for head branch `claude/azerothcore-ip-fork-hoqu8m` of this repo (PR or push); otherwise **skipped**, not red | The original 7-path allowlist against `edd639b0`, `src/`/`data/`/`conf/`/`optional/` byte-identical, append-only register |
| `marker-reset-chain` | pull requests touching the harness files, `src/**`, `conf/**`, `data/sql/**` or `optional/sql/**`; pushes with the same paths to `claude/azerothcore-ip-fork-hoqu8m` or **`develop/cosmic-ip`**; manual dispatch | Pinned + legacy checkouts with SHA and clean-tree verification, the full unittest (g++ C++17), then clang++ C++17 and g++ C++20 builds and `check` |

**Not a required check.** Nothing here configures GitHub branch protection. Whether `marker-reset-chain` becomes a *required* status check on `develop/cosmic-ip` is a separate owner decision, and the current branch-protection settings are **unverified**. Until then a red run is a signal, not a merge block.

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
- **Text-based.** Contracts assert specific source shapes, so a pure upstream refactor will fail the harness. That is intended (re-verify, then update), but a red result is not automatically a regression. For example, rewriting the `ResetQuests` iterator loop as a range-`for` reports `MSG_REMOVE_NOT_UNCONDITIONAL`, `MSG_RESET_FUNCTION_CHANGED` or an extraction error even though behaviour is unchanged.
- **Inventories are pin-specific.** The tree-wide call table and the per-file inventories are frozen for `e88cc409`. Bumping the PB pin requires re-deriving them, which is deliberate.
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

## Revision 3 changes

| CS01-IP priority / CS02-IP rev-2 finding | Change |
| --- | --- |
| P1 / Medium CI coverage | Triggers widened to `src/**` and `conf/**` (plus `data/sql/**`, `optional/sql/**`, harness); `develop/cosmic-ip` added to `push.branches`. The frozen PR17 guard is unchanged and skipped elsewhere. The comments and memo now state the real coverage and that this is not a required check. |
| P2 / Medium SQL false negative | Quote-aware SQL static gate: chain-column allowlist, three-valued `WHERE` evaluation against the real marker rows, case-insensitive ID, fail-closed DDL, sub-query, multi-table and non-literal cases; 9 negatives plus 6 controls |
| P3 / docs | Role-neutral header crediting the project owner; corrected CI trigger claims; `random` command (L883–887) and AddClass routes added to the inventory |
| Low / route inventory | Tree-wide (file, enclosing function, callee) multiset over all PB `src/`; AddClass `OnBotLogin` condition contract with an outcome matrix; `initself` regex tolerates a variable rename |
| Low / outside-loop changes | `ResetQuests` function-level top-level statements pinned (`MSG_RESET_FUNCTION_CHANGED`) |

## Revision 4 changes

| CS01-IP item / CS02-IP finding | Change |
| --- | --- |
| P1 / N1 (Medium) multi-statement and cross-file SQL dependency | Whole-tree two-pass fixpoint dependency analysis (fail closed); 4 same-file and 1 cross-file negatives, 6 controls; real-baseline mutable columns documented and asserted |
| P2 / N2 executable comments, stored/dynamic SQL | `/*! … */` naming a marker table, and `CREATE`/`PREPARE`/`TRIGGER`/`PROCEDURE` statements naming one, are findings; ordinary comments and quest text stay clean |
| P2 / N3 pointer-style calls | Tree-wide inventory counts `.` and `->` forms; frozen baseline unchanged |
| P2 / N4a `SET ID = stage literal` | Finding regardless of `WHERE`; `SET ID = <expression>` fails closed; PvP and other literal IDs stay clean |
| Optional / `drop` route | `RemoveRewardedQuest` caller inventory (4 sites) and `DropQuestAction::Execute` control-flow contract with an ID-agnostic stage/PvP mapping |
| P2 / N4b PR description | Refreshed on GitHub after the exact-head runs (not a repository file) |

## Revision 5 changes

| CS01-IP priority / finding | Change |
| --- | --- |
| P1 / F1 + S1/S2 unmodelled verbs | Read-only verb allowlist; every other verb naming a marker table fails closed (`LOAD DATA/XML`, `WITH … UPDATE`, future forms) |
| P2 / F2 + ODKU parser error | ODKU assignments checked (`ID` stage literal or expression, other chain columns); rows parsed before ODKU, so `VALUES(c)` no longer errors |
| P3 / S3 ANSI_QUOTES | A double-quoted marker-table write target fails closed; string literals stay clean |
| F3 / F4 | Documented as an accepted conservative false positive and as explicit non-guarantees |

## Revision 6 changes

| CS01-IP item / finding | Change |
| --- | --- |
| G1 double-quoted marker names beyond the first target | Any double-quoted token equal to a marker-table name (or last dotted segment) fails closed in non-read-only statements; the rev-5 first-target check is subsumed; `.` added as an operator token so qualified quoted names are analysed by design |
| G2 `EXPLAIN ANALYZE` / `EXPLAIN <write>` | Fail closed when the statement names a marker table; vendor/version not inferred |

## Rollback

Delete the seven files listed in the `pr17-frozen-scope-guard` allowlist and remove the `IP-TEST-C1-001` register section(s). Nothing else references them, and no runtime, config or data change has to be reversed.
