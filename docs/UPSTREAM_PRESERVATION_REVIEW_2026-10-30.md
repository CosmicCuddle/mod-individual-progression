# Grimfeather Individual Progression upstream delta — preservation gate

**Owner:** CosmicCuddle  
**Reviewed:** 30 October 2026  
**Status:** SOURCE/DIFF AUDIT ONLY, **NO upstream merge**, no database migration, no compile, no gameplay acceptance, no live deployment.  
**Protected local development:** `develop/cosmic-ip@edd639b0a6eba858830876e987def510450f438a`.  
**Previously recorded source baseline:** `dd96f2b77e3ebfa2a4726bf785525ba133f1ab61`.  
**Upstream-tracking mirror:** `CosmicCuddle/mod-individual-progression:master@18999da65adcbb70c7f4b962afb3600534481ef6`, verified same SHA as `Grimfeather/mod-individual-progression:master` when inspected.  
**Exact [GitHub source comparison](https://github.com/CosmicCuddle/mod-individual-progression/compare/edd639b0a6eba858830876e987def510450f438a...18999da65adcbb70c7f4b962afb3600534481ef6):** GitHub returned `diverged`: 29 upstream commits ahead, one local documentation merge behind; 11 paths differ. **The current owner's installed source SHA/config/data remain unverified.**

## 1. Decision: do NOT merge this upstream revision automatically

The upstream source contains a gameplay-significant **removal** of IP's Chromaggus Time Lapse threat-change script, loader registration, and BWL SQL binding. This is specifically **UNDECIDED** by CosmicCuddle in the existing [Chromaggus rule Issue #5](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/5). Information verified the upstream rationale [Grimfeather issue #295](https://github.com/Grimfeather/mod-individual-progression/issues/295): under some Time Lapse stun/threat transitions, temporary threat reduction may not restore correctly. The historical Core still includes its own Time Lapse target attack-ineligibility rule even if the IP supplemental threat script is removed. Neither retaining nor removing the IP script has been owner-approved as the permanent gameplay rule. Do not assume a newer commit is safe.

## 2. Exact 11 upstream-different paths and preservation review

| Upstream path | Observed change | Preservation/compatibility gate |
| --- | --- | --- |
| `src/vanillaScripts/boss_chromaggus.cpp` | **Deleted** the custom 23310 Time Lapse spell script | **HOLD: owner keep/remove/correct decision**; test threat restoration, tank/bot aggro, all-stunned evade and encounter finish before approving code removal |
| `src/IndividualProgression_loader.cpp` | Removed script declaration and `AddSC_boss_chromaggus_60_2_A()` | Same **HOLD**; avoid unresolved loader references if retaining/replacing script |
| `data/sql/world/base/dungeon_blackwing_lair.sql` | Deletes `spell_bwl_chromaggus_time_lapse` binding; no longer re-inserts it | Same **HOLD**; live SQL state and rollback must be documented; do not import this automatically |
| `data/sql/world/base/av.sql` | Two added `DELETE FROM creature_formations` GUID ranges for upgraded world DBs | **HIGH DATA IMPACT**: identify matching rows and owner custom AV formations in a *restored disposable DB* before considering migration; no live delete |
| `data/sql/world/base/dungeon_magisters_terrace.sql` | New 101-line TBC smart-script package, including deletions and inserts | New TBC-era content; check IP TBC gating, relevant Core schema and installed smart scripts; avoid applying in current Vanilla stage without staged plan |
| `data/sql/world/base/zone_howling_fjord.sql` | New two-line WotLK location correction for an existing gameobject GUID | WotLK/quest state compatibility; avoid unrelated production SQL import |
| `data/sql/world/base/zone_ironforge.sql` | Reworked Grand Mason Marblesten quest/ambient route and smart scripts | Current Vanilla content: potential custom waypoint/quest interactions; test owner current route on disposable staging and preserve existing quest completion |
| `data/sql/world/base/zone_isle_of_queldanas.sql` | Alters Emissary of Hate/Fel Portal Alarm spawning via `smart_scripts` replacement | TBC encounter and spawn/phase behaviour; compare installed data and Naxx-related scripts before importing |
| `data/sql/world/base/zz_ipp_aware_npcs.sql` | Adjusts NPC GUID lists across IP phase masks, adding 93954 and 94416 | IP phasing impacts; confirm DB spawn GUID and stage-sensitive visibility, no global character manipulation |
| `optional/sql/world/zz_optional_custom_races.sql` | Replaces previously inserted Blood Knight quest-template copies with a joined `UPDATE` of existing 109681/109684 rows | Potential **missing-row/optional SQL order** hazard; check row existence, human BE/Draenei quest behaviour and rollback, especially F14 race rules |
| `optional/sql/world/zz_optional_vanilla_transports.sql` | Adds Menethil dockmaster NPC change including `DELETE` and `INSERT` by GUID | Potential impact to existing owner custom dockmaster/spawn/transport; requires row-level staged diff and path/quest checks |

**Confirmed unchanged by this particular 11-file diff:** `src/IndividualProgressionSpells.cpp` (Hunter code location), `src/IndividualProgressionPlayer.cpp` (grouping/login location) and `src/IndividualProgression.cpp` (group sync and force-write location). This reduces **direct textual collision risk** with the currently drafted Hunter/group/F14 changes, but **does not** establish runtime compatibility, custom patch presence or safe SQL/state preservation. The fork's protected development revision includes one **docs-only** local divergence and is not equivalent to the active owner binary.

## 3. Protected custom IP modifications (do not drop)

- **IP-HUNTER-001:** Preserve native same-pet explicit ON/OFF across legal rank replacement; [Hunter DRAFT #1](https://github.com/CosmicCuddle/mod-individual-progression/pull/1) is unmerged. Owner reported positive initial Wolf Bite test; full ON/OFF/stable acceptance remains.
- **IP-GROUP-001:** `EnforceGroupRules` ON groups by Vanilla 0–7 / TBC 8–12 / WotLK 13–18, maintaining OFF legacy behaviour; [grouping DRAFT #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3) is unmerged; Playerbots direct-`AddMember` gate and offline party decisions still unresolved.
- **IP-F14-001:** Positive race-10/11 RandomBot stage-8 floor; protected saved stage quests on relog, no forced stage-8 unlock while Vanilla eligibility is unavailable. [research DRAFT #4](https://github.com/CosmicCuddle/mod-individual-progression/pull/4), [synthetic test DRAFT #6](https://github.com/CosmicCuddle/mod-individual-progression/pull/6). **Not implemented as runtime.**
- **IP-F14-002:** Hide only positively identified race-10/11 RandomBots from Vanilla observers, not humans/Altbots/Selfbots/Unknown; coordinate existing Naxx F14 identity/world visibility drafts. **Not implemented as runtime.**
- **All other installed custom changes:** inventory incomplete. Do not reset owner's source, discard dirty patches, change world DB, or infer active configs from GitHub.

## 4. Required safe integration procedure (not executed)

1. **Source identity:** record owner-installed Core, Playerbots, IP and Naxx module SHAs, actual dirty tracked/untracked changes and **compiled-binary provenance** when owner permits a single consolidated read-only inventory. Historical GitHub-pinned builds are not that evidence. Read actual effective IP config; the earlier supplied non-dist snapshot states `ProgressionLimit=0`, which does not mean Vanilla tier 7.
2. **Review commit-by-commit:** preserve owner custom behaviour and review all 11 upstream path diffs. Split safe upstream C++/SQL patches from blocked Chromaggus and other data changes; resolve each conflict with reviewable commits, never squash away the reasoning.
3. **Review data migrations:** SQL files in `data/sql/world/base` and `optional/sql/world` may delete/update existing world rows. These source files are **not** evidence of what is deployed or applied. Only consider migrations against disposable copies after backups/restoration are verified, row-level effects inspected and owner approves their scope.
4. **Historical isolated compile:** prove a full C++ Worldserver link with exactly the intended frozen Core/PB/IP/module source mixture, including Hunter/group changes where approved; verify executable, no launch/deployment. Test source and C++ semantics plus configured OFF paths.
5. **In-game regression before release:** Chromaggus Time Lapse/BWL gating; Hunter ON/OFF same-pet stable/relog; cross-era bot invitation/rejoin and offline member handling; F14 stage write safety and rndbot-only observer visibility; NPC phase masks, Ironforge quest script, AV formations and any future TBC SQL selected. Document all failures and rollback.
6. **Release gate:** CosmicCuddle explicitly approves exact merged code, SQL, config/DBC, restore-tested backups, feature toggles and deployment. Preserve known-good binaries and state, use staged rollback, then record actual live verification in the [Custom Modifications Register](CUSTOM_MODIFICATIONS_REGISTER.md).

## 5. Review-only branch integration / collision map (30 October 2026)

Verified GitHub PR changed-file lists and base branches; **this is NOT an instruction to merge them**.

| Open DRAFT | Base | Executable or documentation scope | Reconciliation requirement |
| --- | --- | --- | --- |
| [Hunter #1](https://github.com/CosmicCuddle/mod-individual-progression/pull/1) | `develop/cosmic-ip` | `src/IndividualProgressionSpells.cpp`; Hunter checks, README, review and modifications register | Preserve exact owner-tested C++; reconcile README and shared register before a separately approved merge |
| [Grouping #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3) | `develop/cosmic-ip` | `src/IndividualProgression.cpp`, `src/IndividualProgressionPlayer.cpp`, group policy, checks, register and audit | Keep exact-head compile frozen; direct Playerbots `Group::AddMember` bypass and offline party policy remain gated |
| [Grouping audit #5](https://github.com/CosmicCuddle/mod-individual-progression/pull/5) | Grouping **feature branch** | Two Markdown files: pre-add bypass audit and shared register | This is a *stacked child*, not a separate implementation; do not merge before resolving/rebasing parent #3 |
| [F14 research #4](https://github.com/CosmicCuddle/mod-individual-progression/pull/4) | `develop/cosmic-ip` | F14 source memo and shared register | Research only, no rndbot floor/visibility code; coordinate registered options without new core hook |
| [F14 synthetic tests #6](https://github.com/CosmicCuddle/mod-individual-progression/pull/6) | F14 **research branch** | Pure model/tests + isolated CI + QA memo | Stacked child; synthetic green is not F14 runtime, do not mistake model for hook implementation |
| [Upstream audit #7](https://github.com/CosmicCuddle/mod-individual-progression/pull/7) | `develop/cosmic-ip` | Two Markdown files only, including this ledger | Register update must be reconciled with any other draft; no executable source/SQL changed |

**Concrete collision warning:** `docs/CUSTOM_MODIFICATIONS_REGISTER.md` is changed in **#1, #3, #4, #5 and #7**. The shared register is a **single authoritative list**; a blind merge/rebase of divergent draft copies can silently drop entries. Before any owner-approved merge, compare all five register revisions and retain each custom feature, owner decision, test state, rollback notes and upstream risk. The Hunter and grouping drafts do **not** both modify the same C++ file in their current PR diffs, but later F14 runtime work would need to touch the existing grouping/login paths and can create semantic and textual conflicts. Exact installed Core/Playerbots compatibility is still unknown.

**Suggested later review sequence, not approved execution:** (a) freeze and verify each exact current draft, (b) approve individual game behaviour and restorable backup plans, (c) reconcile **all** register copies into one source-of-truth document, (d) integrate approved source changes into a fresh, reviewable candidate rather than overwrite either original branch, (e) run exact combined historical Worldserver link and owner-approved staging/gameplay/rollback, (f) only then consider any upstream SQL or Chromaggus decision. Keep each source branch intact for provenance.

## 6. What was and was not done

This audit is based on GitHub public source **commit comparison**, not remote-host files or a SQL SELECT. No source update, branch merge, script removal, SQL apply, config edit, game test, Worldserver build, backup claim, restart or deployment took place. All unmerged IP drafts remain isolated. The authorisation and recorded gameplay decisions are in Coordination [#4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4), [#5](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/5), [#9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), and [#10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10).
