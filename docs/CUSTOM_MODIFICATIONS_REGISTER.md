# Custom Modifications Register — Individual Progression fork

**Owner:** CosmicCuddle  
**Scope:** Custom source changes and planned IP-owned features in CosmicCuddle's Individual Progression fork, with external module dependencies recorded separately. This is **not** a complete inventory of the installed server.  
**Last reviewed:** 2026-10-30  
**Authority:** CosmicCuddle's approved gameplay rules; coordination Issues are the two-way decision log.  
**Operating rule:** Update this register for every proposed or implemented IP customization. Review it and actual owner source before upstream integration. Keep retired entries, and distinguish requested behaviour, committed source, passing builds, owner-reported game tests, verified live binaries and merged releases.

## IP-GROUP-SAFETY-002-INTEGRATION — Offline-leader no-stage0 port to combined Hunter/grouping (DRAFT)

| Field | Record |
| --- | --- |
| Intended behaviour | On combined Hunter+grouping candidate `OnPlayerCanGroupAccept`, never treat unavailable/offline group leader as legitimate stage0. OFF → allow join with no guessed progression write; ON → fail closed with no write. Real online-leader rules remain exactly as #10. |
| Owner/coordination | CosmicCuddle; CS02 review C3 [Issue #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11); Information grouping decision [Issue #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10). Independently developed original-code fix in [DRAFT #15](https://github.com/CosmicCuddle/mod-individual-progression/pull/15), this is a **semantic integration port**, not replacement. |
| Owning component/branch | `CosmicCuddle/mod-individual-progression` branch `integration/hunter-grouping-offline-leader-guard-20261031`, stacked on original combined Hunter/grouping [DRAFT #10](https://github.com/CosmicCuddle/mod-individual-progression/pull/10) exact `d3688e243a24441438eaff7b3488b05f41614deb`. |
| Executable source | Only `src/IndividualProgressionPlayer.cpp` `OnPlayerCanGroupAccept`: check leader non-null/in-world before stage access. Preserve Hunter `IndividualProgressionSpells.cpp`, grouping `IndividualProgression.cpp`, group policy and GroupScript. |
| Supporting source | [Detailed review/rollback](IP_COMBINED_HUNTER_GROUP_OFFLINE_LEADER_PORT_2026-10-31.md), `tests/test_ip_combined_offline_leader_guard.py`, strict integration source/static-C++ CI. |
| Config / SQL / DBC / client | No new switches, SQL/DB schema, DBC or client patches; only existing `IndividualProgression.Enable` and `EnforceGroupRules`. |
| Risks | The pinned PB accept action checks for an online leader, so this mainly addresses excluded-account or alternate-accept-path edge cases; some legitimate online-guild/offline-leader group acceptance remains fail-closed with enforcement ON; special Core group types, direct Playerbots `AddMember` bypass, normal online-leader downgrades, F14 stage8+ floor and PB `ResetQuests` outside scope. Actual owner installed versions and config unknown. |
| CI / runtime status | Dedicated child source/policy checks succeeded at previous head `55b2fbfc` ([run #41196812266](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41196812266)); new six-file exact-head check must be rerun. Inherited #10 **frozen exact-five-file step** is branch-scoped to original #10 only; Hunter/group regression steps run on all relevant child PRs. #16 separately verifies original Hunter/engine/policy blobs, source and step-only scoping. At any approved merge of #16 into #10, rebaseline #10's frozen path/source assertion under review; it will otherwise correctly fail. Parent combined Worldserver pinned compile known successful; this updated executable source has **not** been newly proven compiled/linked or tested in game. Owner explicit test approval required after actual backups/source match. |
| Merge / rollback | Do **not** blindly merge this with #3/#10/#15 or overwrite the earlier source/register; reconcile all tested custom changes via a later approved combined release. Branch revert for source; character/group DB snapshots for lost stage markers. No live source, binaries, data or server changed. |

## IP-HUNTER-001 — Preserve same-pet Hunter autocast across rank reconciliation

| Field | Record |
| --- | --- |
| Feature | Retain native Hunter pet autocast ON or OFF during legal *same-loaded-pet / same-ability-chain* rank replacement by Individual Progression |
| Intended behaviour | Keep prior native ON/OFF and valid action-bar placement; preserve IP's family/progression rules; no transfer between pets or nonmatching spells; preserve native default for freshly learned spells |
| Decision / authority | [CosmicCuddle's explicit 2026-10-30 approval](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/3#issuecomment-6524119930) for **this narrow behaviour**, not A02 stored-preference replay or permission to deploy |
| Owning component | CosmicCuddle **Individual Progression fork**, Hunter spell reconciliation and PetScript/PlayerScript callbacks |
| Proposed branch | `fix/hunter-autocast-rank-continuity-20261030` based on `develop/cosmic-ip` |
| Protected source ref | `baseline/installed-ip-dd96f2b-2026-10-30` at historical IP commit `dd96f2b77e3ebfa2a4726bf785525ba133f1ab61` (name is a historical snapshot; **actual running IP revision unverified**) |
| Review PR | [DRAFT PR #1](https://github.com/CosmicCuddle/mod-individual-progression/pull/1), **OPEN / UNMERGED** as at this review |
| Affected implementation file | `src/IndividualProgressionSpells.cpp`: Hunter `reconcile` path, bounded delayed-pet event identity check, Hunter-only post-loading recovery |
| Supporting files | Hunter review document, source regression checker, static CI workflow and full-build CI workflows in DRAFT PR #1 |
| Core APIs | `Pet::m_spells`, `m_autospells`, `ToggleAutocast`, `unlearnSpell`, `learnSpell`, `CharmInfo` action bar, pet GUID/CustomData, `PlayerScript::OnPlayerAfterUpdate`. Historical [core ref](https://github.com/Grimfeather/azerothcore-wotlk/tree/b7f06a16ac7120b5abc2e8d3f96a94265a363224) only; verify installed implementation |
| Playerbots interactions | Existing `isBotAccount` exclusion is retained; verify exact installed Playerbots/core and that updates do not add bot regressions |
| Config dependencies | Existing `IndividualProgression.Enable`, VanillaHunterPets and individual progression gates; **no new config key proposed** |
| SQL / database | Existing native `pet_spell` autocast state and action bar persistence; **no SQL/schema changes proposed**. Real schema and loaded settings not inspected |
| DBC / client-patch | No new patch proposed; actual installed spell-chain/DBC data and WotLK client response must be verified |
| Risks | Unexpected rank chains; multiple/stale spell and autocast entries; action-bar ghost slot; packet timing; async pet-load races; prolonged loading; per-tick hook overhead; cross-module dependencies; owner core API drift |
| Tests completed | Source preservation checks; verified pinned native Worldserver [CI 41073281260](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41073281260) and owner-source-matched full Worldserver [CI 41084770943](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41084770943) both PASSED. On 2026-10-30 CosmicCuddle reported a successful **local** Worldserver build after updating Core/Playerbots and a working Wolf Bite Rank 8 in-game before Molten Core. Do not infer every autocast edge case passed. |
| Tests outstanding | Controlled autocast **ON and OFF across actual same-chain rank replacements**, stable swapping two distinct pets, relog/save, delayed pet loading, correct client bar packets and Playerbots/Warlock regression on the updated owner's environment. Initial Bite-rank correctness alone is not proof of every case. |
| Rollback prerequisites | Verified backup of source, binary, configuration and **test pet/character DB state** before gameplay tests; known-good version / restore exercise. Revert a draft branch before merge, or a separately approved release rollback if ever deployed |
| Live status | **Owner reports successful local build and positive initial in-game testing** after preparing installation, but the running executable/source fingerprint has not been independently verified. **OPEN DRAFT PR #1 UNMERGED.** Owner locally tested Hunter source commit f0044987cc2d6f0b73c1cd25d17012a26a764beb on branch hunter-autocast-test. |
| Blockers | [Coordination #3](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/3), [#4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4); remaining direct autocast acceptance cases, exact running-binary evidence and owner's approval before merge or future deployment |

### Never conflate with other features

- **P1:** restoring a persisted setting requires fresh post-combat display-only current-pet prompt and separate protected human apply click.
- **K1:** different pets—even with the same displayed name—retain separate settings. Runtime pet GUID or recycled PetNumber is not a durable identity/provenance guarantee.
- **D18:** in-combat prearmed Pet A→B protected execution remains forbidden.
- **A02/N-Core:** separate addon-side saved preference research is not delivered or authorised by this in-place IP rank continuity rule.
- Existing commented Cower and other `keepId=0` removal-only lines are not granted or transformed into learned spells.


## IP-GROUP-001 — Expansion-based EnforceGroupRules (EXPERIMENTAL DRAFT CODE)

| Field | Record |
| --- | --- |
| Intended behaviour | Keep the existing config switch: **0 = OFF** (original grouping and bot sync); **1 = ON** (same **effective expansion**, not exact raid tier). Vanilla stages **0–7**, TBC **8–12**, WotLK **13–18**; distinct raid tiers within a single expansion can group. Do not alter human characters' stages to make them eligible. |
| Race exception | When enforcement is ON, Blood Elves and Draenei cannot join Vanilla groups, including human characters; **humans are never concealed**. Whether this must also apply with enforcement OFF has not been separately selected. |
| Owner rule | CosmicCuddle's 2026-10-30 instruction, recorded in [Coordination #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10). Separate from **rndbot-only visibility and starting tier floor** in #9. |
| Branch / owning component | Isolated **`feature/expansion-group-rules-20261030`** development branch from `develop/cosmic-ip`, preserving Hunter DRAFT PR #1 separately. IP fork only. |
| Changed implementation files | **`src/IndividualProgressionGroupPolicy.h`** new pure policy; **`src/IndividualProgressionPlayer.cpp`** invitation and acceptance paths with **no progression quest mutation until successful member add when ON**; **`src/IndividualProgression.cpp`** group-add sync eligibility and recorded tier. Existing GroupScript::OnAddMember is reused. No Hunter spell files changed. |
| Source/test documentation | [Detailed review](EXPANSION_GROUP_RULES_REVIEW_2026-10-30.md), `tests/group_expansion_policy.cpp`, `tests/check_group_expansion_source.py`, `.github/workflows/ip-expansion-group-policy.yml`. Draft branch created after the documentation-only PR #2 merged into protected develop. |
| Source ref / dependencies | Recorded rewarded-quest stage defines expansion; `ProgressionLimit` disallows **over-cap stages** rather than reclassifying their expansion. Existing stage/race enum values are static-asserted. Group lookups use existing Core script hooks. Updated owner Core/Playerbots commits and Worldserver compile compatibility are not independently verified. |
| Config / SQL / DBC / client | **Existing** `IndividualProgression.EnforceGroupRules` only, 0/1; no added config key or config value changed. No SQL/schema, DBC, client patch or production modifications. |
| Direct group-add bypass (BLOCKER) | [Source-backed audit](EXPANSION_GROUP_ADDMEMBER_BYPASS_AUDIT_2026-10-30.md): pinned Playerbots `GroupInviteOperation`, arena group creation and `BattleGroundJoinAction` invoke **`Group::AddMember` directly**, bypassing `OnPlayerCanGroupInvite` and `OnPlayerCanGroupAccept`. Current historical Core `Group::AddMember` offers no verified *pre-insert* IP veto; post-add callback cannot guarantee prevention. This DRAFT **does not enforce all bot-created groups** and must NOT merge as a global guarantee. Confirm owner Core/Playerbots versions, coordinate minimal approved Core seam or exhaustive Playerbots guard, then native build and game test. |
| Known incomplete dependency | F14 positive rndbot race-10/11 **tier-8 starting floor is NOT enforced by this grouping patch**. Its pure predicate is intentionally **unwired** pending verified RandomBot identity, spawn/login handling and cap safety. With enforcement OFF, legacy group sync remains including the tier-0 skip. When ON, online bot members **can now sync to a pre-MC stage-0 leader** and identical-stage no-op sync writes are skipped; this needs game validation. Also when ON, offline group members are handled **fail-closed** via persisted Group::GetMemberSlots (no DB quest reads); this can block inviting new players while any existing member is offline and needs owner acceptance testing. DO NOT MERGE until this interaction is resolved or separately authorised. |
| Tests completed / outstanding | Latest source-tested hosted [GitHub Actions run 41132972902](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41132972902): **PASS** in C++17 and C++20, **3,681/3,681 stage/race/cap policy cases per standard** plus **20/20 static source-wiring checks**, read from the original job log. These verify original OFF-path hooks, enforced bot invitations, and post-add-only quest writes, but **do not cover direct Group::AddMember bypasses** or actual gameplay. Full native Worldserver build [run 41132972780](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41132972780) was queued/pending at review; no native compilation outcome or deploy evidence. Actual owner Core/Playerbots compatibility, direct-add paths, offline group members and all in-client regressions are still required. |
| Status / rollback | **CODE COMMITTED ON ISOLATED DRAFT BRANCH, NOT MERGED, NOT DEPLOYED.** Not yet proven compiled or tested in game; do not run on production. Retain original protected develop/source and binary/config/character-data backups and plan branch-revert rollback before any later live test. |

## IP-F14-001 — Blood Elf/Draenei rndbots start in TBC (PLANNED, NO CODE)

| Field | Record |
| --- | --- |
| Intended behaviour | **Verified Random Bots (rndbots)** of race 10 Blood Elf or race 11 Draenei must **start at IP tier 8 (TBC)**, never be assigned Vanilla tiers **0–7**, and may progress through TBC/WotLK. Other race rndbots keep Vanilla eligibility. This is mandatory independently of the optional grouping setting. |
| Owner / reference | CosmicCuddle 2026-10-30; [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [grouping Issue #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10). |
| Component / affected files | IP fork with reviewed Playerbots integration. src/IndividualProgressionPlayer.cpp::OnPlayerLogin presently assigns tier 0 to non-normal accounts level <=60; group accept can force a leader's stage; src/IndividualProgression.cpp::SyncBotsProgressionToLeader and ForceUpdateProgressionState can also write tiers below 8. Check rndbot creation/generation and manual tier changes. **No paths changed for this feature.** |
| Safety | Under an active Vanilla progression cap, **never force tier 8 just to bypass that cap**. Race-10/11 rndbots should not be eligible for Vanilla population/grouping; design precise non-destructive suppression for pre-existing bots rather than deleting characters or changing live data without approval. Humans, Altbots, SelfBots and unknown identities are not subject to this rndbot-specific starting rule. |
| Config / SQL / DBC / client | No new config, SQL/schema, DBC, client patch or live bot migration approved. Verify actual installed Core/Playerbots revisions before implementation. |
| Risk / tests | Positive rndbot identity (not account regex alone), initial spawn/login, relog, level and stage transitions, manual changes to 0/7/8/12/13, group accept/sync, Vanilla vs TBC server cap, human/Altbot negative controls and rollback. |
| Status / rollback | **OWNER REQUEST ONLY. NOT IMPLEMENTED, COMPILED, GAMEPLAY-TESTED OR DEPLOYED.** Future isolated PR and backup/migration/rollback proposal require owner review. |

## IP-F14-002 — Hide TBC-race rndbots from Vanilla observers (RESEARCH, NO CODE)

| Field | Record |
| --- | --- |
| Intended behaviour | A Vanilla-stage observer should not see **positively identified** Blood Elf/Draenei rndbots; TBC+ observers can. **Real human characters must remain visible**, as must Altbots, SelfBots and Unknown identities unless the owner explicitly changes the rule. Concealment applies consistently to agreed /who and world interaction surfaces, not race alone. |
| Owner / reference | CosmicCuddle's 2026-10-30 **rndbot-only** decision; [F14 handover #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9). No coding-ownership transfer authorised yet. |
| Component / affected paths | Candidate policy owner: IP fork; possible narrow reviewed AzerothCore visibility hook if needed, plus positive Playerbots identity. Existing Naxxramas research: [WHO draft #145](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/145), [classifier #197](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/197), [facts #202](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/202), [race exposure #234](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/234). Reuse their work, avoid parallel implementations. |
| Technical blocker | Historical Grimfeather core 8daffdabe9202c228e45d833c9a1d399597a15cd returns visible ordinary players before the OnPlayerNotVisibleGloballyFor hook, so full world concealment purely through IP is not yet proven. WHO-only concealment does not hide name query, fields, model, or guarantee interaction/targeting restrictions. Existing #145 hides humans too and must NOT be enabled as written. |
| Config / SQL / DBC / client | Default OFF until approved, no new key or data/client patch authorised; inspect installed current revisions and effective IP cap. |
| Risk / tests | Visibility for real humans and unknowns; who counts, name query, first-sight and stage transitions, groups, permitted combat, GM/self open decisions; Playerbots snapshot concurrency with 8 map threads; safe fallback. |
| Status / rollback | **ARCHITECTURE RESEARCH ONLY; NOT IMPLEMENTED, COMPILED, IN-GAME TESTED OR DEPLOYED.** Plan one authoritative owner, reversible default-OFF implementation and separate approval before editing code. |

## External dependency checkpoint (not an IP fork source modification)

On 2026-10-30 CosmicCuddle updated Core and Playerbots before the Hunter local build. The updated Core required CreateAsyncConnection for Playerbots' queue-based database connection override, while the checked-out Playerbots header still had CreateConnection for that overload. CosmicCuddle backed up the header and corrected the one method name; the owner subsequently reported a successful Worldserver build. That Playerbots header change is **outside this IP repository**, and its persistence across future Playerbots updates must be checked. New owner Core/Playerbots Git HEADs, compiled install fingerprint and restoration status are not independently verified. See [build coordination #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).

## IP-INT-001 — Hunter/autocast + expansion grouping unified DRAFT candidate (31 Oct 2026; NOT MERGED)

| Field | Record |
| --- | --- |
| Feature | **Review-only integration** of owner-reviewed Hunter same-pet autocast rank continuity [PR #1](https://github.com/CosmicCuddle/mod-individual-progression/pull/1) and expansion-era `EnforceGroupRules` [PR #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3), preserving both original source branches |
| Intended behaviour | Keep original Hunter native ON/OFF across same-loaded-pet legal rank reconciliation and grouping Vanilla 0–7 / TBC 8–12 / WotLK 13–18 with switch ON; OFF preserves historic grouping behaviour |
| Owning component / files | CosmicCuddle IP fork: exactly inherited grouped `src/IndividualProgression.cpp`, `src/IndividualProgressionPlayer.cpp`, `src/IndividualProgressionGroupPolicy.h` plus original **Hunter `src/IndividualProgressionSpells.cpp`** at Git blob `6ee93e1174b5aef6c5d61c9ea31526506c9d08da`; original Hunter checker `tests/check_hunter_autocast_source.py`, existing grouping checks, new integration CI and [integration memo](HUNTER_GROUPING_INTEGRATION_REVIEW_2026-10-31.md) |
| Source refs / dependencies | Parent exact grouping SHA `e0cd785f6b2090c8f099c824811077d0ab2a9de5`, Hunter executable source revision `f0044987cc2d6f0b73c1cd25d17012a26a764beb`; pinned Core `8daffdabe...`, Playerbots `e88cc409...` with **runner-only** historical 2-line compatibility substitutions; Naxx `80e9de1...`; actual owner installed and dirty changes **NOT VERIFIED** |
| SQL/config/DBC/client | No additional SQL, DB schema, DBC, client patch or new config key. Existing `IndividualProgression.Enable`, `VanillaHunterPets`, `EnforceGroupRules`, `ProgressionLimit` and saved pet state apply. Snapshot `ProgressionLimit=0` is unlimited IP limit, **NOT** stage7 cap |
| Tests | **Historical native S4 success**: [combined full Worldserver run #41145398854](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41145398854), all 3 IP source units compiled and executable linked/verified. Original Hunter native/standalone grouped native runs independently passed. [Exact source-review CI 41186284245](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41186284245) **completed/success at HEAD 93dc97b1d81274056223cbfddd1d3f8fd0e42f10**: strict five-path scope, source blob preservation, original Hunter checker and grouping C++17/C++20 policy matrix all PASS; later docs-only heads require rerunning review CI. No full owner-compatible/source-matched game client or manual bot group/dual-pet ON/OFF acceptance |
| Live status | **Unmerged DRAFT / no new deployment**, not evidence of owner running both new features. Preserve all original PR branches for provenance |
| Known risks / decisions | Blocked: direct Playerbots `AddMember` preflight missing, offline-party policy owner undecided, arena/BG special groups undecided, F14 positive race10/11 RandomBot stage8 protection and observer filtering unimplemented; Hunter OFF/default ambiguity and two-pet relog/stable need actual acceptance; Chromaggus upstream removal not approved |
| Rollback / release gate | Development-only revert of committed integration branch possible; production requires restorable owner backups, source/binary fingerprints, matched build, real group/pet tests, single reconciled Custom Modifications Register, and CosmicCuddle's separate explicit approval to merge and deploy |

## Other existing custom changes — inventory not yet complete

The protected `develop/cosmic-ip` baseline already contains individual progression rules and prior local behaviour. **Those changes are not yet individually enumerated here.** They must be recorded after comparing the owner-installed core/IP/Playerbots/module revisions and custom working-tree modifications. Do not treat this initial register as a licence to reset, overwrite, rebase, prune or upgrade them. See [coordination #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4) for the evidence gate.

## Upstream integration checklist

Before every proposed Grimfeather update: identify actual installed commits and dirty files; compare upstream paths and ABI; cross-check this register and live issue decisions; propose a conflict-preservation plan; compile in a separate environment; test game outcomes and custom regressions; document backup and rollback; and request CosmicCuddle's approval before merge or production change.

## IP-INT-002 — Partial F14 saved-stage login safeguard ported onto combined Hunter/grouping/C3 (31 Oct 2026; DRAFT, flag OFF, NOT MERGED)

| Field | Record |
| --- | --- |
| Feature / intended behaviour | One branch containing #16 (Hunter autocast, expansion grouping, C3 offline-leader guard) **and** #13's default-OFF partial F14 login safeguard. When the flag is ON, a positively identified headless Blood Elf/Draenei RandomBot whose saved stage is already 8–18 skips the destructive level-based login `ForceUpdateProgressionState`. All other characters, and every character with the flag OFF, follow the original login path. |
| Owner / coordination | CosmicCuddle; assignment and claim in [Coordination Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13). IP #14 GM-command fixes are deliberately **not** included. |
| Branch / parents | `integration/hunter-group-c3-f14-partial-login-20261031`, created from [DRAFT #16](https://github.com/CosmicCuddle/mod-individual-progression/pull/16) `c44c3320406670cdc8f25c963ebba13e9cf24b71`. Port source [DRAFT #13](https://github.com/CosmicCuddle/mod-individual-progression/pull/13) `a18397bbb0c7054052030a83ac30b699ac382d38`. Both parents are unchanged and preserved. |
| Executable source | `src/IndividualProgressionPlayer.cpp`: #13's optional Playerbots include head and login guard, verbatim. #16's `OnPlayerCanGroupAccept` is byte-identical. `src/IndividualProgression.cpp`: the config read plus a forced-OFF error when the header is missing. `src/IndividualProgression.h`: one `bool`. Hunter `IndividualProgressionSpells.cpp` (blob `6ee93e11…`), group policy and GM commands are unchanged. |
| Config / SQL / DBC / client | New `IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin = 0` in `conf.dist`; the code default is also `false`. No owner configuration changed. No SQL, schema, DBC or client patch. |
| Supporting files | [Port memo](IP_COMBINED_C3_F14_PARTIAL_LOGIN_PORT_2026-10-31.md), `tests/test_ip_combined_c3_f14_partial_login.py`, the isolated workflow `ip-combined-c3-f14-partial-login-20261031.yml`. Declared deviation: #16's own frozen six-file step is scoped to #16's head with one `if:`, while its regression steps still run on this child. |
| CI / runtime status | Source tests and the pinned full Worldserver link (Core `8daffdab…`, Playerbots `e88cc409…` with two runner-only compatibility substitutions, Naxx `80e9de1c…`, `-DCOSMIC_IP_F14_REQUIRE_PLAYERBOTS`) are recorded in the PR and the coordination issue. The earlier #13 and #16 builds do not certify this combination. **No launch, no gameplay test, no owner-installed proof.** |
| Known risks / blockers | Partial F14 only: no stage 8 creation, TBC eligibility, stage 0–7 migration or rndbot visibility. C1 Playerbots `ResetQuests` and the other marker-removal routes are **not** protected. The enforcement-OFF group-accept write before a possible Core rejection remains, as does the direct Playerbots `Group::AddMember` bypass and the GM command hazards (IP #14). Live DB engine, `sql_mode`, installed revisions and effective settings are **UNKNOWN**. |
| Merge / rollback | Do not merge automatically. Folding into #16 requires a reviewed rebaseline of #16's frozen assertions. Revert or discard this branch only. Turning the flag OFF or restoring a binary does not restore deleted markers; restorable character and world DB backups and the owner's approval are required before any staging. |

### IP-INT-003 — reviewed GM null-safety port onto combined Hunter/C3/F14 child (DRAFT)

| Field | Preservation record |
| --- | --- |
| Intent / owner | CosmicCuddle's existing `.ip set`, `.ip tele`, `.ip setbot`, `.ip setrep`, `.ip attune` remain usable without dereferencing absent target, offline/unloaded player or missing console session; respect explicitly named/GUID targets. No change to valid GM stage progression authority or command permissions. |
| Owning component and source | IP fork `src/cs_individualProgression.cpp`; exact complete source blob **`654119d29e0519600d50b179ccd5a9c5df8695b0`** ported from standalone DRAFT [#14](https://github.com/CosmicCuddle/mod-individual-progression/pull/14) HEAD `d05f51a1f68fdf25e8f705dd1deddbcb397f22fc` onto frozen combined [#19](https://github.com/CosmicCuddle/mod-individual-progression/pull/19) parent `d53eb71c39c35f2ed488f2315565f89d8be71aa7`. Baseline #14 original GM file and #19 GM source had identical SHA `dd5754716e5b5546fa05324dc9e2250334cd5313` before this isolated port. |
| Code and tests | One real GM C++ file plus original `tests/test_ip_gm_command_null_safety.py` (15 tests); `tests/test_ip_combined_c3_f14_partial_login.py` now checks exact reviewed GM blob instead of old parent blob; dedicated six-path source workflow `.github/workflows/ip19-gm-null-safety-integration-20261101.yml`. All original Hunter, group, C3, optional default-OFF F14, source/config blobs remain untouched. Inherited combined #19 workflow runs source tests and separately compiles and links full Worldserver at child's exact head. |
| Config / SQL / DBC / client | No added config, migrations, SQL, DBC or client patch; existing `IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin` still OFF by default. Existing GM command RBAC/settings untouched. |
| Tests / runtime / status | **DRAFT child/unmerged/not deployed.** The original #14 historical full native build passed, but this child's exact joint compile/link and gameplay must be evidenced separately before claiming success. Command name/GUID/selected target, console, offline and numeric parser cases remain **NOT GAME-TESTED**. |
| Risks and dependencies | Known unchecked intentional valid-path GM stage writes can remove rewarded quest markers; C1 Playerbots `ResetQuests` and direct drop unprotected; expansion-group OFF pre-accept write/direct PB add hazards remain; full F14 RandomBot tier8 initialization and WHO/population remain missing; owner-installed Core/PB/IP/Naxx/SQL config, backup and restore unknown. |
| Decision and rollback | No owner merge/release approval. [Composed review memo](IP_GM_COMPOSED_REVIEW_2026-11-01.md). Do not silently overwrite this port in later upstream sync. Discard/revert only new child branch for source rollback. Before any approved staging, privately capture restorable character/world DB backups; binary/source rollback cannot restore deleted rewarded-stage rows. |

## IP-11 — Paid expansion entitlement: earned final milestone, then Vezrath purchase (1 Nov 2026; DRAFT, flag OFF, NOT MERGED)

| Field | Record |
| --- | --- |
| Feature / intended behaviour | Owner rule: the final milestone of an era (current source: Kel'Thuzad 40 = stage 7; Kil'jaeden = stage 13) only makes a character **eligible**. The next expansion opens after buying it from Chronomancer Vezrath. A TBC purchase grants stage 8 (start of TBC). A Wrath purchase stores a flag and writes no stage. An unpaid stage 13 counts as 12 for every access check, and stage 14 is never granted by a purchase. A yellow login reminder shows while the character is eligible and unpaid. |
| Owner / coordination | CosmicCuddle. Assignment and claim in [Coordination #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13); gameplay record [Coordination #18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18); plan [IP #22](https://github.com/CosmicCuddle/mod-individual-progression/pull/22) §3. |
| Branch / parent | `feature/ip-paid-expansion-entitlement-20261101`, created from [DRAFT #20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20) `5fe0f9105d55d249aa55a25510d3fa23e3d84148`. Parents are unchanged. |
| Executable source | New `src/IndividualProgressionExpansionPolicy.h` (pure rules) and `src/IndividualProgressionExpansion.cpp` (purchase API, cache, reminder, grandfather option). In `src/IndividualProgression.cpp`: `hasPassedProgression`/`isBeforeProgression` use the effective stage; `UpdateProgressionState` refuses unpaid cross-era grants; bot-sync ON comparison uses the effective stage; config read; force-OFF; registration. In `src/IndividualProgressionPlayer.cpp`: group invite and accept enforcement-ON comparisons use the effective stage. `src/IndividualProgression.h` gains new lines only. Hunter spells, GM commands, group policy, loader and all scripts are unchanged. |
| Config / SQL / DBC / client | New `IndividualProgression.PaidExpansion.Enable = 0`, `.GrandfatherStage13 = 0` and `.VendorName`. **No SQL**: purchases use Core `character_settings` (source `ip_paid_expansion`), saved in the same transaction as money. Requires worldserver `EnablePlayerSettings = 1`; otherwise the feature forces itself OFF. The live value is UNKNOWN. No DBC or client change. |
| Not gated | Bot and excluded accounts; configured DK / TBC-race / server start stages that already reach the era; characters already at stage 8+ (TBC) or 14+ (Wrath); GM `.ip` commands and `ForceUpdateProgressionState`. |
| Supporting files | [Design memo](IP_PAID_EXPANSION_ENTITLEMENT_2026-11-01.md), `tests/expansion_purchase_policy.cpp` (C++17/20, both eras), `tests/test_ip_paid_expansion_source.py`, workflow `ip-paid-expansion-entitlement-20261101.yml`. Declared deviations: #20's frozen six-path step is scoped to #20's head; #19's test reverses only the documented group substitutions before its strict comparisons. |
| CI / runtime status | Source, policy and pinned full Worldserver link results are recorded in the PR and Coordination #13. **No launch, no gameplay test, no Character Services integration, no owner-installed proof.** |
| Known risks / open decisions | Independent review (Coordination #11) open items: DB `conditions` on quest 66013 still pass for unpaid stage-13 characters (vendor stock, phasing gossip); DK / TBC-race start stages still grant their era (account unlocks now use an effective ceiling where an unpaid 13 counts as 12); no GM entitlement command; GM `.ip tele` reads the raw stage. Also: exact final requirements; grandfathering of existing stage-13 characters (decide before first enable); GM / Altbot exemptions; within-era catch-up; live prices; `/reload` is login-only. Character Services must switch from `ForceUpdateProgressionState(8/14)` to `PurchaseExpansion` and cap catch-up with `GetMaxServiceGrantableStage`. Bots remain ungated, and Playerbots `ResetQuests` (C1) can still remove bot stage markers. |
| Merge / rollback | Do not merge automatically. Set `PaidExpansion.Enable = 0` and restart to return to parent behaviour; stored flags are then ignored. Revert this branch to remove the code. Gold is never refunded automatically; any live cleanup needs backups and owner approval. |

### IP-CS01-GM-SELF-AUDIT — player-self get and paid-mode GM grant warning (DRAFT, 1 Nov 2026)

- **Owner intent:** CosmicCuddle previously specified `.ip get` usable by any player **for their own progression only**. Historical GM-only `.ip set`, `.ip tele`, `.ip setbot`, etc. retain security. GM stage overrides remain intentional, but must visibly warn and operationally log when they newly open a paid-era without a purchase. No new paid entitlement grant/revoke authority or forced syntax.
- **Source path / baseline:** `src/cs_individualProgression.cpp` exact changed blob `278aaa48dc5fb7770bd6013e659e4872752d4b03`; new `src/IndividualProgressionGmCommandAccessPolicy.h`, tests and DRAFT workflow; base [IP paid #23](https://github.com/CosmicCuddle/mod-individual-progression/pull/23) `8dc16ffb3ca7f70a9c7ddc7cfbe6c5bd40088e37`. Temporary CS01 Maintenance branch, NOT Claude's claimed branch.
- **Implementation:** `get` SEC_PLAYER with issuer-self-only direct resolution and explicit-name rejection for non-GMs; selected target ignored for ordinary players; GM/console original lookup retained. Historical OFF output remains; when paid ON, additionally show earned/raw vs effective/current access for normal account only. `set` logs only newly granted era rights for a normal account in paid mode, preserving original force update, achievement rollback and phasing. Log contains actor/name/account id or console, target GUID, before/after raw stage and era rights, no passwords or paid flags. `.ip tele` unchanged; historical raw `<13` condition documented.
- **Dependencies/risks:** new API v1 methods `GetEffectiveProgression` and `HasExpansionEntitlement`; pinned Core command parser `Optional<PlayerIdentifier>`, `WorldSession::GetSecurity`, Core module logging. The standard `module` logger may not be durable without host logging configuration. Console, offline targets, privacy of identifiers, staging `.ip get` permission and native Core linking require verification. No new secret storage, SQL, DBC or client patch.
- **Tests/evidence:** pure C++ policy compiled and executed GCC/Clang C++17/C++20 against exact Github header/test blobs (4/4 local PASS). New source-wiring tests and inherited exact-Git-blob contracts updated to the new `cs_` blob; hosted source/native CI, in-game command tests and actual owner account permissions remain NOT TESTED/PENDING.
- **Status/approval:** only reviewable default-off development; no server process or configuration change, no merge, no production. Rollback: revert this separate PR to the #23 head; owner approval required before live changes. Source memo: `docs/IP_GM_SELF_AUDIT_CS01_2026-11-01.md`.

### IP-11 review addendum — startup warning accuracy and storage-outage recovery (1 Nov 2026; DRAFT)

| Field | Record |
| --- | --- |
| Intended behaviour | Keep paid entitlement **OFF by default** and retain historical DK/TBC-race creation/unlock logic. Startup messages must warn only when a high starting stage is **combined with** an absent/too-low account creation threshold. No operator should lower historical DK start 13 to quiet a false warning. |
| Ownership / source | Maintenance child of [IP paid entitlement DRAFT #23](https://github.com/CosmicCuddle/mod-individual-progression/pull/23) at `8dc16ffb3ca7f70a9c7ddc7cfbe6c5bd40088e37`; [Grok independent #23 delta review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6561460786). |
| Files | `src/IndividualProgression.cpp` (log-warning guards only), `tests/test_ip_paid_expansion_source.py` (exact warning contracts), `docs/IP_PAID_EXPANSION_ENTITLEMENT_2026-11-01.md` (storage-outage and rollback runbook), this register. |
| Dependencies | `PaidExpansion.Enable` stays OFF by default; `EnablePlayerSettings` must remain ON for future owner-authorized paid feature. No migration, SQL, DBC, Core, Playerbots, vendor, character or bot state change. |
| Known risk | During settings-store downtime, a character who temporarily reads unpaid may have an automatic cross-era progression grant refused; recovery of stored purchase bits does **not** replay missed grants. Source does not produce per-character refusal audit logs. Manual repairs require verified affected players, owner approval, character backups and rollback. No live situation claimed. |
| Test / status | Source/CI checks are pending at the new draft SHA; no actual Worldserver compilation, in-game account creation, interrupted-store acceptance, GM remediation or live deployment is established by this addendum. |
| Rollback | Revert this child branch to recover the original warnings and text; owner must separately approve any future binary change. Turning the paid switch OFF restores historical raw-stage access and must not be represented as entitlement validation. |

### IP-11 PR #27 addendum — cross-era start warning coverage (1 Nov 2026; DRAFT)

| Field | Record |
| --- | --- |
| Source and review | [Grok review of #27 exact `cbc51cf4`](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6562168799) found two warning-only gaps; this addendum updates the same CS01-owned [draft #27](https://github.com/CosmicCuddle/mod-individual-progression/pull/27), not #23 or #25. |
| Intended behaviour | Check both TBC and Wrath access for **each** DK and TBC-race start/unlock combination; DK 8/7 and race 13/12 must warn, while historical default DK 13/13 and gated race 8/8 remain quiet. Two logs may occur if a configuration bypasses both eras. |
| Changed code | `src/IndividualProgression.cpp` adds two `LOG_WARN`-only guards, no stage/purchase/character/account writes; `tests/test_ip_paid_expansion_source.py` increases warning count to six, asserts both era thresholds and adds a labelled synthetic matrix; runbook updates example combinations. |
| Dependencies and risks | Owner-specific actual config still UNKNOWN; `PaidExpansion.Enable = 0` by default. No SQL, DBC, Core/Playerbots/vendor change, account race-creation rule change or installation. Warnings are startup-only and cannot establish live bypasses. |
| Tests, release, rollback | Source/matrix CI pending; native compile/gameplay and owner-host logging NOT verified. Revert the same child draft diff if declined; never overwrite #23, and obtain owner approval plus backup and rollback before any live deployment. |

### IP-11 integrated candidate — CS01 GM self-audit + cross-era warning delta (DRAFT)

| Field | Record |
| --- | --- |
| Intent | Compose separately reviewed [IP #25](https://github.com/CosmicCuddle/mod-individual-progression/pull/25) player-self/GM audit with [IP #27](https://github.com/CosmicCuddle/mod-individual-progression/pull/27) warning-only fixes, on a separate child branch. Preserve original parent #23 and both review branches unchanged. |
| Runtime boundaries | New GM self-only get / paid-mode set warning from #25, plus six operator-only startup log warnings from #27. No other runtime code, SQL, vendor, Core, Playerbots, paid-flag default or owner config modification. |
| Collision resolution | Retained #25 exact `cs_individualProgression.cpp` blob pin and the repaired explicit GM-exception test, while layering #27's six-log source contract and synthetic matrix. Register retains both separate changes append-only. |
| Evidence | Combined review/source/CI pending. No Worldserver native link, GM gameplay, vendor purchase or owner-host deployment proven. Default paid switch OFF; rollback by dropping this integration child. |

### IP-11 PR warning-label source-test follow-up (1 Nov 2026; DRAFT / no runtime change)

| Field | Record |
| --- | --- |
| Purpose / reviewer finding | [Grok #28 exact-head PASS with Low message-label gap](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6563199772): C++ paid-era warning conditions were source-checked but swapping their quoted TBC/Wrath operator labels could still pass all tests. |
| Owning component / files | CS01 isolated child of [IP #28](https://github.com/CosmicCuddle/mod-individual-progression/pull/28) exact parent `740a792da88ba3cbf8b0d0db74ce438266247fc2`; **only** `tests/test_ip_paid_expansion_source.py` and this appended register entry. |
| Intended behaviour | Preserve six existing warning-only logs and default-OFF paid mode; require each of four DK/TBC-race × TBC/Wrath C++ conditions to have the **same era name** in its operator warning. No character, gold, GM permission, entitlement, group, database or config behavior changed. |
| Source / dependencies | Existing pinned `src/IndividualProgression.cpp` unchanged. Python source test now captures the TBC/Wrath warning label and checks it against the exact threshold in the same complete `if (...) LOG_WARN` shape. No SQL/DBC/client, Core or Playerbots changes. |
| Verification / risks | GitHub source diff and structural sanity checks are not a complete Python suite, C++ compile or owner-host gameplay. Grok to perform independent exact-head read-only source/test/mutation check. Original startup warnings can report a risky setting but do not enforce access. |
| Live status / rollback | DRAFT, NOT merged/installed. Revert this child test/docs-only delta if declined; original canonical #28 branch remains frozen for Claude receipt integration. No production action or approval implied. |

## IP-F14-005 — F19 level floor and progression limit for the F14 saved-stage login guard (1 Nov 2026; DRAFT, F14 flag OFF, NOT MERGED)

| Field | Record |
| --- | --- |
| Feature / intended behaviour | With the default-OFF F14 login safeguard ON, keep a verified bot's saved stage only if it is at least the original level-based login stage (≤60 → 0, 61–70 → 8, 71+ → 13) and not above a nonzero `progressionLimit`; otherwise run the original write. Turning the safeguard ON can then never leave a bot below the original behaviour. |
| Owner / coordination | CosmicCuddle. Independent review findings F19-1/2/6 and Maintenance decision in [Coordination #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11); claim in [Coordination #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13). |
| Branch / parent | `fix/ip-f19-login-preserve-level-floor-20261101`, created from [DRAFT #20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20) `5fe0f9105d55d249aa55a25510d3fa23e3d84148`. #19 and #20 unchanged. |
| Executable source | New pure `src/IndividualProgressionF14LoginPolicy.h`; `src/IndividualProgressionPlayer.cpp` gains one include, one extra guard condition and a corrected comment. Nothing else changes. |
| Config / SQL / DBC / client | None. The existing `IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin = 0` stays the default. |
| Supporting files | [F19 memo](IP_F19_LOGIN_FLOOR_2026-11-01.md), `tests/f14_login_preserve_policy.cpp`, `tests/test_ip_f19_login_floor_source.py`, workflow `ip-f19-login-floor-20261101.yml`. Declared deviations: #20's frozen six-path step scoped to #20's head; #19's test reverts only the documented F19 lines before its strict comparisons. |
| CI / runtime status | Results are recorded in the PR and Coordination #13. **No launch, no gameplay test, no owner-installed proof.** |
| Merge / rollback | Do not merge automatically. Leave the F14 flag at 0, or revert this branch. |

### IP-11 + IP-F14-005 — combined F19 login floor and paid-expansion candidate (1 Nov 2026; DRAFT)

| Field | Record |
| --- | --- |
| Owner/component | CS01 Maintenance integration of separately reviewed #24 F19 into existing #30 warning-label/purchase source child, **without changing either source branch**; governing owner: CosmicCuddle. |
| Intended behaviour | With the existing default-OFF F14 RandomBot login preservation feature ON, preserve a saved stage only if it meets the historic level floor (0/8/13) and does not exceed a configured nonzero progression limit. Otherwise use the original normal-account-excluded level-stage write. All default-OFF IP, normal human paid-era effective group rules and Vezrath/GM logic remain inherited untouched. |
| Source changes and dependencies | `IndividualProgressionPlayer.cpp` only receives the exact #24 login-region plus `IndividualProgressionF14LoginPolicy.h` include; the rest of that C++ file retains the paid-effective group/account logic at #30. New standalone F19 policy header/test, F19 source contracts and native CI workflow pinned to the integrated GM command blob; prior F19 memo retained. Inherited paid/Hunter/group regression tests are adjusted only to admit the exact F19 changes. No SQL, Playerbots fork edit, config/DBC/client patch or live process. |
| Risks/tests | Original #24 source/policy harness is a reviewed *separate* source snapshot. This new combined HEAD needs its own Python, C++17/20 policy, full pinned Worldserver link and bot login acceptance. The historical pinned Worldserver/Playerbots source is NOT the owner-installed revision. `GrandfatherStage13`, F14 and paid mode are OFF in default configs. |
| Release/rollback | Isolated stacked DRAFT; revert child or retain F14 OFF if F19 has not passed combined test. No migration/DB/gold, source merge or owner-host deployment; request approval before any live activation. Preserve all previously registered custom changes. |

### IP-F19 + Paid/GM combined CI — historical fallback non-goal (1 Nov 2026; review-only DRAFT)

| Field | Record |
| --- | --- |
| Owner and parent | CosmicCuddle; CS01 child of independently Grok-source/Python-PASSed [IP #31](https://github.com/CosmicCuddle/mod-individual-progression/pull/31) exact `6b67cb9c1fdbdb54cabd8e4f83a88236ee7ffd51`. Does **not** modify source runtime, original parent, Claude receipt branch or installed realm. |
| Intended behaviour | Exercise paid purchase, GM command access, Hunter/group/F14/F19 source tests **together** when these files change; correct the operator memo to say saved F14 stage must be **at least** level stage and no higher than a configured nonzero cap. |
| Affected paths | `.github/workflows/ip-f19-login-floor-20261101.yml` adds five paths to PR triggering and paid/GM Python + C++17/20 pure-policy suites; `docs/IP_PAID_EXPANSION_ENTITLEMENT_2026-11-01.md` documents exact fallback; this append-only register entry. |
| Source dependencies and risks | Historical Core+Playerbots+Naxx pins, IP GM and paid source at #31 unchanged; no SQL, config, DBC or client patch. `ForceUpdateProgressionState(LevelStage(level))` can set level 71+ non-normal accounts (including excluded humans) to stage13 even with limit below13. This is confirmed historic OFF behaviour, **not** a new F19 cap bypass. Owner needs to decide whether to retain it or develop a separately guarded, default-OFF policy. |
| Verification status | Exact-head hosted CI, GCC/Clang and Worldserver link **not verified** for this child. The original #31 Grok independent source/local policy PASS is inherited evidence, not this workflow's execution. No gameplay, purchase, migration or owner-host test. |
| Live/rollback | DRAFT / unmerged / not enabled. Revert this child CI/doc amendment to restore original #31. Before any real install require native link, bot and excluded-human staged acceptance, backups, rollback and CosmicCuddle's explicit approval. |


## IP-PAID-TBC-QUEST-PURCHASE-SLICE-20261102 — owner-final stage7 purchase without auto8 (DRAFT, 2 November 2026)

| Field | Record |
| --- | --- |
| Owner rule | Owner [final specification via Grok](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6566353358) **supersedes** the earlier entry 'TBC purchase grants 8': KT40 earns raw7; paying Vezrath grants per-character TBC entitlement but leaves raw7, permits `Into the Breach` 10259, and **quest hand-in** grants8. Wrath KJ raw13/effective unpaid12 remains buy-and-go effective13, no stage14. |
| Owning component/parent | CS01 IP source-only child of [#32](https://github.com/CosmicCuddle/mod-individual-progression/pull/32) `6c180c0a78cdadcf6c79e8e627f2cc2e91e0fdae`; Claude's separate receipt work remains on [coordination #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13). Do not overwrite #32, Claude or IP upstream. |
| New source/test paths | `src/IndividualProgressionExpansionPolicy.h` (purchase grants no stage, paid raw7 generic-service cap7); `src/IndividualProgressionExpansion.cpp` (no stage award, verify unchanged raw+paid flag, preflight quest10259/66008 templates, refuse previously rewarded quest before debit, explicitly register and veto unpaid quest10259 completion in PlayerScript); `tests/expansion_purchase_policy.cpp` and `tests/test_ip_paid_expansion_source.py` updated for owner rule and hook source contract. |
| Documentation | [Full implementation, exclusions, release gates and rollback](IP_TBC_QUEST_PURCHASE_BOUNDARY_2026-11-02.md) and amended prior paid expansion memo. |
| Config, SQL, DBC and client patches | No new config switch, SQL, DBC or client file. Uses existing paid OFF by default. **Quest availability gate is not yet implemented**; stock world quest giver/ender must be verified before an SQL/ConditionScript design. |
| Compatibility and risks | Source now vetoes **new unpaid quest10259 completion** through Core's before-complete hook and refuses already-rewarded 10259 **before charging**, but it **does not hide/refuse quest offer/accept/share**, protect a previously saved COMPLETE from rewarding, or recover rewarded-at-raw7 characters. Pinned Core's `ConditionScript` call is commented out in `ConditionMgr.cpp`: **DO NOT use a SourceType19 ScriptName-only condition**; static paid marker would also break OFF unless handled. `DisableDefaultProgression=1` still makes paid hand-in inert. Claude receipt must support paid raw7/no quest-stage write with strict settings; `.pdump` still blocked. Preserve GM/bot/DK/race/grandfather exemptions, Wrath immediate effective13, quest66013 earned KJ. |
| Test status | Portable policy C++ and Python tests **authored, not executed here**, exact native Worldserver/Character Services or owner gameplay **not verified**, source branch unmerged, owner production permission absent. Re-check exact-head CI before calling it PASS. |
| Live and rollback | **NOT LIVE / NOT READY TO ENABLE**. Discard isolated child to revert to #32 source; actual backup/test/rollback plan and explicit owner approval required for any future real installation. |


### IP-PAID-TBC-10259-SAVED-COMPLETE-S1 — safe login-only deferral (2 Nov 2026; DRAFT)

| Field | Record |
| --- | --- |
| Owner / source | CosmicCuddle, CS01-IP Maintenance; isolated child of [#33](https://github.com/CosmicCuddle/mod-individual-progression/pull/33) at exact `201177b55489148f450770a91b2711efb63feefd`. Grok IP-only audit in [Coordination #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6573638063). |
| Intended behavior | With paid mode ON, an unpaid normal character whose nonrepeatable quest10259 is already saved COMPLETE is returned to INCOMPLETE at login, **without deleting objective counts or quest history**, so a direct reward cannot strand them at unpaid stage7. Paid/bot/excluded/start-stage and default OFF remain unchanged. Never touch REWARDED. |
| Source affected | `src/IndividualProgressionExpansion.cpp` existing OnPlayerLogin callback only; `tests/test_ip_paid_expansion_source.py` source/negative model assertions. No Core, Playerbots, Character Services, SQL, config, DBC or client patch changes. |
| Dependencies and risks | Core `Player::IncompleteQuest` at pinned 8daffdabe... changes status and log COMPLETE bit, not objective counters. Login ordering, actual quest client refresh, persisted status and re-login must be tested. Direct offline GM `.quest reward` remains unenforceable in IP and is an operator block. Offer/accept/share gating, already REWARDED recovery, receipts, DisableDefaultProgression and pdump stay release-blocking. |
| Verification | Exact-head CI/source tests and real Worldserver/Playerbots/Character Services link **not yet verified at creation**; game/DB/host tests not performed. Source/model tests are not gameplay evidence. |
| Live status / rollback | Development-only DRAFT, feature OFF. Drop isolated child to restore #33 unchanged. Before production require owner approval, restorable backups and staged quest/raid regression checks. |

### IP-PAID-TBC-10259-A1A2-ACCESS — scoped quest-accept undo and gossip suppression (2 Nov 2026; DRAFT)

| Field | Record |
| --- | --- |
| Owner / parent | CosmicCuddle / CS01-IP, distinct isolated child of [IP #34](https://github.com/CosmicCuddle/mod-individual-progression/pull/34) exact `a6ac77d9fae9690be91f6ddb97422476164c0ca2`; Grok [Issue #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6573888217). |
| Intended behaviour | Normal unpaid humans with paid mode ON cannot retain quest10259 after an online Core quest accept; the ordinary Proudwell quest menu is suppressed. Bot/excluded, paid/start privileges and mode OFF unchanged; stage8 and reward remain protected by H1/S1. |
| Source and dependencies | `src/IndividualProgressionExpansion.cpp`: `PLAYERHOOK_ON_PLAYER_QUEST_ACCEPT` post-add slot-clear + active-quest removal + timed-achievement cleanup; separate `AllCreatureScript::CanCreatureGossipHello` for NPC19942 with proper registration. `tests/test_ip_paid_expansion_source.py` exact-method source checks. No Core/Playerbots/Services SQL, configuration, DBC or client patch. |
| Known exceptions | Direct quest query still displays details; ! icon persists; Playerbots-driven normal-account AI bypasses the post-add PlayerScript; offline GM commands bypass. This is an effective online human accept defence, **not** a full pre-take or all-account quest-invisibility gate. Client flicker/quest-tracker artifact and modified world quest-giver source require real-game verification. |
| Verification | CI/native and game acceptance **NOT yet verified at creation**. Hook ordering source-checked on pinned Core `8daffdabe...`; tests are contracts, not gameplay. |
| Live / rollback | DRAFT, unmerged, paid OFF; discard isolated child to retain #34. Owner approval, restorable backups and latest joint Worldserver/quest gameplay before enabling. |

### IP-PAID-RECEIPT-STRICT-LEDGER-20261102 — CS01 interim receipt coding (DRAFT, source/model only)

| Field | Record |
| --- | --- |
| Owner / developer | CosmicCuddle; CS01 Maintenance temporary takeover from Claude/CS03-IP by explicit owner request. Child of IP #35 source `3a9997984ea827223e3cb31022b1cfe8cdf7c75c`; Claude's unreported work remains untouched. |
| Intended behaviour | Validate raw `ip_paid_expansion` character settings with version/presence sentinel and per-era money/timestamp pairs. Refuse legacy/missing/malformed/shifting/unsupported rows rather than inferring zero, gift entitlement or recharging; TBC purchase still must remain raw stage7, Wrath effective13. |
| Affected files | New `src/IndividualProgressionReceiptPolicy.h`, `tests/receipt_ledger_policy.cpp`, `.github/workflows/ip-receipt-ledger-policy-20261102.yml`, `docs/IP_RECEIPT_TAKEOVER_2026-11-02.md`, this append. No existing runtime C++ modified by this first slice. |
| Dependencies/risks | Pinned Core `PlayerSettings.cpp` malformed-token dropping and `SaveToDB` malformed-row overwrite, missing/pdump source rows, owner-approved bootstrap, actual transaction wiring and costs are **not solved**. No SQL, Core, Playerbots, client, DBC or config change. |
| Tests / live | Disposable C++17/20 policy CI and negative raw-token cases authored, job results must be checked; no native Worldserver, DB, money or gameplay claim. Default feature OFF. |
| Rollback | Discard isolated child to restore #35 unchanged; no production rollback or DB migration. Require owner approval before any live activation. |


### IP-RECEIPT-STRICT-RUNTIME-20261102 — guarded payment source candidate (DRAFT)

| Field | Record |
| --- | --- |
| Owner/component | CosmicCuddle; CS01 interim Claude receipt takeover, isolated child of policy #36 exact `73ce3be8df5fc2fadf2af3e5a52d9e2e82077c1c`. |
| Desired behaviour | Raw v1 validated on login/account; no missing/invalid entitlement; no debit without valid ledger; amount/time prepared before charge and saved through Player in-memory settings in same SaveToDB transaction. TBC raw7 and Wrath effective13 unchanged. |
| Files | `src/IndividualProgression.h`, `src/IndividualProgressionExpansion.cpp`, source contracts, this register and receipt memo; standalone policy header remains unchanged. No SQL/config/DB/Core/Playerbots/client changes. |
| Risks/tests | **Blocking:** no approved new/legacy ledger bootstrap, Core permissive parser may rewrite malformed bytes, pdump omits settings, real native build & gameplay pending. No production money tests. Fail-closed store means no owner character can buy without pre-provisioned valid v1 row. |
| Live/rollback | Feature OFF, unmerged. Discard child to restore strict policy #36. Owner staging, backups and explicit approval required. |
