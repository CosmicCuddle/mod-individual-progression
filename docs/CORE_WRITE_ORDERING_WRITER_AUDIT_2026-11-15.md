# Writer audit for design C (15 Nov 2026, Core `21ff1973` + modules IP/PB/CS)

**Requested by** CS01, Issue #13 comment 6892952406: audit all direct, async and module writers, and the multi-character keys, before any release.

**Method:**
1. Every `CharacterDatabase` prepared statement that writes (`INSERT`/`UPDATE`/`DELETE`/`REPLACE`) to one of these tables: `characters`, `character_settings`, `character_inventory`, `item_instance`, `mail`, `mail_items`, `auctionhouse`, `character_queststatus_rewarded`, `guild_bank_item`.
2. Every call site of those statements in `src/` and `modules/`.
3. Raw-SQL writes to the same tables.

This is a **source inventory**: a statement appearing here doesn't mean it's unordered today. Each one needs a key decision.

## Summary
- **70 prepared write statements**, called from 27 files:
  - player storage, settings and misc;
  - `Item.cpp`;
  - trade, mail, auction-house and character handlers;
  - `Guild.cpp`, `MailMgr.cpp`, `AuctionHouseMgr.cpp`, `ArenaTeamMgr.cpp`, `AccountMgr.cpp`, `BanMgr.cpp`, `SocialMgr.cpp`, `WorldSession.cpp`;
  - GM commands (`cs_character`, `cs_item`, `cs_mail`, `cs_misc`, `cs_quest`, `cs_reset`, `cs_achievement`);
  - Playerbots `CheckMailAction.cpp`.
- **Multi-character writers** (need key sets): trade (`TradeHandler`), mail send/return (sender and receiver), the auction house (seller, bidder, buyer), the guild bank (members), and GM commands targeting another character.
- **Raw SQL:**
  - **Startup/maintenance:** `Main.cpp` online reset, `World.cpp` mail cleanup, `InstanceSaveMgr`, `ObjectMgr` item-guid cleanup, `GroupMgr`/`GuildMgr` orphans. These run before players log in. Order them before admission, or leave them unkeyed and document that.
  - **Runtime, needing keys:**
    - `cs_character.cpp` `UPDATE characters SET account` (character transfer);
    - Playerbots `HireAction.cpp` `UPDATE characters SET account`;
    - Playerbots `RandomPlayerbotFactory.cpp` bulk deletes of bot characters and orphans (bot accounts; run at startup/maintenance);
    - Playerbots `GuildTaskMgr.cpp` mail delete.
- **IP:** writes only through `Player` (settings via `UpdatePlayerSetting` plus `SaveToDB`); its only direct SQL is the strict sync **read**, which C replaces with a holder read.
- **Character Services:** no direct writes to these tables found (it goes through Player/IP APIs). Re-check when its paid v2 consumer lands.

## Prepared write statements and call sites
| Table | Prepared statement | Files | Call sites |
|---|---|---|---|
| `auctionhouse` | `CHAR_DEL_AUCTION` | 1 | game/AuctionHouse/AuctionHouseMgr.cpp |
| `auctionhouse` | `CHAR_INS_AUCTION` | 1 | game/AuctionHouse/AuctionHouseMgr.cpp |
| `auctionhouse` | `CHAR_UPD_AUCTION_BID` | 1 | game/Handlers/AuctionHouseHandler.cpp |
| `character_inventory` | `CHAR_DEL_CHAR_INVENTORY` | 5 | game/Entities/Item/Item.cpp, game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, scripts/Commands/cs_item.cpp, scripts/Commands/cs_quest.cpp |
| `character_inventory` | `CHAR_DEL_CHAR_INVENTORY_BY_BAG_SLOT` | 1 | game/Entities/Player/PlayerStorage.cpp |
| `character_inventory` | `CHAR_DEL_CHAR_INVENTORY_BY_ITEM` | 4 | game/Entities/Item/Item.cpp, game/Entities/Player/PlayerStorage.cpp, scripts/Commands/cs_item.cpp, scripts/Commands/cs_quest.cpp |
| `character_inventory` | `CHAR_REP_INVENTORY_ITEM` | 1 | game/Entities/Player/PlayerStorage.cpp |
| `character_queststatus_rewarded` | `CHAR_DEL_CHAR_QUESTSTATUS_REWARDED` | 4 | game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, game/Handlers/CharacterHandler.cpp, scripts/Commands/cs_quest.cpp |
| `character_queststatus_rewarded` | `CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST` | 3 | game/Entities/Player/PlayerStorage.cpp, game/Handlers/CharacterHandler.cpp, scripts/Commands/cs_quest.cpp |
| `character_queststatus_rewarded` | `CHAR_INS_CHAR_QUESTSTATUS_REWARDED` | 2 | game/Entities/Player/PlayerStorage.cpp, scripts/Commands/cs_quest.cpp |
| `character_queststatus_rewarded` | `CHAR_UPD_CHAR_QUESTSTATUS_REWARDED_ACTIVE` | 1 | game/Handlers/CharacterHandler.cpp |
| `character_queststatus_rewarded` | `CHAR_UPD_CHAR_QUESTSTATUS_REWARDED_ACTIVE_BY_QUEST` | 1 | game/Handlers/CharacterHandler.cpp |
| `character_queststatus_rewarded` | `CHAR_UPD_CHAR_QUESTSTATUS_REWARDED_FACTION_CHANGE` | 1 | game/Handlers/CharacterHandler.cpp |
| `character_settings` | `CHAR_DEL_CHAR_SETTINGS` | 1 | game/Entities/Player/Player.cpp |
| `character_settings` | `CHAR_REP_CHAR_SETTINGS` | 1 | game/Entities/Player/PlayerSettings.cpp |
| `characters` | `CHAR_DEL_CHARACTER` | 3 | game/Accounts/AccountMgr.cpp, game/Entities/Player/Player.cpp, game/Entities/Player/SocialMgr.cpp |
| `characters` | `CHAR_INS_CHARACTER` | 5 | game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, game/Entities/Player/SocialMgr.cpp, game/Misc/BanMgr.cpp, scripts/Commands/cs_quest.cpp |
| `characters` | `CHAR_RES_CHAR_TITLES_FACTION_CHANGE` | 1 | game/Handlers/CharacterHandler.cpp |
| `characters` | `CHAR_UDP_CHAR_ARENA_POINTS` | 3 | game/Entities/Player/Player.cpp, scripts/Commands/cs_item.cpp, scripts/Commands/cs_quest.cpp |
| `characters` | `CHAR_UDP_CHAR_ARENA_POINTS_ACCUMULATIVE` | 2 | scripts/Commands/cs_item.cpp, scripts/Commands/cs_quest.cpp |
| `characters` | `CHAR_UDP_CHAR_HONOR_POINTS` | 3 | game/Entities/Player/Player.cpp, scripts/Commands/cs_item.cpp, scripts/Commands/cs_quest.cpp |
| `characters` | `CHAR_UDP_CHAR_HONOR_POINTS_ACCUMULATIVE` | 2 | scripts/Commands/cs_item.cpp, scripts/Commands/cs_quest.cpp |
| `characters` | `CHAR_UDP_CHAR_MONEY` | 2 | game/Entities/Player/PlayerStorage.cpp, scripts/Commands/cs_quest.cpp |
| `characters` | `CHAR_UDP_CHAR_MONEY_ACCUMULATIVE` | 1 | scripts/Commands/cs_quest.cpp |
| `characters` | `CHAR_UDP_RESTORE_DELETE_INFO` | 1 | scripts/Commands/cs_character.cpp |
| `characters` | `CHAR_UPD_ACCOUNT_ONLINE` | 1 | game/Server/WorldSession.cpp |
| `characters` | `CHAR_UPD_ADD_AT_LOGIN_FLAG` | 6 | game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, game/Handlers/CharacterHandler.cpp, scripts/Commands/cs_achievement.cpp, scripts/Commands/cs_character.cpp, scripts/Commands/cs_reset.cpp |
| `characters` | `CHAR_UPD_ALL_ARENA_POINTS` | 1 | scripts/Commands/cs_reset.cpp |
| `characters` | `CHAR_UPD_ALL_AT_LOGIN_FLAGS` | 1 | scripts/Commands/cs_reset.cpp |
| `characters` | `CHAR_UPD_ALL_HONOR_POINTS` | 1 | scripts/Commands/cs_reset.cpp |
| `characters` | `CHAR_UPD_CHARACTER` | 4 | game/Entities/Player/Player.cpp, game/Entities/Player/PlayerMisc.cpp, game/Entities/Player/SocialMgr.cpp, game/Misc/BanMgr.cpp |
| `characters` | `CHAR_UPD_CHARACTER_POSITION` | 1 | game/Entities/Player/PlayerMisc.cpp |
| `characters` | `CHAR_UPD_CHAR_ARENA_POINTS` | 1 | game/Battlegrounds/ArenaTeamMgr.cpp |
| `characters` | `CHAR_UPD_CHAR_NAME_AT_LOGIN` | 1 | game/Handlers/CharacterHandler.cpp |
| `characters` | `CHAR_UPD_CHAR_ONLINE` | 1 | game/Handlers/CharacterHandler.cpp |
| `characters` | `CHAR_UPD_CHAR_RACE` | 1 | game/Handlers/CharacterHandler.cpp |
| `characters` | `CHAR_UPD_CHAR_REMOVE_GHOST` | 1 | game/Entities/Player/Player.cpp |
| `characters` | `CHAR_UPD_CHAR_TAXIMASK` | 1 | game/Handlers/CharacterHandler.cpp |
| `characters` | `CHAR_UPD_CHAR_TAXI_PATH` | 1 | game/Handlers/CharacterHandler.cpp |
| `characters` | `CHAR_UPD_CHAR_TITLES_FACTION_CHANGE` | 1 | game/Handlers/CharacterHandler.cpp |
| `characters` | `CHAR_UPD_DELETE_INFO` | 1 | game/Entities/Player/Player.cpp |
| `characters` | `CHAR_UPD_GENDER_AND_APPEARANCE` | 1 | game/Entities/Player/PlayerMisc.cpp |
| `characters` | `CHAR_UPD_LEVEL` | 1 | scripts/Commands/cs_character.cpp |
| `characters` | `CHAR_UPD_NAME_BY_GUID` | 1 | scripts/Commands/cs_character.cpp |
| `characters` | `CHAR_UPD_REM_AT_LOGIN_FLAG` | 1 | game/Entities/Player/Player.cpp |
| `characters` | `CHAR_UPD_XP_ACCUMULATIVE` | 1 | scripts/Commands/cs_quest.cpp |
| `characters` | `CHAR_UPD_ZONE` | 1 | game/Entities/Player/Player.cpp |
| `guild_bank_item` | `CHAR_DEL_GUILD_BANK_ITEM` | 1 | game/Guilds/Guild.cpp |
| `guild_bank_item` | `CHAR_DEL_GUILD_BANK_ITEMS` | 1 | game/Guilds/Guild.cpp |
| `guild_bank_item` | `CHAR_DEL_NONEXISTENT_GUILD_BANK_ITEM` | 1 | game/Guilds/Guild.cpp |
| `guild_bank_item` | `CHAR_INS_GUILD_BANK_ITEM` | 1 | game/Guilds/Guild.cpp |
| `item_instance` | `CHAR_DEL_ITEM_INSTANCE` | 8 | game/Entities/Item/Item.cpp, game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, game/Handlers/TradeHandler.cpp, game/Mails/Mail.cpp, game/Mails/MailMgr.cpp, scripts/Commands/cs_item.cpp, scripts/Commands/cs_quest.cpp |
| `item_instance` | `CHAR_DEL_ITEM_INSTANCE_BY_OWNER` | 1 | game/Entities/Player/Player.cpp |
| `item_instance` | `CHAR_REP_ITEM_INSTANCE` | 1 | game/Entities/Item/Item.cpp |
| `item_instance` | `CHAR_UPD_CHAR_INVENTORY_FACTION_CHANGE` | 1 | game/Handlers/CharacterHandler.cpp |
| `item_instance` | `CHAR_UPD_ITEM_COUNT` | 1 | scripts/Commands/cs_misc.cpp |
| `item_instance` | `CHAR_UPD_ITEM_INSTANCE` | 1 | game/Entities/Item/Item.cpp |
| `item_instance` | `CHAR_UPD_ITEM_INSTANCE_ON_LOAD` | 1 | game/Entities/Item/Item.cpp |
| `item_instance` | `CHAR_UPD_ITEM_OWNER` | 3 | game/AuctionHouse/AuctionHouseMgr.cpp, game/Mails/Mail.cpp, game/Mails/MailMgr.cpp |
| `mail` | `CHAR_DEL_MAIL` | 6 | game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, game/Handlers/MailHandler.cpp, game/Mails/MailMgr.cpp, mod-playerbots/src/Ai/Base/Actions/CheckMailAction.cpp, scripts/Commands/cs_mail.cpp |
| `mail` | `CHAR_DEL_MAIL_BY_ID` | 6 | game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, game/Handlers/MailHandler.cpp, game/Mails/MailMgr.cpp, mod-playerbots/src/Ai/Base/Actions/CheckMailAction.cpp, scripts/Commands/cs_mail.cpp |
| `mail` | `CHAR_INS_MAIL` | 1 | game/Mails/Mail.cpp |
| `mail` | `CHAR_UPD_MAIL` | 2 | game/Entities/Player/PlayerStorage.cpp, game/Mails/MailMgr.cpp |
| `mail` | `CHAR_UPD_MAIL_RETURNED` | 1 | game/Mails/MailMgr.cpp |
| `mail_items` | `CHAR_DEL_INVALID_MAIL_ITEM` | 2 | game/Entities/Player/PlayerStorage.cpp, scripts/Commands/cs_mail.cpp |
| `mail_items` | `CHAR_DEL_MAIL_ITEM` | 6 | game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, game/Handlers/MailHandler.cpp, game/Mails/MailMgr.cpp, mod-playerbots/src/Ai/Base/Actions/CheckMailAction.cpp, scripts/Commands/cs_mail.cpp |
| `mail_items` | `CHAR_DEL_MAIL_ITEMS` | 1 | game/Entities/Player/Player.cpp |
| `mail_items` | `CHAR_DEL_MAIL_ITEM_BY_ID` | 6 | game/Entities/Player/Player.cpp, game/Entities/Player/PlayerStorage.cpp, game/Handlers/MailHandler.cpp, game/Mails/MailMgr.cpp, mod-playerbots/src/Ai/Base/Actions/CheckMailAction.cpp, scripts/Commands/cs_mail.cpp |
| `mail_items` | `CHAR_INS_MAIL_ITEM` | 1 | game/Mails/Mail.cpp |
| `mail_items` | `CHAR_UPD_MAIL_ITEM_RECEIVER` | 1 | game/Mails/MailMgr.cpp |
