# F14 BE/Draenei pair-rights runtime evidence adapter (diagnostic only)

**Owner:** CosmicCuddle. **Developer:** CS03-IP. **Lead/review:** CS01-IP; CS02-IP (Grok) reviews the delivery.
**Date:** 13 November 2026.
**Assignment, claim, ACK:** Coordination #13 [6848752750](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6848752750), [6849167112](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6849167112), [6849231953](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6849231953).
**Parent:** IP #172 `aed05a92f262362608f5d643c8570b504189aae0` (on IP168 `455597329f`).
**Core:** Core25 `395b68ee4ce425826c2a79c0ddf3ade3c67c8160`.
**Status:** DRAFT. Opt-in build macro `COSMIC_IP_F14_PAIR_RIGHTS_ADAPTER_EXPERIMENT`, never defined in source and absent from default builds.

## What it does

When the existing token-bound initial-OUT observer has passed all of its gates, the adapter takes a snapshot of the current viewer and creature, maps it to `IndividualProgressionF14CurrentPairRightsRisk::Facts` and evaluates the existing model. Those gates are: active pass token for the exact current human viewer identity, same `Map*`, the exact live creature incarnation, and current verified RandomBot attackers > 0.

The verdict is **only counted**: one atomic counter per result, and at most one generic log line per result. **The observer still returns FALSE on every path.**

There is no Store Open/Reserve/Commit, prior-hidden ledger change, packet, destroy/send, phase change, WHO filtering, restore-debt completion, fence reset or client-ACK claim. Even `NoKnownVeto_NotAuthority` would not be authority, and Coordination #42 stays blocked.

## Files
- `src/IndividualProgressionF14PairRightsEvidence.h`: pure `Raw` → `Facts` mapping. It alone decides every `EvidenceComplete` bit. Every default fails.
- `src/IndividualProgressionF14PairRightsCoreAdapter.h`: Core25 collector, included only by the lifecycle file under the macro.
- `src/IndividualProgressionF14CurrentCoreLifecycle.cpp`: four exact macro-guarded insertions (dependency `#error`, include, the call, the counter member). Removing them reproduces the parent byte-for-byte.
- Tests, workflow, this note and the register entry.

## Call chain and read boundary (Core25)

The query is reached synchronously:
1. `VisibleNotifier` opens a pass token for a non-headless viewer (`GridNotifiers.cpp:60`; the pass ends at :76).
2. The batched `Player::UpdateVisibilityOf` asks `IsF14CreatureSuppressedFromClientViewForInitialOut` (`PlayerUpdates.cpp:1745`, called at :1872). It does this only for a creature the viewer has at client and that is ordinarily visible, and only after Core's own candidate filter has excluded GMs, owned/charmed creatures, pets, summons, guardians and totems.
3. ScriptMgr calls the observer, which calls the adapter.

At that point both objects are live on the viewer's map, and the adapter reads **only** their own state:
- through const accessors;
- the two units' own applied-aura containers (caster GUIDs only);
- the static quest template and loot stores.

It never dereferences a controlled unit, summon, vehicle base or passenger, and never looks up an aura caster. There is no `ObjectAccessor`, Auth/character DB, Playerbots, group-member iteration or `const_cast`. No pointer outlives the call.

**Thread-safety basis (corrected after CS02 review L1):**
- **Not "what Core reads in the same frame":** `Player::UpdateVisibilityOf` does **not** itself read several things the adapter reads, so that earlier wording overstated the argument. Those are the viewer's quest log and quest-status map, either unit's applied-aura map, the creature's threat manager, the viewer's group pointer and the session vendor entry.
- **The actual basis is AzerothCore's usual invariants:**
  - one map is updated by one thread at a time;
  - session and world handlers run before `MapMgr::Update` in `World::Update`, not alongside it.

  So the viewer's and creature's own containers aren't written concurrently while this frame runs. No concurrent writer has been found, but this is an argument from Core's design, not a proof.

**Still not proven:** Core25 has no owning-map-frame API (`Map::IsWithinF14MapUpdateInvocation` existed only in a disposable prototype), and the active notifier token is **not** thread-ownership proof. That's why secondary objects (controlled units, vehicles, aura casters) are excluded entirely.

**Known limits (CS02 review, recorded and not fixed in this diagnostic):**
- **L2, diagnostic hot-path cost:** with the macro on, `Collect` runs on every token query that passes the gates. That's up to 25 quest slots × 4 objectives, two aura scans of up to 64 each, and three `HaveQuestLootForPlayer` calls that walk whole loot templates, once per visibility pass for each viewer and each RandomBot-attacked creature. It never runs in default builds. A future child could skip the loot-store calls once an earlier veto has fired, or rate-limit `RecordPairRightsDiagnostic` per pair.
- **I1, counters not observable:** `_pairRightsResults` is incremented but never read or logged periodically. A run shows only the first-occurrence line per result, not frequencies. A bounded periodic summary would need a later child.

## Evidence completeness

| Family | Core25 facts (file:line) | Complete when |
|---|---|---|
| viewerEligibility | `IsGameMaster` Player.h:1178, `GetCharmerGUID` Unit.h:725, `GetSeer` Player.h:2399, plus the observer's exact identity gate | gates held, not charmed, seer is the viewer |
| creatureEligibility | `IsAlive`, exact live incarnation gate | gates held and alive |
| directInteraction | `GetTarget` Unit.h:867, `GetVictim` :910 (both ways), `IsThreatenedBy` :972, `IsInCombatWith` :950 (both ways) | always scanned |
| ownedUnit | public `m_Controlled` Unit.h:2095 and `m_SummonSlot` :2099 (public block :2091, `public:` :671 until `protected:` :2137); `GetCharm/Minion/Pet/CritterGUID` :723–731; `GetVehicle`/`GetVehicleKit` :1947–1948 | **only if all are empty**. Presence alone means incomplete, and nothing is dereferenced (CS01 conservative rule). `GetFirstControlled()` (Unit.cpp:7996) returns only the charm or `UNIT_FIELD_SUMMON` minion, so it is never used as proof |
| creatureRole | `GetOwnerGUID` :719, `GetCharmerGUID`, `GetCreatorGUID` :721, `IsControlledByPlayer` :1324, `ToTempSummon` :738 + `GetSummonerGUID` TemporarySummon.h:66, `IsPet/IsGuardian/IsTotem` :801–805, vehicle relation | always scanned. Any relation is a veto; seats are not enumerated |
| groupLoot | `GetGroup() const` Player.h:2519; `GetLootRecipientGUID` / `GetLootRecipientGroupGUID` Creature.h:231–233 | always scanned. Any group is a veto |
| aura | `GetAppliedAuras() const` Unit.h:1401 on both units; `AuraApplication::GetBase` SpellAuras.h:63; `Aura::GetCasterGUID` :106. At most 64 per unit, otherwise overflow | both scans within bound and every caster classified as self or player/pet. Any other or empty caster is **unknown**, because casters are never resolved |
| pendingSpell | any `GetCurrentSpell(t)`, `t < CURRENT_MAX_SPELL` (Unit.h:1593, :570), is a veto | **never**: a launched delayed spell lives as `SpellEvent` (Spell.cpp:555) in the caster's `m_Events` (Object.h:749), and `EventProcessor` exposes only `HasEvents()` |
| serviceInteraction | `GetNpcFlags` Unit.h:768, `IsServiceProvider` :823, `WorldSession::GetCurrentVendor` WorldSession.h:507 are vetoes | **never**: external scripted escort/service can't be excluded. `CreatureAI::IsEscorted()` is non-const (CreatureAI.h:181) and isn't called |
| scriptedReward | `GetScriptId` Creature.h:215, `GetAIName` :213, `Map::Instanceable/IsDungeon` Map.h:298–299, `GetFormation` Creature.h:371, `isWorldBoss`/`IsDungeonBoss` :123/:131 are vetoes | **never**: no reverse index of instance/zone/SmartAI/game-event/spawn-group references |
| quest | 25 slots (`MAX_QUEST_LOG_SIZE` QuestDef.h:33), `GetQuestSlotQuestId` Player.h:1510, `GetQuestStatus` :1492, `GetQuestTemplate` ObjectMgr.h:840, `RequiredNpcOrGo` QuestDef.h:305 vs entry and `KillCredit` CreatureData.h:190; quest loot through `HaveQuestLootForPlayer` LootMgr.h:226 for the creature, pickpocket and skinning loot ids | **never**: script and spell kill credit can name any entry, and other loot routes exist |

**Consequence:** on Core25 a runtime snapshot can never reach `NoKnownVeto_NotAuthority`. The best outcome is `EvidenceIncomplete`, and any observed interaction reports its specific veto first. This is the negative witness, and it is tested exhaustively.

**Missing Core facts that would be needed to ever complete a family:**
- a per-caster in-flight spell target query;
- a reverse script-reference index;
- exact service-session attribution;
- a script/spell kill-credit inventory;
- an owning-map-frame API.

## Tests
- **Native** (`tests/f14_pair_rights_evidence_test.cpp`, GCC/Clang, C++17/20, with and without NDEBUG):
  - every veto field maps to its expected result;
  - every bound, overflow and inconsistency case fails;
  - an exhaustive 2¹¹ omission witness on the model;
  - an exhaustive 2¹⁶ runtime-reachability witness;
  - a 200,000-case randomised witness.
- **Source contracts** (`tests/test_f14_pair_rights_adapter_source.py`):
  - every observer `return` is `return false;`, and the call runs after every gate;
  - the lifecycle file equals the parent plus exactly four reviewed insertions;
  - the macro requires the token observer and is never defined;
  - forbidden reads and writes are absent, owned units are presence-only, the loop allowlist holds, and casters are classified by GUID;
  - the anchored Core25 APIs are used;
  - the four families are hard-incomplete and the defaults fail;
  - every other parent file is byte-identical, and the flags are OFF.
- **Core25 anchor job:** 72 exact `file:line` anchors, plus negative checks on the access block, `GetFirstControlled`, the missing map-frame API and the `EventProcessor` surface.
- **Local only:**
  - the collector and the full macro-ON lifecycle file were syntax-checked (`-fsyntax-only`) against Core25 headers with the adapter ON and OFF;
  - adapter-ON without the token observer hits the `#error`;
  - the default build is unaffected.

  This is not a link.
- **Full native build:** the macro-ON full Worldserver link is the separate Services gate, run by Maintenance on the exact head.

## Evidence levels
Source and model proof, plus a local header syntax check, only. **No** Worldserver link has been claimed here yet, and no gameplay, client or owner-host test exists.

## Owner obligations and rollback
Before any future enablement, the owner still needs:
- an installed source/config/SQL/DBC/client inventory;
- tested backups;
- full human rights evidence;
- a reviewed restore-debt rule (#42);
- an approved in-client test plan.

Any future offline test must preserve tracked, untracked and ignored local custom SQL cleanup and migration files.

**Rollback:** leave the macro undefined (the default), or close the DRAFT and delete the branch. Nothing on any host, database, config or client was changed.
