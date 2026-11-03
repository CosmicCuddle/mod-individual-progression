# F14 — Playerbots ResetQuests preservation prototype (CI-ONLY)

**Owner and authority:** CosmicCuddle. **Not authorised to install or enable on the server.** This is reviewable code carried as a unified patch under *Individual Progression's test-only draft branch*, not as a modification to the external Playerbots repository or an approved Playerbots fork.

**Historical source references:** `mod-playerbots/mod-playerbots@e88cc4099e69f35126b28942a412ca702d20453b`, Core `Grimfeather/azerothcore-wotlk@8daffdabe9202c228e45d833c9a1d399597a15cd`, IP #46 `26d5b7a1de30ad92a595aaf251fc14754cf54f32`. None proves the owner's installed source or current DB.

**Problem:** A full `PlayerbotFactory::Randomize(false)` calls `ResetQuests`, which removes already rewarded quest entries including IP progression markers `66001–66018`; a subsequent `SaveToDB` persists the loss. IP's optional F14 login and group-floor safeguards cannot recover a marker that no longer exists. See [coordination #11 B1](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6604947427) and [IP draft #48](https://github.com/CosmicCuddle/mod-individual-progression/pull/48). Full F14 stage changes remain **BLOCKED_CORE_PB_HOOK**.

## Proposed narrow four-file source patch

**File:** `tests/pb_patches/reset_earned_ip_markers_prototype.patch`. This patch is not imported or compiled into IP. The isolated workflow checks **exact** historical Playerbots source blobs, uses `git apply --check`, and applies it *only in a disposable CI checkout*. No PB branch, fork, PR, live configuration or quest data is changed.

| Historical Playerbots file | Proposed edit |
| --- | --- |
| `src/PlayerbotAIConfig.h` | Add an 18-bit `std::atomic<uint32> ResetQuestsPreserveMask{0}` with `<atomic>`; map-worker reads never touch a mutable container |
| `src/PlayerbotAIConfig.cpp` | Load proposed `Playerbots.ResetQuestsPreserveQuestIds` with fallback **empty string** into a *local temporary set* using the existing set loader, build a bitmask of only IDs 66001–66018, then atomically publish it once with release ordering. Reloading to empty publishes zero (stock reset), without mutating a shared set |
| `src/Bot/Factory/PlayerbotFactory.cpp` | In the existing `ResetQuests` loop, **before** removal: skip only quest IDs **66001–66018**, only if `GetQuestStatus(entry)==QUEST_STATUS_REWARDED` and its bit is present in the immutable atomic mask; otherwise continue the exact old removal sequence |
| `conf/playerbots.conf.dist` | Document new key with **empty value** and explicit owner-approval warning |

**Why this is conservative:** no new stage is awarded; a newly created Blood Elf/Draenei RandomBot with no earned markers remains without them. A quest that is active but not already rewarded is removed as before. Other quests reset as before even if mistakenly entered in the list (runtime range check limits preservation to 66001–66018). If disabled, `ResetQuests` preserves none and is identical in effect to the historical path. The update uses a **single atomic snapshot read at the beginning of each ResetQuests operation**, not a mutable set or Playerbots identity lookup. A config reload publishes a new mask atomically; the in-progress reset keeps its original mask for all marker decisions, while the next reset sees the new setting. This avoids mixed preservation of different markers during one reset.

**Outstanding policy detail:** the proposed narrow check applies to any bot that passes through `PlayerbotFactory::ResetQuests` **if an existing rewarded IP marker ID is explicitly configured**, including Altbots if such markers exist. It does *not* independently identify rndbots; an IP-era owner decision is needed before approval if only verified RandomBots are intended. Scope is more general than F14 visibility, which remains strictly verified RandomBots only. This is a proposal, not an owner policy decision.

**Why an atomic mask rather than a shared set:** The owner commonly runs multiple map threads. A mutable configured `std::set` could race against a `ResetQuests` read if config reload happened simultaneously. The prototype uses one atomic 18-bit value for the exact allowed range instead; configuration parsing happens off the hot path, and each reset copies the published mask **once before the quest loop**, preventing a mixed-policy reset after reload without iterator or memory ownership hazards. This is only a source-level concurrency design; actual module initialization/reload order still needs native and gameplay verification.

**Compatibility and safety review still necessary:** Need verify Core's `GetQuestStatus` returns `QUEST_STATUS_REWARDED` for stage-marker quests on real saved bots; pinned Core `PlayerQuest.cpp` implements this for nonrepeatable rewarded quests, but actual quest_template flags and database status must be verified. A check of the ID range is not proof that all marker templates exist, nor that their saved state is always returned exactly that way. Check external PB API/code at installed revision, multiple reset pathways, initialization/reload, group/level and RandomBot/Altbot rights. Existing archived stage loss cannot be recovered from this patch.

## Tests

- Pure `tests/pb_reset_keep_list_prototype.cpp` exercises absent/empty list, all 18 allowed IDs, partial list with invalid entries, rewarded vs active vs absent, no invented stage or ordinary quest retention, using standalone C++ and **no Worldserver**.
- `tests/test_ip_pb_keep_list_prototype.py` checks patch scope and exact constructs plus that the applied pinned source matches expected guards. No status mutation.
- `.github/workflows/ip-pb-reset-marker-keep-list-prototype-20261103.yml` applies patch to an **ephemeral** pinned source copy only after source blob and no-IP-executable-edit checks; runs Python and GCC/Clang × C++17/C++20 matrix. A passing model+patch application does **not** establish that modified Playerbots compiles inside Worldserver, has correct saved quest semantics or works in-game.

## Release / future work

1. Obtain CosmicCuddle's approval for the **specific** Playerbots/Core patch, policy scope and any fork or deployment workflow before applying to real source. The owner's earlier preference was to defer establishing a Playerbots fork; no source fork is created here.
2. Verify exact installed PB/Core/IP version tuple and custom changes. Prepare buildable source and config/DB backups, rollback (code revert, set key back empty, restore DB only when necessary), and check SQL/quest status.
3. Compile and link the full pinned Core+Playerbots+IP+Naxx+Services integration **with the patch applied on a disposable runner**. Include negative tests for default empty, ordinary/active quests, and live bot identity, plus positive cases for already rewarded `66001–66018`.
4. Stage with disposable bot characters and owner approval. Confirm rewards remain through full Randomize, ClearEverything, admin init, relog, multiple resets and restart; quest log and loot unaffected.
5. Never make a production merge/config edit/quest re-award based only on the C++ model. The server currently has no approved preservation fix or F14 stage-changing release.

**Rollback today:** close/discard this proposal PR; no PB executable, config, database or live-server change exists to roll back. The real PB preservation blocker stays open until separately authorised.
