# F14 event-driven visibility refresh — source-only generation model

**Owner:** CosmicCuddle. **Developer:** Maintenance. **Date:** 3 November 2026. **State:** UNWIRED / DEFAULT OFF. New policy header and portable tests only; no Core, Playerbots, server, client, database or configuration changes. The model is stacked on [F14 WHO cached-GUID draft #52](https://github.com/CosmicCuddle/mod-individual-progression/pull/52) exact `e24d6d7954ea0adee772dfa9a82c0e07a61d4e64`.

## What has been implemented

- `src/IndividualProgressionF14RefreshPolicy.h` is **pure C++**, with no Worldserver, Core, Packet, ScriptMgr or DB access. It tracks `creatureIdentityToken`, `combatGeneration`, creature alive state, eligible verified attacker status, current safety proof and completed observer refresh acknowledgement.
- `OnEvent` invalidates the earlier observer-creature proof and requests an **immediate visibility refresh** on attack begin/stop, attacker target switch, death, despawn, threat/tap/rights change, observer era/identity change, RandomBot login/logout, map/teleport transition and config reload. The model returns an advisory refresh-required flag; it **does not execute** a refresh.
- On creature death or despawn the candidate is marked not alive and its active attacker is cleared. A target switch clears the active attacker even if creature combat technically persists.
- Identity reuse on respawn or a generation counter overflow is rejected. Stale proof commits and out-of-order visibility acknowledgements cannot re-hide a corpse, a previously hidden target or a different creature.
- `CommitVerifiedFacts` revokes an older positive result **before** evaluating new evidence. `AcknowledgeObserverRefresh` only accepts exact matching target identity and generation. `MayUseConcealmentProof` remains false until both a complete positively verified evidence record and a refresh acknowledgement are present.
- `tests/f14_visibility_refresh_policy.cpp` tests **56 positive/negative assertions**. Local isolated test compilation and execution **PASSED on g++ and clang++ with C++17 and C++20**, `-Wall -Wextra -Werror -pedantic -O2 -DNDEBUG`. GitHub Actions performs independent four-way tests but is not a full Worldserver build or runtime proof.

## Source-level Core integration facts and safety blockers

- Historic `Grimfeather/azerothcore-wotlk@8daffdabe9202c228e45d833c9a1d399597a15cd`: [`Object.cpp` near line 3070](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Entities/Object/Object.cpp#L3070) implements `WorldObject::UpdateObjectVisibility` with `Acore::VisibleChangesNotifier` visiting nearby objects. [`Creature.cpp` around 1974](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Entities/Creature/Creature.cpp#L1974) handles `Creature::setDeathState` transitions; [`Player.cpp` near line 11827](https://github.com/Grimfeather/azerothcore-wotlk/blob/8daffdabe9202c228e45d833c9a1d399597a15cd/src/server/game/Entities/Player/Player.cpp#L11827) invokes `UpdateVisibilityForPlayer(true)` after map entry. These are **candidate APIs for review**, not proof that they can be invoked directly from IP hooks safely or that the client gets an immediate corpse update.
- There is **NO runtime integration** that reads threat/participants, monitors bot attack start/end/switch, safely snapshots per-viewer interactions or calls visibility refresh. The model cannot be enabled to conceal anything. The third `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION` build contract intentionally remains **UNDEFINED**, so the #52 experimental world+WHO visibility option is forcibly OFF even if someone writes `1` in a config.
- A real implementation must prove all callbacks execute on an appropriate map worker thread, produce complete per-observer event ordering, never access stale/deleted Unit pointers, preserve loot/quest/sparkle/tap/rights and threat/AI behavior, and **return each dead creature immediately to the appropriate Vanilla observer's view**. Human BE/Draenei, Altbots and unverified creatures remain visible by default; F14 applies **only to positively verified BE/Draenei RandomBots**.
- If an eligible bot stops attacking or changes target while a creature remains in combat, hiding must cease at once, not wait for the whole encounter to reset. Visibility refresh must avoid an invisible mob hitting or being hit by an unsuspecting nearby player.
- A fresh generation and identity are necessary but not sufficient; visibility computations also need an era-aware, per-viewer, thread-safe snapshot. Relevant GM, low-level, no-session/headless and paid-expansion observers require separate tests.
- Do not include this work in tonight's first controlled Naxx/IP/Services test cut. Owner-installed Core/PB versions and backup/rollback readiness are still unverified. No merging/deploying without CosmicCuddle approval.

## QA and rollback

- Run the new dedicated four-way GitHub CI source model only on the branch, plus historical combat model #47 (no runtime change).
- Future integration branch: implement owner-map-thread event collector, generation-stamped snapshots and map-safe per-viewer visibility invalidation; test negative cases (corpse/switch/loot), confirm WHO parity, then a full pinned native Worldserver link and actual multi-client staging test.
- Current rollback: discard the isolated unmerged child branch; **no server files or player data changed**.

**Coordination:** [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9) and [first-test readiness #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
