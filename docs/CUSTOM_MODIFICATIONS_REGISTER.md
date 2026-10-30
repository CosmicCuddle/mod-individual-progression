# Custom Modifications Register — Individual Progression fork

**Owner:** CosmicCuddle  
**Scope:** `CosmicCuddle/mod-individual-progression` only. This is the first **source-backed entry**, **not** a complete inventory of all existing modifications or of the installed server.  
**Last reviewed:** 2026-10-30  
**Authority:** CosmicCuddle's approved gameplay rules; coordination Issues are the two-way decision log.  
**Operating rule:** Review this register **and** actual installed working-tree changes before upstream integration. A proposed change is not a live feature. Keep entries for rejected/retired proposals with their history rather than silently deleting them.

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
| Supporting files | `docs/HUNTER_AUTOCAST_RANK_CONTINUITY_REVIEW_2026-10-30.md`; `tests/check_hunter_autocast_source.py` |
| Core APIs | `Pet::m_spells`, `m_autospells`, `ToggleAutocast`, `unlearnSpell`, `learnSpell`, `CharmInfo` action bar, pet GUID/CustomData, `PlayerScript::OnPlayerAfterUpdate`. Historical [core ref](https://github.com/Grimfeather/azerothcore-wotlk/tree/b7f06a16ac7120b5abc2e8d3f96a94265a363224) only; verify installed implementation |
| Playerbots interactions | Existing `isBotAccount` exclusion is retained; verify exact installed Playerbots/core and that updates do not add bot regressions |
| Config dependencies | Existing `IndividualProgression.Enable`, VanillaHunterPets and individual progression gates; **no new config key proposed** |
| SQL / database | Existing native `pet_spell` autocast state and action bar persistence; **no SQL/schema changes proposed**. Real schema and loaded settings not inspected |
| DBC / client-patch | No new patch proposed; actual installed spell-chain/DBC data and WotLK client response must be verified |
| Risks | Unexpected rank chains; multiple/stale spell and autocast entries; action-bar ghost slot; packet timing; async pet-load races; prolonged loading; per-tick hook overhead; cross-module dependencies; owner core API drift |
| Tests completed | Source-only review of pinned core APIs and family mapping preservation; equivalent 17 static invariant checks passed on 2026-10-30 via connector. **The committed Python checker has not itself been executed.** |
| Tests outstanding | Actual isolated C++ compile/link with installed-compatible core, controlled ON→ON / OFF→OFF higher/lower ranks, pet switch, slow/stuck loading, native pet save/relog/stable, client bar packets, Playerbots/Warlock regression |
| Rollback prerequisites | Verified backup of source, binary, configuration and **test pet/character DB state** before gameplay tests; known-good version / restore exercise. Revert a draft branch before merge, or a separately approved release rollback if ever deployed |
| Live status | **EXPERIMENTAL SOURCE ONLY. NOT COMPILED, NOT GAMEPLAY-TESTED, NOT MERGED, NOT DEPLOYED.** |
| Blockers | [Coordination #3](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/3), [#4 installed-source/restore-readiness](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4); separate isolated build/test and merge/deployment approval |

### Never conflate with other features

- **P1:** restoring a persisted setting requires fresh post-combat display-only current-pet prompt and separate protected human apply click.
- **K1:** different pets—even with the same displayed name—retain separate settings. Runtime pet GUID or recycled PetNumber is not a durable identity/provenance guarantee.
- **D18:** in-combat prearmed Pet A→B protected execution remains forbidden.
- **A02/N-Core:** separate addon-side saved preference research is not delivered or authorised by this in-place IP rank continuity rule.
- Existing commented Cower and other `keepId=0` removal-only lines are not granted or transformed into learned spells.

## Other existing custom changes — inventory not yet complete

The protected `develop/cosmic-ip` baseline already contains individual progression rules and prior local behaviour. **Those changes are not yet individually enumerated here.** They must be recorded after comparing the owner-installed core/IP/Playerbots/module revisions and custom working-tree modifications. Do not treat this initial register as a licence to reset, overwrite, rebase, prune or upgrade them. See [coordination #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4) for the evidence gate.

## Upstream integration checklist

Before every proposed Grimfeather update: identify actual installed commits and dirty files; compare upstream paths and ABI; cross-check this register and live issue decisions; propose a conflict-preservation plan; compile in a separate environment; test game outcomes and custom regressions; document backup and rollback; and request CosmicCuddle's approval before merge or production change.
