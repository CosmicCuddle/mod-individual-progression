# CosmicCuddle — Grimfeather Individual Progression upstream integration matrix (2026-11-04)

**Owner:** CosmicCuddle. **Maintainer:** Server-Side-Maintenance (CS01-IP).  
**Status (Part 25):** Development-only two-parent source merge exists in [DRAFT #59](https://github.com/CosmicCuddle/mod-individual-progression/pull/59); **not merged to fork master, not installed, not gameplay-tested, and not approved for deployment**.  
**Current protected owner-game-test tuple:** Naxx #292 `51d0611476358286505aed3ca806ea250f644f66`, IP #46 `26d5b7a1de30ad92a595aaf251fc14754cf54f32`, Services #16 `b5d372eda5cd08b6a650c78a5faeb01ee1c3702d`, historical Grim Core `8daffdabe9202c228e45d833c9a1d399597a15cd`, Playerbots `e88cc4099e69f35126b28942a412ca702d20453b`.

## Exact refs / Git DAG

| Role | Immutable reference | Verified Git facts |
| --- | --- | --- |
| Custom baseline | `dd96f2b77e3ebfa2a4726bf785525ba133f1ab61` | common ancestor of latest Grim and custom #46 |
| Cosmic fork master before sync | `18999da65adcbb70c7f4b962afb3600534481ef6` | **97 commits behind** Grim master, zero ahead |
| Grim current IP master | `42c9a32892740af55a40e076e6381cc879181396` | 126 commits ahead of the custom baseline |
| Protected custom integrated IP | `26d5b7a1de30ad92a595aaf251fc14754cf54f32` | 222 custom commits ahead of common baseline; not merged into upstream |
| Upstream mirror proposal | [draft #57](https://github.com/CosmicCuddle/mod-individual-progression/pull/57) | unchanged upstream commit as branch; **NOT merged into fork master** |
| Custom integration workspace | [draft #58](https://github.com/CosmicCuddle/mod-individual-progression/pull/58) | new branch from protected custom #46; upstream PR is **not mergeable automatically** |
| Grim latest Core | `0d754d1921261dfd939f91060ebc022eae280f71` | **84 Core commits newer** than CI's historical pinned `8daffdabe`, 109 Core paths changed (74 src, 30 SQL, 5 other) |

**Do not mistake a fast-forward of Cosmic fork master for updating our substantial custom branch.** Upstream IP changed 27 paths since common baseline, while #46 changed 65; a combined tree must deliberately preserve both sets.

## Six directly overlapping paths: manual source merge REQUIRED

| Path | Scope to preserve / review | Merge test |
| --- | --- | --- |
| `conf/individualProgression.conf.dist` | Existing F14, paid entitlement, group policy, Hunter and security settings; import upstream additions without overwriting effective owner config; keep new AutoAttune 0/OFF | Compare all config keys and default semantics; no unapproved auto-enabled switch |
| `src/IndividualProgression.cpp` | Custom paid receipt progression transitions, group floor/GM handling, old hidden quest milestones; add upstream GrantAttunements gated by OFF default only after cross-path review | No stage grants, quest rewrites or attunement item mails when OFF; no bypass of paid gates |
| `src/IndividualProgression.h` | Custom receipt/identity/gm/group fields, enums, locks and functions; add only compatible upstream declarations | ABI/header source compile and branch local tests |
| `src/IndividualProgressionPlayer.cpp` | Custom login, protected F14 race/RandomBot/bot group logic, paid state, not overriding known stage; upstream autoattune starting-stage hooks only when OFF safe | Logins, race stage 8 floor, real/Alt/RND bots, login/relog; verify grant never fires while OFF |
| `src/IndividualProgressionSpells.cpp` | Approved Hunter same-pet autocast/rank reconciliation; upstream spell fixes | Hunter autocast ON and OFF, pet reload/owner separation, full Worldserver compile |
| `src/cs_individualProgression.cpp` | GM null-safety, owner manual `.ip` mutators, paid audit and historical `.ip attune` | GM + normal player permission matrix, no bypass/changed command security |

**Overlap counts are file-level, NOT a promise of exactly six Git conflict hunks.** Exact dry-run merge workflow is `.github/workflows/ip-grim-upstream-merge-conflict-audit-20261104.yml` on this isolated integration branch; it does **not** push, modify executable source, merge, or touch a host.

## Upstream-only file edits (21 paths — still semantic review REQUIRED)

- **World SQL (13):** `data/sql/world/base/av.sql`, `av_gameobjects.sql`, `cs_individualProgression.sql`, `dungeon_blackwing_lair.sql`, `dungeon_dire_maul.sql`, `dungeon_magisters_terrace.sql`, `dungeon_onyxia.sql`, `naxx40.sql`, `zone_borean_tundra.sql`, `zone_howling_fjord.sql`, `zone_ironforge.sql`, `zone_isle_of_queldanas.sql`, `zz_ipp_aware_npcs.sql`.
- **Optional SQL (2):** `optional/sql/world/zz_optional_custom_races.sql`, `zz_optional_vanilla_transports.sql`.
- **Runtime loader/scripts (6):** `src/IndividualProgression_loader.cpp`, `src/naxx40Scripts/custom_gameobjects_40.cpp`, `src/vanillaScripts/boss_chromaggus.cpp` (**removed by upstream**), `src/vanillaScripts/instance_onyxias_lair.cpp`, `src/vanillaScripts/quest_the_masquerade.cpp`, `src/wotlkScripts/zone_borean_tundra.cpp` (added).

**Critical existing feature protection — Chromaggus:** upstream removes the custom Time Lapse script/loader/DB binding, while [coordination #5](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/5) states the owner has NOT selected keep/remove/correct. This deletion is **not implicitly accepted** merely because the path does not overlap #46's direct edits. Preserve the established legacy rule pending a documented owner decision and regression test; never silently remove the feature.

**SQL safety:** separate read-only row-level diff and data snapshot. New world/optional SQL may contain deletes/replacements or progression-sensitive spawn/phase/quest/command permissions. No auto-apply to the owner's production world/auth/characters schema; verify migrations, actual owner config and reversible backup first.

## New upstream AutoAttune is NOT an approved gameplay rule

Latest Grim [#328](https://github.com/Grimfeather/mod-individual-progression/commit/42c9a32892740af55a40e076e6381cc879181396) adds `IndividualProgression.AutoAttune = 0`. When **enabled**, stage bumps can grant Drakefire Amulet, Tempest Key, Medallion, and mark several attunement quests rewarded; an already-active quest may be abandoned/removal-marked and its source item taken; full bags may cause mailed items with finite retention. It calls `GrantAttunements` from force stage writes and some login starting-stage paths. This intersects earned-vs-paid era [#18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18) and manual `.ip attune` [#14](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/14). **Default OFF must stay OFF, including installed effective config, until owner approves exact behavior.** [Information request #35](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/35) tracks policy only.

## Part 25 live development evidence — 4 November 2026

- [IP DRAFT #59](https://github.com/CosmicCuddle/mod-individual-progression/pull/59) contains two-parent development merge `39299ddf7ee83021ced057628e1f1ad70ddb7e8a`, with original Cosmic custom commit `26d5b7a1` and Grim upstream commit `42c9a328` as ancestors. The source-preservation test preserves all earlier custom config values, Chromaggus script/DB registration, GM command byte identity, and upstream-only source/SQL blobs. The **source-identity CI passed**; original full native Worldserver remained in progress at this checkpoint.
- Source-only CI correction `9cb4b037679d3de2e98ef373944aba20703e8c14` makes the **historical conflict detector** distinguish an unresolved base from a later candidate that contains both required histories. Its [exact-head audit](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41820475265) **COMPLETED SUCCESS**. This checks Git ancestry, not whether every resolved hunk behaves correctly. Original historical provenance tests remain intact and may fail when intentionally applied to newer upstream source; these are **not** silently considered PASS.
- Owner sequence: finish this upstream sync before completing [F14 PR #56](https://github.com/CosmicCuddle/mod-individual-progression/pull/56) review/integration. PR #56 itself now has **five of five source/native CI workflows SUCCESS** on exact head `6cf528c0`, but F14 model/refresh is still UNWIRED and OFF. No runtime enablement is authorised by CI.
- **Safety correction under development:** Grim's upstream `ProfessionTierCap=1` can remove above-tier skills/achievements/titles at login. Cosmic fork integration will preserve the feature **OFF by default** across config, C++ fallback and member initialization; ON needs explicit owner policy and a restorable character backup. This is independent of AutoAttune=0.
- Remaining: native Worldserver + latest-Core/PB cross-version evidence, full SQL/DBC migration review, separate in-game acceptance, owner-installed SHAs/config+backup evidence and explicit release approval.

## Release-gate order

1. **Source merge — DEVELOPMENT CANDIDATE PREPARED, REVIEW PENDING:** six shared paths were reconciled on the isolated #59 branch and authentic merge commit `39299ddf` has custom and Grim parents. Review exact source/SQL, preserved Chromaggus and owner-pending excluded-human hunk; validate merge CI and source contracts. Keep #57/#58/#59 DRAFT until reviewed. Do not replace protected #46 or live module. F14 #55/#56 remains separate and **FORCED OFF**.
2. **Compatibility:** establish requirements from latest Grim Core `0d754d` vs older pinned Core `8daffd` (84 commits, 74 C++ file changes) plus Playerbots API, Naxx #292 and Services #16. Avoid falsely claiming older CI builds prove new Core compatibility. Determine whether this IP upstream update requires a Core update (and any DB/client patches) before compilation.
3. **Regression:** compile the exact integrated Worldserver with all dependent modules on a disposable runner, then test Hunter rank/autocast ON/OFF, group invites/offline leaders, GM normal/player rights, F14 rules/default OFF, receipt and purchase OFF, quest/reward/AutoAttune OFF, Chromaggus/Onyxia and SQL-dependent content.
4. **Owner preflight:** verify actual installed Core/PB/IP/Naxx/Services Git SHAs, custom working-tree modifications, effective configs, world/characters/auth DB migrations and restorable source/config/binary/DB backups; no backup or installed baseline is established by GitHub. Protect unmerged first game test candidate.
5. **Owner authorisation:** only after tests and explicit CosmicCuddle permission choose dev/offline or protected production window, versioned install, feature toggles and rollback. A build is not gameplay evidence, a draft is not a release.

**Rollback now:** discard staging branches/PRs #57/#58 and remove this audit workflow; no live code/config/data changed. **Rollback of later install:** re-deploy exact prior binaries/source+config and restore carefully scoped DB snapshots if migrations/quest changes were applied; never roll back characters to stale state without owner-approved data protection.
