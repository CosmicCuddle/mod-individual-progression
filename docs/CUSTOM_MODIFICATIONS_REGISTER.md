# Custom Modifications Register — Individual Progression fork

**Owner:** CosmicCuddle  
**Scope:** Custom source changes and planned IP-owned features in CosmicCuddle's Individual Progression fork, with external module dependencies recorded separately. This is **not** a complete inventory of the installed server.  
**Last reviewed:** 2026-10-30  
**Authority:** CosmicCuddle's approved gameplay rules; coordination Issues are the two-way decision log.  
**Operating rule:** Update this register for every proposed or implemented IP customization. Review it and actual owner source before upstream integration. Keep retired entries, and distinguish requested behaviour, committed source, passing builds, owner-reported game tests, verified live binaries and merged releases.

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

## IP-F14-003 — C++ RandomBot progression pre-write contract (TEST-ONLY, NO RUNTIME HOOK)

| Field | Record |
| --- | --- |
| Feature / goal | Implement a reusable **pure C++17** decision for a verified Blood Elf/Draenei RandomBot stage-8 minimum, preserving saved stage8+ on relog/restore, rejecting downgrades and over-cap writes before side effects |
| Owner / component | CosmicCuddle Individual Progression fork; source `src/IndividualProgressionRandomBotStagePolicy.h`, test `tests/f14_verified_rndbot_stage_policy.cpp`, source-test workflow and [policy memo](F14_VERIFIED_RNDBOT_CPP_STAGE_POLICY_2026-10-31.md) |
| Source provenance | Stacked on [combined Hunter+group candidate PR #10](https://github.com/CosmicCuddle/mod-individual-progression/pull/10) HEAD `d3688e243a24441438eaff7b3488b05f41614deb`; reuses `IndividualProgressionGroupPolicy.h` race/stage definitions; does **not** change executable Hunter/group/login/GM implementation |
| Inputs and identity | Requires **external verified RandomBot identity** and explicit TBC access, saved/new stage evidence, ingress route and progression cap. Human/Altbot/Selfbot/verified other defer; Unknown race10/11 identity is unresolved, **not write permission** |
| Configuration/SQL/DBC | Uses existing cap semantics (0 unlimited, positive cap enforced); no new config key, SQL, DBC or client patch, no in-game write API |
| Tests | Actual header compiled/tested under C++17/20 in isolated GitHub workflow with identity, race, stage and cap matrix. **Exact-head CI status must be verified separately**; no full Wordserver/build/gameplay outcome claimed for new (unwired) policy |
| Live status | **UNWIRED DRAFT, NOT IMPLEMENTED IN GAME**. No user host or actual character altered |
| Risks and missing work | Early source-matched positive identity before Core script login stage0 reset, preserved `add` bot restore, stage0 legacy saved-character migration decision, command-side achievement guard, rndbot-only observer visibility, owner Core/Playerbots source compatibility, gameplay matrix |
| Rollback/approval | Reject/revert isolated draft without changes to installed code. Real integration later needs approved interface, restorable backups, full native link and game regressions, plus CosmicCuddle's explicit merge/deploy decisions |

## Other existing custom changes — inventory not yet complete

The protected `develop/cosmic-ip` baseline already contains individual progression rules and prior local behaviour. **Those changes are not yet individually enumerated here.** They must be recorded after comparing the owner-installed core/IP/Playerbots/module revisions and custom working-tree modifications. Do not treat this initial register as a licence to reset, overwrite, rebase, prune or upgrade them. See [coordination #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4) for the evidence gate.

## Upstream integration checklist

Before every proposed Grimfeather update: identify actual installed commits and dirty files; compare upstream paths and ABI; cross-check this register and live issue decisions; propose a conflict-preservation plan; compile in a separate environment; test game outcomes and custom regressions; document backup and rollback; and request CosmicCuddle's approval before merge or production change.
