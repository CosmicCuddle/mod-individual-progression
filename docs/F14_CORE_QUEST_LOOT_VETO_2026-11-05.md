# F14 viewer per-creature quest-item loot veto (DRAFT/OFF)

**Owner:** CosmicCuddle
**Maintainer:** Server-Side-Maintenance CS01 while Claude is on an owner-confirmed break
**Date:** 5 November 2026
**Status:** isolated disposable Core development, no live installation

## What this changes

The pinned [#101](https://github.com/CosmicCuddle/mod-individual-progression/pull/101) real Core helper already checks viewer target/pet/charm/guardian/group/aura, direct and alternate creature kill quest requirements. This additive [#102](https://github.com/CosmicCuddle/mod-individual-progression/pull/102) prototype checks **quest-item loot** through **the actual pinned AzerothCore LootStore implementation**, not a fabricated item-to-creature list.

After validating exact #99/#100/#101 helper source anchors, the new patcher adds a single negative-only condition in disposable Grim Core `PlayerUpdates.cpp`:

`if (questCreature->lootid && LootTemplates_Creature.HaveQuestLootForPlayer(questCreature->lootid, viewer)) return true;`

It includes `LootMgr.h` and reuses the already-checked `CreatureTemplate const* questCreature`. This asks the real Core's per-player creature loot store whether it contains quest drops for that viewer. A TRUE result means **keep the creature visible**. A FALSE result **must NEVER be interpreted as safe concealment**: conditions, item loot, personal group rights, drops from other sources, scripted quests, skinning, active loot, client object state, and many forms of assist/kill credit can still require visibility.

Both per-viewer direct/batched F14 hide query sites and all previous guards remain unmodified. The query defaults FALSE because there is no registered positive listener. Thus no creature becomes invisible from this patch, and no prior-hidden ledger is cleared.

## What is tested

- Exactly six new or append-only repository paths: this note, the Custom Modifications Register, one Core patcher, one C++ behavioural model, one Python source/immutability test, and a disposable CI workflow. No protected Core fork, original IP executable files, Naxx/Services/PB source, SQL, DBC, client, configuration, database or host changes.
- Immutable prior #99/#100/#101 patcher blobs and unchanged Claude #94 viewer protocol.
- Real pinned `LootStore::HaveQuestLootForPlayer(uint32, Player const*) const`, `LootTemplates_Creature`, and `CreatureTemplate::lootid` APIs.
- The C++ test compiles the **exact layered original Core helper** using interface stubs in GCC/Clang C++17/20 with and without `NDEBUG`. Previous 48 behavioural checks plus quest-loot positive/negative/null-loot-id/unrelated-loot-id checks.
- Full native disposable Grim Core/IP/Playerbots/Naxx #292/Character Services #16 Worldserver compilation and link required in the same workflow; **do not claim success until exact-head GitHub CI completes**. A compile does not imply in-game behaviour.

## Approval and rollback

All F14/paid experimental flags stay OFF. The final owner-approved combined test still requires a complete authoritative human-player rights census (quests, loot, group, combat, pets, assist/DoTs and spells), positive verified selected BE/Draenei **headless RandomBot-only** identity, safe per-viewer creature hide and client-visible reappearance/targetability after stops, death, corpse, logout/map and metadata overflow, exact installed source/config/DB/DBC/client inventory, tested backups and CosmicCuddle's approval.

Rollback: close/reject this isolated development draft. No host or protected code is changed.
