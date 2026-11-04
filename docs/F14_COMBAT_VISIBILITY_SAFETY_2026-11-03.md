# F14 combat-target creature visibility — isolated safety proposal

**Owner:** CosmicCuddle. **Status:** UNWIRED code model, not a functioning feature, not merged or deployed. **Source baseline:** IP #46 `cbc8de8e`; F14 source-only PR #47. **Review:** [Grok CS02-IP 6603455946](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6603455946) and [6603460102](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6603460102). **Coordination:** [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9).

## Owner's requested outcome

Avoid a visible NPC appearing to fight thin air after a *positively verified* Blood Elf/Draenei **RandomBot** is hidden from a **Vanilla** observer. Do not hide ordinary human Blood Elf/Draenei players, Altbots, Selfbots or unknown identities. TBC+ observers retain normal visibility. This is a requested player-experience improvement, not approval of broad NPC concealment or any production change.

## Owner clarification — active attack only; corpse always visible (3 November 2026)

The owner clarified that a Vanilla observer's creature-hide effect is needed **only while a positively identified, already-hidden Blood Elf/Draenei RandomBot is currently attacking that particular living creature**. The creature must return immediately when the last qualifying bot stops attacking it, switches targets, logs out or resets — **even if the creature is still in combat**. Old threat, prior hits and the creature continuing to chase a bot must never prolong hiding. If another qualifying bot continues attacking that same target, the effect may continue subject to all original safety protections.

**Death is an unconditional show event:** once the creature dies, it must immediately reappear as its ordinary corpse, regardless of stale current-attacker, target or combat flags. Its loot sparkle, skinning, quest credit and other usual interactions must not be suppressed. The pure model now includes a distinct positive active-attack fact and the already-existing alive guard; the separate portable transition tests cover both cases. This does **not** implement the Worldserver visibility refresh required to make either transition instantaneous.

Required future refresh triggers include **attack begin, attack stop, target switch and creature death**, not merely combat enter/exit. All earlier real-player rights, no-ghost-hit, TBC observer and human player exceptions remain mandatory.

## Why source-only model is not safe to wire today

1. Pinned Core `Grimfeather/azerothcore-wotlk@8daffdabe`: `Player::IsVisibleGloballyFor` is not a world-visibility deny hook for normally visible players. Therefore the prerequisite *already-hidden verified RandomBot* is **not** implemented in a safe general world hook.
2. `CreatureAI::CanBeSeen(player)` is an AI-specific virtual. Replacing all AIs would disrupt scripts, bosses and SmartAI; it also has earlier always-visible exits. **Do not override arbitrary creature AI.**
3. The creature's own targeting, threat, aggro, assists and AoE may still hit a Vanilla observer even while that observer cannot see/attack the NPC. A visibility-only patch without **immediate threat/target refresh before hostile action** risks ghost hits.
4. Combat start/end/death/new victim/identity change and IP tier change do not all trigger an immediate `UpdateObjectVisibility` under this pinned Core. A stationary encounter can leave stale hiding. A simple `ShouldConceal` change in a C++ header is not enough.
5. `CanSeeOrDetect` is a hot path. It must not iterate the threat list, resolve ownership, call mutable `RandomPlayerbotMgr::IsRandomBot`, perform database access or log per packet. Snapshots should be map-thread-owned and published safely.
6. Spell, dynamic-object, aura and combat-log visibility are not yet audited. Their visibility and damage semantics cannot be inferred from an NPC model.

## Two-stage, OFF-by-default conceptual interface

- **Event-side creature snapshot** (future approved implementation): aggregate known current combat/threat participants, **positive** bot identity and minion ownership, plain non-elite non-scripted NPC classification, real-player threat/tap/quest rights and transitions. Invalidate/rebuild on threat add, victim change, engage, combat exit, death/evade, participant logout/reset and ownership change. If incomplete/stale: creature **visible**. A plain-ordinary-NPC-only pilot is proposed, not yet owner-approved for activation.
- **Viewer-side snapshot**: require a normal human observer at effective Vanilla era; every verified participant actually hidden by the same F14 player rule for that viewer; no observer/owned pet/totem combat, selected target, aura/DoT, pending missile, loot/quest/group credit or possible aggro/assist/cleave/AoE reach. Require exact positive evidence that Core viewer-deny + threat/visibility refresh is deployed and safe. If not: **visible**.
- The pure `ShouldConcealCreatureFromObserver` models this decision. The `ConcealmentDecisionChanged` helper models which changes would need refresh; **neither function performs a refresh**.

## Reviewed snapshot-generation guard (Part 20)

Independent reviewer [Grok #6603971174](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6603971174) found a hypothetical stale-cache failure. If another bot joins creature C, old per-observer "all participants hidden" facts could still conceal C when the newcomer is visible to that viewer. The pure policy now treats `ObserverSnapshot` as a **specific observer-creature pair** and binds it to `CreatureSnapshot.generation` through `checkedAgainstCreatureGeneration`. Require matching, nonzero generations or show the creature. A newly joined participant, active attack beginning/stopping, threat change, rights change, death or owner/era change must invalidate/rebuild the affected snapshot and its viewer-pair facts before the next visibility decision.

A generation value alone does **not** deliver immediate visibility or prevent a stale snapshot from being published. In the future Core integration, update these facts on the owning map thread and perform the appropriate client visibility refresh on the same transition; do not rely on shared unsynchronised `uint32` fields or a per-viewer threat-list walk. The current pure header/tests have no live state publication and cannot make this guarantee.

**Additional cross-creature identity guard (Part 21):** creature A and creature B can both have revision `1`. A reused viewer-target cache from A must NEVER hide B just because the revision numbers match. The pure header now requires a nonzero `CreatureSnapshot.creatureIdentityToken` to match `ObserverSnapshot.checkedAgainstCreatureIdentityToken` in addition to their generation match. Tests explicitly reject cross-target cache reuse and missing tokens even when all other safety facts are true, and accept a positively verified pair for B. A future implementation needs a stable **per-living-instance** token assigned by Core that changes on despawn/respawn and prevents GUID reuse from carrying stale permissions. The pure model supplies no GUID reader, token issuer, cross-thread cache or actual refresh.

## Grok re-review: preserve F19 source ownership and avoid stale deaths

Independent [Grok #6604459559](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6604459559) accepted the generation-bound pure model but found a **red inherited F19 strict source provenance check** at the previous head: the new unwired header was missing from its exact allowed-`src/` delta. The isolated #47 branch now explicitly lists only that one research header and rejects any executable `src/*.cpp` or alternate `src/*.h` file including it. Frozen paid/F19 source comparisons remain in place. A green specific exact-head F19 job is still required; earlier code-bearing model PASS never established that inherited CI was green.

**Critical game-time rule:** a snapshot version match cannot by itself prove the target is still alive or that the hidden RandomBot is still attacking it. The pure model therefore now additionally requires **two positive, live-at-evaluation facts**: `targetIsAliveAtThisEvaluation` and `eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation`. The future map-thread Core hook must read current `Unit` death state and the qualifying bot's actual victim/attack state at the time of each visibility decision, not copy these values from a cached creature entry. They default false and are tested against intentionally stale cached facts. A model check does NOT cause a client packet or immediate scene refresh: attack-stop, target switch and corpse death must still synchronously invalidate/update the right observer's visibility **before** an invisible corpse or hostile hit could occur. If either verification or immediate refresh is unavailable, do not conceal the creature.

**Cache ownership:** viewer-creature pair booleans are also affected by viewer position, group, quest rights, tier and combat status. Invalidate/recompute on those changes as well as creature generation changes. Never reuse generation zero; real implementation must handle wrap/reuse safely and publish snapshots coherently on the owning map thread. Unsynchronised counters or cross-thread PB lookups are not a safe solution.

**Conservative pilot scope:** `plainUnscriptedNonEliteHostileNpc` means a normal living NPC whose hostility/interaction safety is positively checked for the particular observer and target scenario. A faction reaction alone is not sufficient; unclear hostility is visible. Preserve stronger boss/rare/escort, pet, gameobject, loot and group exclusions until an owner-approved integration pilot.

## Required integration boundaries before runnable code

**IP** owns era policy and a safe positive cached RandomBot identity snapshot. **Core owner (not yet selected)** would provide a narrowly reviewed default-permit per-viewer **player and creature** visibility deny hook with suitable precedence, as well as visibility invalidation on threat-add, victim switch, exit combat, death and stage change. **Playerbots** remains unchanged until approved; owner has deferred a fork. **Naxx** F14 #143 and WHO #145 remain separate; do not duplicate or enable race-only concealment.

**Core runtime failure rule:** if the visibility callback cannot guarantee that the hidden NPC will not target or damage the viewer before refreshing visibility, don't conceal it. Early return normal visibility for GMs, uncertain states, special instances, scripted units, player-controlled targets, pets, summons and unreviewed bosses. No global phase/despawn, no AI replacement, no suppression of quest credit or loot, and no packet-only hide.

## Test and release gates

1. Pure C++ policy matrix with flag OFF, Vanilla/TBC, human BE/Draenei, verified/unverified rndbots, minions/unknown participants, player threat/tap/quest, auras/target/pets, aggro danger, transition/expired cache and absence of Core refresh; test `-DNDEBUG` too.
2. Independent source review by Grok and owning Core/Naxx integration review, including hot-path cost, cache concurrency and packet/interaction security.
3. Exact pinned **Worldserver native compilation and link** for the selected Core/IP/PB/Naxx/Services revisions; no old successful build counts for a new source HEAD.
4. Separate owner-approved staging game checks: stationary combat begin/end, new threat target, bot death/reset/logout, civilian player and group participation, player tier change, NPC corpse/loot, client reconnect, 8-map-thread timing, AoE/DoT and spam/ghost hits. No paid/RandomBot flag enabled just because the model passes.
5. **Before live test:** installed source/binary/config inventory, verified restorable complete character DB plus relevant config backups and rollback. **Before live enablement/deployment:** explicit CosmicCuddle approval and gameplay acceptance.

**Rollback today:** close/discard this isolated draft PR and branch. It has no runtime effect. Any future Core hook or stage writes require a distinct rollback and owner decision.
