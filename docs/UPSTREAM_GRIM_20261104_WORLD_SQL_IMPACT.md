# Grimfeather 42c9 SQL impact and protected-release gate — 4 November 2026

**Owner:** CosmicCuddle. **Scope:** Individual Progression upstream source merge [DRAFT #59](https://github.com/CosmicCuddle/mod-individual-progression/pull/59), custom IP #46; **no host or live DB accessed, queried, backed up or changed.** This is an impact register, **not executable SQL**. Core database updates (especially newer Grim Core `0d754d`) need a separate migration audit.

## Evidence and interpretation

Compare exact upstream history: `dd96f2b77e3ebfa2a4726bf785525ba133f1ab61` (common base) → Grim IP `42c9a32892740af55a40e076e6381cc879181396`. Upstream changes **15 module SQL paths**, 13 `data/sql/world/base` and 2 optional `optional/sql/world`. The development #59 tree imports upstream source SQL on those 15 paths **except** the Chromaggus deletion in `dungeon_blackwing_lair.sql`, intentionally held pending [coordination #5](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/5).

The operation inventory below counts **new text lines beginning with DML verbs in Git's added patch lines**. It is not SQL parsing, not an assurance about executed statement counts, not an actual DB diff or evidence of install.

| Upstream SQL path | Newly added DML / semantic difference | Main protection/acceptance |
| --- | --- | --- |
| `data/sql/world/base/av.sql` | Two added `DELETE creature_formations` ranges `@CGUID+640..700`, `@CGUID+733..744` | Compare resolved `@CGUID`, ownership and existing Alterac Valley formations; do not delete owner-custom formations |
| `data/sql/world/base/av_gameobjects.sql` | `UPDATE gameobject_template_addon` faction on 175404, 176643, 176645 | Check ore vein interaction/faction; protect custom gathering rules |
| `data/sql/world/base/cs_individualProgression.sql` | Changes `.ip set` and `.ip tele` command help to document optional player name; keeps SQL command permission overrides | Preserve owner commands/security in actual world `command` rows; compare with custom GM source and pending #37 |
| `data/sql/world/base/dungeon_blackwing_lair.sql` | Upstream removes supplemental Chromaggus Time Lapse registration; conflict resolved to **preserve Cosmic** | Owner `#5` keep/remove/correct undecided; test boss threat with humans and Playerbots |
| `data/sql/world/base/dungeon_dire_maul.sql` | Replaces a legacy SQL row/value | Compare exact patched row and owner override before migration |
| `data/sql/world/base/dungeon_magisters_terrace.sql` | **New file**, 3 `DELETE` SmartAI/conditions groups, 2 `INSERT` groups for entries 24683–24698 | Snapshot and compare `smart_scripts`, `conditions`, script module requirements; TBC dungeon should not affect current Vanilla progression |
| `data/sql/world/base/dungeon_onyxia.sql` | Replaces a row/value | Verify existing Onyxia raid behavior and custom attunement, not auto-overwrite |
| `data/sql/world/base/naxx40.sql` | Replaces a row/value | Check protected Naxxramas-Core #292 and boss/quest links |
| `data/sql/world/base/zone_borean_tundra.sql` | **New file**, `UPDATE creature` GUID108032 / NPC25478 ScriptName | Requires new `zone_borean_tundra.cpp` loader registration and verified world row; no initial Vanilla use |
| `data/sql/world/base/zone_howling_fjord.sql` | **New file**, NPC, SmartAI, GO and creature deletion/replacement; unit flags, loot chance and positions altered; Juniper/Black Conrad and related records | High collision risk: world/custom spawns, quest completion, GUID ownership and DB row snapshots before apply; do not replay blind |
| `data/sql/world/base/zone_ironforge.sql` | Extra SmartAI cleanup, `creature_template_addon`, `waypoint_data` row rewrites; one creature movement change (GUID100/NPC2790) | **Vanilla active area** — confirm custom guard/patrol/quest effects and retain tested NPC behavior |
| `data/sql/world/base/zone_isle_of_queldanas.sql` | SmartAI `DELETE` + `INSERT` for NPC23310 Fel Portal Alarm | TBC era script conditional on actual deployed SQL/area |
| `data/sql/world/base/zz_ipp_aware_npcs.sql` | Two changed NPC-related SQL values | Review stage visibility and behavior against F14/paid/group policy |
| `optional/sql/world/zz_optional_custom_races.sql` | Changes a Blood Knight quest-copy pattern; added `UPDATE quest_template` for 109681/109684 racial eligibility | Optional must **not** auto-apply; check Blood Elf/Draenei race availability, F14/RNDbots quest visibility and prerequisites |
| `optional/sql/world/zz_optional_vanilla_transports.sql` | Extra Menethil Dockmaster (GUID1550) replacement, gossip `MenuID66001` and `npc_text66001` delete/reinsert, related creature template update; broader transport/taxi changes are already in historical file | Optional, historically mutating transport routes and world spawns; confirm whether owner ever enabled original patch. Never replay on production without full world snapshot and tested travel routes |

## Release preflight — required before any SQL execution

1. Establish **actual installed** Core, IP, Playerbots, Naxx, Services source SHAs; locate SQL updater state and owner-added overrides. GitHub CI source is not installed source evidence.
2. Read actual **effective** `world`, `auth`, `characters` database versions and revision history; do not assume upstream base SQL is safe as an incremental patch. Check database schema/column case and key names against installed Core; compare IP base plus newer Core migrations, **avoid duplicate migrations**.
3. On a backed-up offline/test database, perform **read-only row-by-row comparisons** of touched GUIDs, entries, commands, SmartAI/conditions, quests, formations, transports, scripts, worldstates and new progress-sensitive NPC rows. Resolve conflicts with all modules, especially Naxx/Playerbots/Character Services and owner custom SQL.
4. Before executing any approved migration, verify restorable **world/auth/characters backups**, dated SQL plan, exact list of touched tables/rows, required downtime, rehearsed rollback, verification counts and separate owner approval. Prefer idempotent, scoped transformations to replaying full `base` files on existing production data.
5. Test gameplay on a disposable server after proposed migration: **Vanilla first** (Ironforge, AV, Onyxia, Naxx, Dire Maul, Chromaggus), then staged TBC/WotLK access/quests/transport/instance scripts. Confirm F14, bot visibility, GM security, and paid feature toggles OFF unless independently approved. A Worldserver compile does not validate SQL.
6. **Rollback:** restore only the appropriate approved DB snapshot/rows and exact prior source/config/binaries; characters/accounts require special care not to discard legitimate new progress. Do not treat a Git branch reset as a database rollback.

**Status:** Source tree contains upstream SQL where permitted, but **zero row-level owner-DB checks, real SQL migrations, owner backups or gameplay checks have been performed**. All optional patches remain opt-in. This document intentionally contains no executable `DELETE`/`UPDATE` statements.
