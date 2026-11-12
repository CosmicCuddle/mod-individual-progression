# F14 Controlled Gameplay Acceptance Plan — 12 November 2026

Owner: CosmicCuddle  
Component: Individual Progression / AzerothCore / Playerbots integration  
Status: **PREPARATION ONLY — NOT AUTHORISATION TO ENABLE, START, DEPLOY OR MODIFY OWNER DATA**

This plan is the acceptance checklist for the Blood Elf / Draenei Individual Progression work after source/native gates are complete. It deliberately separates tests that are safe while positive concealment remains impossible from tests that stay blocked on Coordination Issue #42 (R2) and a separately reviewed positive implementation.

## 1. Pinned pre-test candidate

Current source candidate at preparation time:

- Core21: `991f2c86c928222d501a559a61434cfd15055dc8`
- Playerbots15: `4b355ea3a8fd00a0b116681bf9db4c7a3eb0e051`
- IP168: `455597329f2578c634091b8c3a79d83b31181d40`
- Naxx292: `51d0611476358286505aed3ca806ea250f644f66`
- Character Services newest native child: Services68
- WPS gameplay patch: `016999b24c5698b83fefb7f1919061cfd156f9c4`

Do not silently replace any pin during testing. If upstream sync is integrated first, record a new exact tuple and rerun source/native gates before gameplay.

## 2. Mandatory safety prerequisites before any owner-host test

Before a Worldserver is started for this work:

1. Confirm the exact installed Core/module revisions and effective config files.
2. Preserve tracked, untracked and ignored custom files, especially SQL cleanup/update files.
3. Take recoverable backups of relevant databases, configs, binaries/module source and client/DBC custom assets.
4. Confirm the restore procedure, not only backup creation.
5. Use a development/disposable environment or owner-approved offline copy first.
6. Keep all distributed F14 and paid-progression runtime switches OFF unless a later test step explicitly requires one and CosmicCuddle has separately approved it.
7. Never treat a successful compile as gameplay acceptance.

## 3. Phase A — observer-only / hard-visible acceptance

**This phase does not require positive concealment.** IP160's token-bound initial-OUT callback remains hard-FALSE. Expected player-visible result is ordinary Core visibility.

### A1. Baseline human visibility

- Normal human-controlled Blood Elf logs in: visible normally to Vanilla and TBC+ test viewers.
- Normal human-controlled Draenei logs in: visible normally.
- Human BE/Draenei reconnects after socket loss: remains ordinary visible.
- Human BE/Draenei changes map/instance, logs out and relogs: no disappearance, stale state or crash.
- GM and non-normal identities remain ordinary visible.

**Pass:** no F14 observer can make a real human character disappear.

### A2. RandomBot identity lifecycle

For positively verified BE/Draenei RandomBots:

- normal login;
- logout;
- map transition followed by logout before another combat sample;
- reconnect/reload where applicable;
- startup restoration of saved Playerbots `add` events;
- Playerbots login failure/cancellation;
- repeated actor GUID with a newer login epoch.

**Pass:** no crash, no stale actor ownership, no incorrect reuse of an older session identity, and no player-visible concealment while hard-FALSE.

### A3. Combat ownership lifecycle

Exercise:

- bot acquires creature target;
- bot switches target;
- bot stops attacking;
- target evades;
- target dies;
- target despawns/removes from world;
- bot changes map while attacking;
- bot logs out immediately after map change;
- two verified bots attack the same creature;
- one leaves while the other remains.

**Pass:** observer state retires safely; later events do not attach to a retired creature incarnation; no stale slot causes a crash or visible suppression.

### A4. Rights-refresh performance/safety

With the IP168 observer macros compiled:

- sustained single-bot combat;
- several bots on one creature;
- several creatures being fought concurrently;
- repeated direct damage/heal, periodic damage and aura transitions;
- target/combat/evade/death transitions.

Observe CPU/map-thread behavior and logs.

**Pass:** no immediate visibility sweep storm attributable to the old `UpdateObjectVisibility(true)` path; no crash/deadlock; ordinary visibility remains intact. Performance measurements are comparative evidence, not a production capacity guarantee.

### A5. Capacity / poison fail-visible checks

In a disposable environment, exercise enough distinct verified bot sessions to stress the fixed actor/pass trackers, including release/reuse.

**Pass:** valid session release permits slot reuse; stale epochs cannot release newer sessions; any deliberately induced uncertainty/poison fails visible and never grants concealment.

## 4. Phase B — positive concealment acceptance

**BLOCKED. DO NOT RUN OR IMPLEMENT FROM THIS DOCUMENT.**

Phase B requires all of the following first:

- explicit owner/Information decision on R2 in Coordination Issue #42;
- a reviewed restoration-debt completion/global-recovery design consistent with that decision;
- separately reviewed positive Store Open/Reserve/Commit/admission wiring;
- fresh source/model/native gates proving positive authority is limited to the intended pair;
- explicit CosmicCuddle approval for controlled gameplay testing.

When those gates exist, the required gameplay matrix is:

### B1. Viewer and actor eligibility

- Human BE/Draenei: **always visible**.
- Positively verified RandomBot BE/Draenei: eligible only under approved F14 conditions.
- Altbots/SelfBots/manual or uncertain identities: **fail visible**.
- Vanilla observer: eligible F14 concealment only where policy says so.
- TBC+ observer: sees eligible BE/Draenei normally.
- Missing/contradictory viewer epoch, map generation, pass token, actor proof or rights: **visible**.

### B2. Combat-target concealment

CosmicCuddle's intended behavior:

- when an eligible hidden BE/Draenei RandomBot attacks an eligible creature, a Vanilla observer must not see a mob apparently attacking thin air;
- the exact creature may therefore be hidden only while the approved actor/rights relation exists;
- target switch retires the old target's concealment;
- combat stop restores it;
- evade restores it;
- creature death restores/clears the relation;
- despawn/respawn cannot inherit the old incarnation;
- bot logout/map change/session change retires old rights;
- multiple attackers must not restore a creature prematurely while another eligible relation remains.

Any uncertain participant, pet, spell-only, tap, loot, quest, assist or ownership situation not positively proven by the reviewed policy must fail visible.

### B3. Restoration and R2

Test exact obligations for:

- same-session visibility reevaluation;
- map transition;
- reconnect/new authenticated session;
- logout/login;
- creature death/despawn;
- viewer departure while restoration is pending;
- multiple hidden creatures;
- retained debt;
- deliberately uncertain commit;
- global poison/unattributed risk.

**Pass criteria must use the owner-approved R2 proof boundary.** A server send/relink call must not be treated as client presentation/ACK unless Issue #42 explicitly authorises that evidence.

The admission fence must never reopen merely because a reconnect happened. Global recovery must satisfy the separately approved all-clear proof.

### B4. WHO / world consistency

Where F14 player visibility and WHO filtering are enabled by separately reviewed policy:

- Vanilla observer must not learn a hidden eligible actor through one surface while the other surface says hidden;
- TBC+ observer receives normal world/WHO behavior;
- human/Altbot/uncertain identities remain visible/ordinary according to normal Core rules.

### B5. Grouping and progression

With the approved grouping rules enabled:

- Vanilla band: tiers 0–7.
- TBC band: tiers 8–12.
- WotLK band: tiers 13–18.
- BE/Draenei subject to the intended restrictions cannot silently enter a Vanilla group path that policy rejects.
- Human progression state must never be rewritten merely to satisfy grouping or visibility.
- Paid progression remains independent and OFF unless separately approved.

## 5. Phase C — upstream-sync regression

After the later Core/Playerbots upstream sync, rerun Phase A and every authorised Phase B test.

Mandatory sync-specific cases:

### Core

Manually preserve and retest the F14 additions in the four current overlap files:

- `src/server/game/Handlers/CharacterHandler.cpp`
- `src/server/game/Server/WorldSession.cpp`
- `src/server/game/Scripting/ScriptMgr.h`
- `src/server/game/Grids/Notifiers/GridNotifiers.h`

The reconnect, socket-disconnect and VisibleNotifier boundaries currently still exist upstream, but that must be reverified at the final merged SHA.

### Playerbots

Manually preserve/retest the eight current overlap paths, especially:

- already-REWARDED IP marker protection in `PlayerbotFactory::ResetQuests()`;
- verified RandomBot identity and `currentBots` lifecycle;
- saved-`add` event replay;
- new upstream population downsize / `RemoveRandomBots()`;
- manual Vanilla BE/Draenei population holds under current upstream count modes;
- pending async login hold;
- Altbot/SelfBot/human negative cases.

Retain the four currently inventoried Playerbots SQL updates and the 25 Core SQL-path changes for explicit applicability review. File-path non-overlap is not permission to apply SQL.

## 6. Evidence record for every gameplay case

Record:

- exact Core/module commit tuple;
- effective relevant config switches;
- database snapshot identifier/date (without publishing private credentials or data);
- test character/bot category, not sensitive account details;
- map/instance and progression band;
- action performed;
- expected result;
- observed result;
- relevant server log excerpt reference;
- PASS / FAIL / BLOCKED;
- regression issue/PR if failed.

Video/screenshots may supplement evidence but do not replace server/source evidence for identity, debt or restoration state.

## 7. Immediate stop conditions

Stop the controlled test and preserve evidence if any of these occurs:

- a human/Altbot/uncertain actor is hidden;
- a Vanilla observer loses an unrelated creature/player;
- a creature remains hidden after its approved relation ends;
- stale session/map identity becomes valid again;
- tracker poison appears during ordinary expected workload;
- restoration debt is silently cleared without the approved proof;
- admission fence reopens without the reviewed global recovery;
- crash, deadlock, repeated map-thread stall or material performance regression;
- SQL/schema/config mismatch appears;
- any established custom behavior disappears.

Do not repair production state ad hoc. Roll back to the prepared baseline and investigate on a development branch.

## 8. Current acceptance status

At creation of this plan:

- source/model safety has progressed through IP168;
- Services64 and Services65 are full native GREEN;
- later Services native descendants remain under validation;
- IP168 focused compiler matrix is GREEN;
- positive concealment remains hard-FALSE;
- R2 #42 remains unanswered;
- no gameplay acceptance has been claimed;
- no production deployment has been authorised.
