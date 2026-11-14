# F14 negative-only current HUMAN viewer session veto — disposable Core, OFF

**Owner:** CosmicCuddle
**Maintainer:** Server-Side-Maintenance CS01-IP
**Date:** 6 November 2026
**Status:** independent stacked DRAFT/source contract. **No positive F14 hiding policy. No Core fork/live server modification.**

## Requirement

Only eligible headless **Blood Elf and Draenei RandomBots** may ever make a non-Vanilla NPC temporarily hidden from **normal human Vanilla viewers**, while preserving combat, quest, loot, corpse and client-visible restoration. **Never hide a human player or Altbot**; do not mistakenly hide NPCs from headless RandomBot viewers or active GMs. A future F14-specific positive policy must separately verify current normal-human/Vanilla viewer rights and owner-authorized selected bot source.

## Pinned evidence, and what it is NOT

- [Grim Core `Player.h` at 0d754d](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Entities/Player/Player.h) provides `GetSession() const` and `IsGameMaster() const`; [`WorldSession.h`](https://github.com/Grimfeather/azerothcore-wotlk/blob/0d754d1921261dfd939f91060ebc022eae280f71/src/server/game/Server/WorldSession.h) provides `IsHeadless() const` backed by `_headless`. The source verifies APIs but **not** a permanent or cryptographically authenticated human classification. A non-headless session is **necessary, never sufficient**. Active GM flag checking does not establish full account security rank.
- [Pinned Playerbots `RandomPlayerbotMgr.cpp`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/RandomPlayerbotMgr.cpp) `IsRandomBot` uses account list plus mutable `currentBots`; that alone is **NOT** an active headless-session/selected-bot authorization. It must not be called from arbitrary map-worker visibility paths. [Existing IP login identity cache](https://github.com/CosmicCuddle/mod-individual-progression/pull/110) verifies race/headless bot account on login and locks a monotonically increasing epoch for lifetime checks, with current policy still OFF. Preserve this without introducing runtime roster races.
- [#110](https://github.com/CosmicCuddle/mod-individual-progression/pull/110) already adds a real **negative** current `Map::Update` pre-query veto in direct and batched `Player::UpdateVisibilityOf`. This child is an additional conservative viewer-side negative requirement, *not* a positive selected RandomBot or a completed historical pair/restore provider.

## Disposable code only

The new `tests/core_patches/apply_f14_human_viewer_prequery_veto.py` pre-validates exact pinned `Player.h`, `WorldSession.h`, `Map.h` and previous #110 two-path F14 queries before it changes the **single** CI checkout file `src/server/game/Entities/Player/PlayerUpdates.cpp`. It adds an explicit `#include "WorldSession.h"`, then, **between** #110 map-frame gate and the F14 positive policy query, short-circuits to FALSE whenever `!GetSession()`, `GetSession()->IsHeadless()`, or `IsGameMaster()`. It leaves `CanSeeOrDetect`, normal Core visibility, creature/pet guards, #99–#102 existing human rights vetoes, #107 pre-link and #108 map frame unchanged.

**Consequence:** without a current non-headless session, the *F14-only* positive policy is **not consulted**, even if a future listener would otherwise say TRUE. The baseline Core still decides normal visibility. A headless viewer or active GM cannot be selected for this F14 concealment. An ordinary connected human viewer is still **not authoritatively approved** without normal-account/Vanilla-era/rights checks.

**No active positive hide provider exists; default F14 remains FALSE.** No source in this child changes the owner-installed AzerothCore fork, IP C++ gameplay source, original Claude #94 restoration ledger, #103 send-attempt adapter, Playerbots, Naxx, Character Services, any paid/experimental config, world/character/auth SQL, DBC/client or host.

## Tests and assurance boundary

- New source fixture tests check both **direct and batched** F14 paths and exact ordering: #110 map context → session and nonheadless + non-GM checks → positive query → original human-rights veto → F14 pre-link callback → viewer link. Invalid/missing Core method, a malformed second path or repeated patch refuses with **no write**.
- Pinned source tests check exact WorldSession and Player declarations, original Core visibility symbols and historical Playerbots roster behavior; nothing is promoted to account/connection proof by reading a Boolean.
- The new pure C++ model enumerates **4,096 fact combinations across two direct/batched paths = 8,192 cases** in 8 GCC/Clang C++17/C++20 regular/NDEBUG variants. Checks stay active in NDEBUG. Both the actual default-FALSE provider and a hypothetical future TRUE provider must fail visible for missing/headless/active-GM viewer, off-map, stale or other missing prerequisites.
- **Source-only CI does not compile the actual full Core or run Worldserver.** Native C++ integration must be independently confirmed using #110's pinned full-stack overlay recipe before claiming native PASS. Even native PASS would not prove in-game 3.3.5a client restoration.

## Risk, decisions, rollback

The Core `GetSession()` and GM flag may be valid but insufficient to establish **normal-human/Vanilla subscription**; additional complete viewer entitlement evidence, full combat/quest/loot/tap/DoT/group rights, exclusive owning-map thread, selected BE/Draenei headless RNDBOT participants, historical prior-hidden pairs and 3.3.5a client OUT/CREATE/targetability are all still missing. Missing/unknown => **visible**. Future policy must never hide a human player or Altbot.

Rollback is rejecting/closing this child and removing the temporary CI overlay, never altering the live server. No merge/deployment or owner-host/DB/DBC/client operation is approved. One complete combined in-game test only after installed-version inventory, recoverable backups and CosmicCuddle approval.

**Coordination:** [F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [combined readiness Issue #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
