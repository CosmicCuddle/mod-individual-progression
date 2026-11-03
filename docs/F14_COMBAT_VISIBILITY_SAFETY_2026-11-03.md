# F14 combat-target creature visibility — isolated safety proposal

**Owner:** CosmicCuddle. **Status:** UNWIRED code model, not a functioning feature, not merged or deployed. **Source baseline:** IP #46 `cbc8de8e`; F14 source-only PR #47. **Review:** [Grok CS02-IP 6603455946](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6603455946) and [6603460102](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6603460102). **Coordination:** [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9).

## Owner's requested outcome

Avoid a visible NPC appearing to fight thin air after a *positively verified* Blood Elf/Draenei **RandomBot** is hidden from a **Vanilla** observer. Do not hide ordinary human Blood Elf/Draenei players, Altbots, Selfbots or unknown identities. TBC+ observers retain normal visibility. This is a requested player-experience improvement, not approval of broad NPC concealment or any production change.

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
