/*
    Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
*/

#include "IndividualProgression.h"
#include "IndividualProgressionGroupPolicy.h"
#include "IndividualProgressionF14LoginPolicy.h"
#include "IndividualProgressionF14StageFloorPolicy.h"
#include "naxxramas_40.h"
#include "Mail.h"
#include <mutex>
#include <limits>
#include "ReputationMgr.h"
#include "WorldSession.h"
#include "PetDefines.h"

IndividualProgression* IndividualProgression::instance()
{
    static IndividualProgression instance;
    return &instance;
}

uint8 IndividualProgression::GetPlayerProgressionFromQuests(Player* player) const
{
    if (!player || !player->IsInWorld())
        return 0;

    uint8 progressionLevel = 0;
    for (uint8 i = PROGRESSION_MOLTEN_CORE; i <= PROGRESSION_WOTLK_TIER_5; ++i)
    {
        uint32 PROGRESSION_QUEST = 66000 + i;
        if (player->GetQuestStatus(PROGRESSION_QUEST) == QUEST_STATUS_REWARDED)
            progressionLevel = i;
    }
    return progressionLevel;
}

bool IndividualProgression::hasPassedProgression(Player* player, ProgressionState state) const
{
    if (!enabled || !state || !player || !player->IsInWorld())
        return false;

    if (progressionLimit && (state > progressionLimit))
        return false;

    // Same as the rewarded stage unless the paid expansion feature caps an
    // unpaid Kil'jaeden (13) character to TBC access (12).
    return sIndividualProgression->GetEffectiveProgression(player) >= state;
}

bool IndividualProgression::isBeforeProgression(Player* player, ProgressionState state)
{
    if (!state || !player || !player->IsInWorld())
        return false;

    return sIndividualProgression->GetEffectiveProgression(player) < state;
}

void IndividualProgression::UpdateProgressionState(Player* player, ProgressionState newState) const
{
    if (!enabled || !newState || !player || !player->IsInWorld())
        return;

    if (progressionLimit && newState > progressionLimit)
        return;

    // Paid expansion (default OFF): automatic progression may not open an era
    // this character has not bought. The grant is refused, not partially applied.
    if (!sIndividualProgression->IsAutomaticGrantAllowed(player, newState))
        return;

    uint8 currentState = GetPlayerProgressionFromQuests(player);
    if (newState > currentState)
    {
        for (uint8 i = currentState; i <= newState; ++i)
        {
            uint32 PROGRESSION_QUEST = 66000 + i;
            Quest const* quest = sObjectMgr->GetQuestTemplate(PROGRESSION_QUEST);

            if (!quest)
                continue;

            player->AddQuest(quest, nullptr);
            player->CompleteQuest(PROGRESSION_QUEST);
            player->RewardQuest(quest, 0, player, false, false);
        }
    }
}

void IndividualProgression::ForceUpdateProgressionState(Player* player, ProgressionState newState)
{
    if (!player || !player->IsInWorld())
        return;

    // F14 stage-8 floor durability (default OFF): every IP-owned route that
    // could lower an earned stage funnels through here. A refused write is a
    // no-op: the current earned stage stays exactly as it is.
    if (sIndividualProgression->BlocksF14VerifiedStageFloorDowngrade(player, static_cast<uint8>(newState)))
    {
        uint8 const keptStage = sIndividualProgression->GetPlayerProgressionFromQuests(player);
        if (IndividualProgressionF14StageFloorPolicy::IsHistoricOverCapStage(keptStage, sIndividualProgression->progressionLimit))
            LOG_INFO("module", "IndividualProgression F14: refused progression write {} for verified RandomBot guid {}; preserved historic over-cap stage {} (limit {}) untouched",
                static_cast<uint32>(newState), player->GetGUID().GetCounter(), static_cast<uint32>(keptStage), sIndividualProgression->progressionLimit);
        else
            LOG_INFO("module", "IndividualProgression F14: refused progression write {} for verified RandomBot guid {} (earned TBC stage {} kept)",
                static_cast<uint32>(newState), player->GetGUID().GetCounter(), static_cast<uint32>(keptStage));
        return;
    }

    if (!newState && newState != 0)
        return;

    // remove all hidden progression quests first
    for (uint8 i = PROGRESSION_MOLTEN_CORE; i <= PROGRESSION_WOTLK_TIER_5; ++i)
    {
        uint32 PROGRESSION_QUEST = 66000 + i;
        if (player->GetQuestStatus(PROGRESSION_QUEST) == QUEST_STATUS_REWARDED)
            player->RemoveRewardedQuest(PROGRESSION_QUEST);
    }

    for (uint8 i = PROGRESSION_MOLTEN_CORE; i <= newState; ++i)
    {
        uint32 PROGRESSION_QUEST = 66000 + i;
        Quest const* quest = sObjectMgr->GetQuestTemplate(PROGRESSION_QUEST);

        if (!quest)
            continue;

        player->AddQuest(quest, nullptr);
        player->CompleteQuest(PROGRESSION_QUEST);
        player->RewardQuest(quest, 0, player, false, false);
    }

    sIndividualProgression->GrantAttunements(player, newState);
}

void IndividualProgression::CheckAdjustments(Player* player) const
{
    if (!enabled || !player || !player->IsInWorld())
        return;

    if (player->getClass() == CLASS_HUNTER)
    {
        // Remove the 15% built-in ranged haste that was added to hunters in WotLK - This lets us add haste spells back to quivers
        player->RemoveAura(RANGED_HASTE_SPELL);
        player->CastSpell(player, RANGED_HASTE_SPELL, false);
    }

    player->RemoveAura(HP_AURA_SPELL);
    player->RemoveAura(ABSORB_SPELL);
}

float IndividualProgression::ComputeVanillaAdjustment(uint8 playerLevel, float configAdjustmentValue)
{
    float adjustmentApplyPercent = (float(playerLevel) - 10.0f) / 50.0f;
    return playerLevel > 10 ? 1.0f - ((1.0f - configAdjustmentValue) * adjustmentApplyPercent) : 1;
}

// Return the highest progression level achieved by an account
uint8 IndividualProgression::GetAccountProgression(uint32 accountId)
{
    uint8 progressionLevel = 0;

    uint32 minQuest = 66000 + PROGRESSION_MOLTEN_CORE;
    uint32 maxQuest = 66000 + PROGRESSION_WOTLK_TIER_5;

    // Query rewarded hidden progression quests for all characters on the account
    QueryResult result = CharacterDatabase.Query(
        "SELECT cc.quest FROM character_queststatus_rewarded cc JOIN characters c ON cc.guid = c.guid WHERE c.account = {} AND cc.quest BETWEEN {} AND {};",
        accountId, minQuest, maxQuest);

    if (result)
    {
        do
        {
            uint32 questId = (*result)[0].Get<uint32>();
            uint8 level = uint8(questId - 66000);
            if (level > progressionLevel)
                progressionLevel = level;
        } while (result->NextRow());
    }
    return progressionLevel;
}

void IndividualProgression::UpdateAccountReputation(uint32 factionId, uint32 accountId, Player* player)
{
    if (!factionId || !accountId || !player || !player->IsInWorld())
        return;

    Group* group = player->GetGroup();
    uint32 account = player->GetSession()->GetAccountId();
    std::string factionName = sFactionStore.LookupEntry(factionId)->name[0];

    if (!group)
        return;

    if (player->GetReputationRank(factionId) < REP_NEUTRAL) // don't increase rep unless the player is at least neutral with the faction
        return;

    uint32 curRep = player->GetReputationMgr().GetReputation(factionId);
    uint32 newRep = 0;

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || member->GetSession()->GetAccountId() != accountId)
            continue;

        if (member->GetReputationRank(factionId) < REP_NEUTRAL)
            continue;

        uint32 repAmount = member->GetReputationMgr().GetReputation(factionId);

        if (repAmount > newRep)
            newRep = repAmount;
    }

    ChatHandler(player->GetSession()).PSendSysMessage("Current {} Rep = {} ({})", factionId, curRep, factionName);
    ChatHandler(player->GetSession()).PSendSysMessage("Highest {} Rep = {} ({})", factionId, newRep, factionName);

    if (newRep > curRep)
    {
        uint32 addRep = newRep - curRep;

        player->GetReputationMgr().ModifyReputation(sFactionStore.LookupEntry(factionId), addRep);
        // ChatHandler(player->GetSession()).PSendSysMessage("Reputation with {} increased by {}.", factionName, addRep);
    }
}

void IndividualProgression::UpdateGroupAttunement(Player* player, std::string location)
{
    if (!player || !player->IsInWorld() )
        return;

    if (location.empty())
        return;

    Group* group = player->GetGroup();

    if (!group)
        return;

    if (location == "onyxia40" || location == "onyxia")
    {
        if (player->HasItemCount(ITEM_DRAKEFIRE_AMULET))
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || sIndividualProgression->isBotAccount(member))
                    continue;

                if (member->GetLevel() < 50)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff{}|r needs to be at least level 50.", member->GetName());
                    continue;
                }

                if (!member->HasItemCount(ITEM_DRAKEFIRE_AMULET))
                {
                    if (member->HasItemCount(ITEM_DRAKEFIRE_AMULET, 1, true))
                    {
                        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff{}|r has the Drakefire Amulet in their bank.", member->GetName());
                        continue;
                    }

                    member->AddItem(ITEM_DRAKEFIRE_AMULET, 1);
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff{}|r received the Drakefire Amulet.", member->GetName());
                }
            }
            return;
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage("You must have the Drakefire Amulet in your inventory to use this command.");
            return;
        }
    }
    else if (location == "bt" || location == "blacktemple")
    {
        if (player->HasItemCount(ITEM_MEDALLION_OF_KARABOR) || player->HasItemCount(ITEM_BLESSED_MEDALLION_OF_KARABOR))
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || sIndividualProgression->isBotAccount(member))
                    continue;

                if (member->GetLevel() < 70)
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff{}|r needs to be at least level 70.", member->GetName());
                    continue;
                }

                if (isBeforeProgression(member, PROGRESSION_TBC_TIER_2))
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff{}|r needs to have progression level 10 (TBC Tier 2).", member->GetName());
                    continue;
                }

                if (!member->HasItemCount(ITEM_MEDALLION_OF_KARABOR) && !member->HasItemCount(ITEM_BLESSED_MEDALLION_OF_KARABOR))
                {
                    if (member->HasItemCount(ITEM_MEDALLION_OF_KARABOR, 1, true) || member->HasItemCount(ITEM_BLESSED_MEDALLION_OF_KARABOR, 1, true))
                    {
                        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff{}|r has the Medallion of Karabor in their bank.", member->GetName());
                        continue;
                    }

                    member->AddItem(ITEM_MEDALLION_OF_KARABOR, 1);
                    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ffff{}|r received the Medallion of Karabor.", member->GetName());
                }
            }
            return;
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage("You must have the Medallion of Karabor in your inventory to use this command.");
            return;
        }
    }
}

// Grants the attunements of the raids that state has reached, for a bump only. Nothing is taken back when progression goes down.
void IndividualProgression::GrantAttunements(Player* player, ProgressionState state)
{
    if (!enabled || !AutoAttune || !player || !player->IsInWorld() || isBotAccount(player))
        return;

    uint8 stage = state;
    if (progressionLimit && stage > progressionLimit)
        stage = progressionLimit;

    auto giveItem = [player](uint32 itemId, uint32 otherItemId = 0)
    {
        if (player->HasItemCount(itemId, 1, true) || (otherItemId && player->HasItemCount(otherItemId, 1, true)))
            return;

        // An earlier grant may still be waiting in the mailbox
        for (Mail const* mail : player->GetMails())
            for (MailItemInfo const& info : mail->items)
                if (info.item_template == itemId)
                    return;

        ItemPosCountVec dest;
        InventoryResult const result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, 1);
        if (result == EQUIP_ERR_OK)
        {
            player->AddItem(itemId, 1);
            return;
        }

        // Only a full bag is mailed: any other refusal, such as the unique check on an item in a trade window, would make a second copy
        if (result != EQUIP_ERR_INVENTORY_FULL)
            return;

        constexpr uint32 NPC_SORIDORMI = 19935;
        Item* item = Item::CreateItem(itemId, 1, player);
        if (!item)
            return;

        CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
        MailDraft draft("Raid attunement", "Your bags were full when your progression granted you this attunement. Mail that is not collected is deleted when it expires, 30 days after it was sent.");
        item->SaveToDB(transaction);
        draft.AddItem(item);
        draft.SendMailTo(transaction, MailReceiver(player, player->GetGUID().GetCounter()), MailSender(MAIL_CREATURE, NPC_SORIDORMI));
        CharacterDatabase.CommitTransaction(transaction);
    };

    // Marked as rewarded without the quest's own rewards. A quest in the log leaves it the way an abandon does, slot included, or the next slot walk brings its status back.
    auto giveQuest = [player](uint32 questId)
    {
        QuestStatus const status = player->GetQuestStatus(questId);
        if (status == QUEST_STATUS_REWARDED)
            return;

        if (status != QUEST_STATUS_NONE)
        {
            uint16 const slot = player->FindQuestSlot(questId);
            if (slot < MAX_QUEST_LOG_SIZE)
                player->SetQuestSlot(slot, 0);

            player->TakeQuestSourceItem(questId, false);
            player->AbandonQuest(questId);
            player->RemoveActiveQuest(questId);
        }

        player->SetRewardedQuest(questId);
    };

    if (stage >= PROGRESSION_MOLTEN_CORE)
        giveItem(ITEM_DRAKEFIRE_AMULET);

    if (stage >= PROGRESSION_AQ && !isAttuned(player))
        giveQuest(NAXX40_ATTUNEMENT_1);

    if (stage >= PROGRESSION_TBC_TIER_1)
    {
        giveItem(ITEM_TEMPEST_KEY);
        giveQuest(TRIAL_MAGTHERIDON);
        giveQuest(CUDGEL_OF_KARDESH);
    }

    if (stage >= PROGRESSION_TBC_TIER_2)
    {
        giveQuest(VIALS_OF_ETERNITY);
        giveItem(ITEM_MEDALLION_OF_KARABOR, ITEM_BLESSED_MEDALLION_OF_KARABOR);
    }
}

void IndividualProgression::RemovePlayerAchievement(ObjectGuid::LowType playerGUID, uint32 achievementId)
{
	if (!playerGUID || !achievementId)
        return;

    CharacterDatabase.Execute("DELETE FROM `character_achievement` WHERE `guid` = {} AND `achievement` = {}", playerGUID, achievementId);
}

void IndividualProgression::LoadCustomProgressionEntries(std::string const& customProgressionString)
{
    std::string delimitedValue;
    std::stringstream customProgressionStream;

    customProgressionStream.str(customProgressionString);
    while (std::getline(customProgressionStream, delimitedValue, ','))
    {
        std::string pairOne, pairTwo;
        std::stringstream progressionPairStream(delimitedValue);
        progressionPairStream>>pairOne>>pairTwo;
        uint32 creatureEntryId = atoi(pairOne.c_str());
        uint8 progressionValue = atoi(pairTwo.c_str());
        sIndividualProgression->customProgressionMap[creatureEntryId] = progressionValue;
    }
}

bool IndividualProgression::hasCustomProgressionValue(uint32 creatureEntry)
{
    if (!creatureEntry)
        return false;

    if (customProgressionMap.empty())
        return false;

    return (customProgressionMap.find(creatureEntry) != customProgressionMap.end());
}

bool IndividualProgression::isAttuned(Player* player)
{
    if (!player || !player->IsInWorld())
        return false;

    if ((player->GetQuestStatus(NAXX40_ATTUNEMENT_1) == QUEST_STATUS_REWARDED) || (player->GetQuestStatus(NAXX40_ATTUNEMENT_2) == QUEST_STATUS_REWARDED) || (player->GetQuestStatus(NAXX40_ATTUNEMENT_3) == QUEST_STATUS_REWARDED))
        return true;
    else
        return false;
}

void IndividualProgression::LoadAccountRegexes()
{
    auto compile = [](std::string const& option, std::string const& pattern)
        {
            try
            {
                return std::regex(pattern);
            }
            catch (std::regex_error const& e)
            {
                LOG_ERROR("module", "IndividualProgression: {} \"{}\" is not a valid regular expression ({}). No account will match it.", option, pattern, e.what());
                return std::regex();
            }
        };

    std::regex excluded = compile("IndividualProgression.ExcludedAccountsRegex", excludedAccountsRegex);
    std::regex bot = compile("IndividualProgression.BotAccountsRegex", botAccountsRegex);

    std::unique_lock<std::shared_mutex> lock(_accountMatchLock);
    _excludedAccountsRegex = std::move(excluded);
    _botAccountsRegex = std::move(bot);
    _accountMatchCache.clear();
}

IndividualProgression::AccountMatch IndividualProgression::GetAccountMatch(uint32 accountId)
{
    {
        std::shared_lock<std::shared_mutex> lock(_accountMatchLock);
        auto itr = _accountMatchCache.find(accountId);
        if (itr != _accountMatchCache.end())
            return itr->second;
    }

    // AccountMgr::GetName and GetSecurity are blocking login database queries, so they run without the lock held
    std::string accountName;
    if (!AccountMgr::GetName(accountId, accountName))
        return {};

    uint32 const security = AccountMgr::GetSecurity(accountId);

    std::unique_lock<std::shared_mutex> lock(_accountMatchLock);
    AccountMatch match;
    match.excluded = std::regex_match(accountName, _excludedAccountsRegex);
    match.bot = std::regex_match(accountName, _botAccountsRegex);
    match.gm = security >= SEC_GAMEMASTER;
    _accountMatchCache[accountId] = match;
    return match;
}

bool IndividualProgression::isExcludedAccount(Player* player)
{
    if (!player)
        return false;

    return isExcludedAccount(player->GetSession()->GetAccountId());
}

bool IndividualProgression::isExcludedAccount(uint32 accountId)
{
    return GetAccountMatch(accountId).excluded;
}

// By the account and not the session: a bot's session has player security, also on a GM's account.
bool IndividualProgression::isGMAccount(Player* player)
{
    // Headless or disconnecting Playerbots may not have a session.
    // Never dereference a missing session while computing optional tier caps.
    return player && player->GetSession() &&
        GetAccountMatch(player->GetSession()->GetAccountId()).gm;
}

bool IndividualProgression::isBotAccount(const Player* player)
{
    if (!player)
        return false;

    return isBotAccount(player->GetSession()->GetAccountId());
}

bool IndividualProgression::isBotAccount(uint32 accountId)
{
    return GetAccountMatch(accountId).bot;
}

bool IndividualProgression::isNormalAccount(Player* player)
{
    if (!player)
        return false;

    if (sIndividualProgression->isExcludedAccount(player) || sIndividualProgression->isBotAccount(player))
        return false;

    return true;
}

// True for gear from an expansion the character has not reached. The id alone would also lock the
// Death Knight starting gear and heirlooms, which have high ids but a low or no level requirement.
// A character above its cap, such as a GM's, is not locked: it was never held back by progression.
bool IndividualProgression::isLockedExpansionGear(Player* player, ItemTemplate const* proto)
{
    if (!enabled || !LockLaterExpansionGear || !player || !proto || isExcludedAccount(player))
        return false;

    if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
        return false;

    uint8 cap;
    if (isBotAccount(player))
        cap = BotAccountsMaxLevel;
    else if (!hasPassedProgression(player, PROGRESSION_PRE_TBC))
        cap = 60;
    else if (!hasPassedProgression(player, PROGRESSION_TBC_TIER_5))
        cap = 70;
    else
        return false;

    if (player->GetLevel() > cap)
        return false;

    if (cap <= 60)
    {
        // The Blood Elf and Draenei versions of the vanilla rank gear, sold by the vanilla vendors.
        if (proto->ItemId >= 29594 && proto->ItemId <= 29617)
            return false;

        return proto->ItemId >= 23728 && (proto->RequiredLevel >= 57 || proto->ItemLevel > 92);
    }

    if (cap <= 70)
    {
        // TBC gear with WotLK ids: Sunwell jewelcrafting, a Sunwell drop and the last arena and honor rewards.
        switch (proto->ItemId)
        {
            case 35693:
            case 35694:
            case 35700:
            case 35702:
            case 35703:
            case 35733:
            case 36737:
            case 37739:
            case 37740:
            case 37927:
            case 37928:
            case 37929:
                return false;
        }

        return proto->ItemId >= 35570 && (proto->RequiredLevel >= 67 || proto->ItemLevel > 164);
    }

    return false;
}

bool IndividualProgression::isPlayerInDungeonOrRaid(Player* player)
{
    if (!player || !player->IsInWorld())
        return false;

    Map const* map = player->GetMap();
    return (map && (map->IsDungeon() || map->IsRaid()));
}

void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(Player* player, bool verified)
{
    if (!player)
        return;
#if defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) && COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT >= 1
    // Read Player / Session facts ONLY on the established verified login
    // source path, OUTSIDE the identity lock. WHO readers never dereference
    // Player or consult mutable RandomPlayerbotMgr.
    ObjectGuid const fullGuid = player->GetGUID();
    WorldSession const* currentSession = player->GetSession();
    uint32 const accountId = currentSession ? currentSession->GetAccountId() : 0;
    uint8 const race = player->getRace();
    bool const canPublishActor = verified && fullGuid.IsPlayer() &&
        player->IsInWorld() && currentSession && currentSession->IsHeadless() &&
        !player->IsGameMaster() && isBotAccount(player) && accountId != 0 &&
        (race == RACE_BLOODELF || race == RACE_DRAENEI);
#endif
    // The previous login epoch (if any) is expired from the F14 lifecycle
    // registry AFTER the identity lock is released (lock order: registry
    // mutex -> identity lock). Logout/relogin never relies on AttackStop.
    std::uint64_t expiredLoginEpoch = 0;
    [&]()
    {
        std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);
        uint32 const guid = player->GetGUID().GetCounter();
        auto const previousEpoch = _f14VerifiedRandomBotLoginEpoch.find(guid);
        if (previousEpoch != _f14VerifiedRandomBotLoginEpoch.end())
            expiredLoginEpoch = previousEpoch->second;
        // Always evict the previous login incarnation, even if this call is
        // an unverified login. An old GUID cannot resurrect a stale attack proof.
        _f14VerifiedRandomBotGroupCache.erase(guid);
        _f14VerifiedRandomBotLoginEpoch.erase(guid);
#if defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) && COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT >= 1
        // Revoke the old public identity at the same lock boundary as its
        // login epoch, even if the new login fails proof or overflows.
        _f14VerifiedActorPublications.erase(guid);
#endif
        if (!verified)
            return;

        // Keep the historically protected group/login identity independent
        // of the optional F14 visibility epoch. A token exhaustion must NEVER
        // weaken established RandomBot group-progression safeguards.
        _f14VerifiedRandomBotGroupCache[guid] = true;

        // Epoch 0 is always invalid, and wrap would reuse an old token.
        // On exhaustion, group protections survive but no new event/visibility
        // incarnation token is issued. The missing token fails visible.
        if (_f14NextRandomBotLoginEpoch == std::numeric_limits<std::uint64_t>::max())
            return;

        const std::uint64_t loginEpoch = ++_f14NextRandomBotLoginEpoch;
        _f14VerifiedRandomBotLoginEpoch[guid] = loginEpoch;
#if defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) && COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT >= 1
        // Config reload may revoke identity between the login precheck
        // and this lock. Always recheck the atomic while holding the lock.
        if (canPublishActor && _f14ActorPublicationEnabled.load(std::memory_order_acquire))
            _f14VerifiedActorPublications.emplace(
                guid, F14VerifiedActorPublication{fullGuid, accountId, race, loginEpoch});
#endif
    }();
    ExpireF14LifecycleActor(expiredLoginEpoch);
}

#if defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) && COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT >= 1
void IndividualProgression::BeginF14ActorPublicationConfigReload()
{
    // WHO/visibility readers and bot-login writers use the same mutex.
    // A reload invalidates ALL old snapshots (even if re-enabled): the
    // next publication must come from a fresh verified bot login.
    std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);
    _f14ActorPublicationEnabled.store(false, std::memory_order_release);
    _f14VerifiedActorPublications.clear();
}

void IndividualProgression::FinishF14ActorPublicationConfigReload(bool ipEnabled)
{
    // End is reached only after the existing LoadConfig completed.
    // Incomplete reload leaves publication OFF, never stale-enabled.
    _f14ActorPublicationEnabled.store(ipEnabled, std::memory_order_release);
}
#endif

// New WHO/actor identity snapshot read does not depend on mutable bot manager,
// Player*, current world/map pointers, SQL or cached WHO race. A snapshot is
// ONLY evidence at the lock acquisition; callers MUST revalidate on send,
// logout, GUID re-use and era changes. Never use it alone to hide anyone.
bool IndividualProgression::TryReadF14VerifiedActorPublication(
    ObjectGuid const& fullGuid, F14VerifiedActorPublication& out)
{
    out = F14VerifiedActorPublication{};
#if defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) && COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT >= 1
    // The producer remains a diagnostic-only source. On an IP runtime
    // disable, any old snapshot MUST fail visible immediately, even before
    // the normal logout hook retires its epoch. out was reset above.
    // Local atomic snapshot intentionally shadows the historical public
    // non-atomic 'enabled' field: never read that field from WHO workers.
    bool const enabled = _f14ActorPublicationEnabled.load(std::memory_order_acquire);
    if (!enabled || !fullGuid || !fullGuid.IsPlayer())
        return false;

    std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);
    // A reload may begin after the first atomic read but before this
    // shared lock. Retry under the lock, paired with reload's exclusive
    // lock, to avoid returning an old published identity.
    if (!_f14ActorPublicationEnabled.load(std::memory_order_acquire))
        return false;
    uint32 const lowGuid = fullGuid.GetCounter();
    auto const valid = _f14VerifiedRandomBotGroupCache.find(lowGuid);
    auto const epoch = _f14VerifiedRandomBotLoginEpoch.find(lowGuid);
    auto const identity = _f14VerifiedActorPublications.find(lowGuid);
    if (valid == _f14VerifiedRandomBotGroupCache.end() || !valid->second ||
        epoch == _f14VerifiedRandomBotLoginEpoch.end() || !epoch->second ||
        identity == _f14VerifiedActorPublications.end() ||
        identity->second.fullGuid != fullGuid ||
        identity->second.loginEpoch != epoch->second ||
        !identity->second.accountId ||
        (identity->second.race != RACE_BLOODELF &&
         identity->second.race != RACE_DRAENEI))
        return false;

    out = identity->second;
    return true;
#else
    (void)fullGuid;
    return false;
#endif
}

void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(Player* player)
{
    RememberF14VerifiedRandomBotGroupIdentity(player, false);
}

// Read-only, lock-protected identity: never ask mutable Playerbots state
// from a per-viewer Core visibility worker. Absence always means visible.
bool IndividualProgression::HasF14VerifiedRandomBotIdentity(Player const* player)
{
    if (!player)
        return false;
    std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);
    uint32 const lowGuid = player->GetGUID().GetCounter();
    auto const it = _f14VerifiedRandomBotGroupCache.find(lowGuid);
    if (it == _f14VerifiedRandomBotGroupCache.end() || !it->second)
        return false;
    auto const epoch = _f14VerifiedRandomBotLoginEpoch.find(lowGuid);
    return epoch != _f14VerifiedRandomBotLoginEpoch.end() && epoch->second != 0;
}

// WHO queries carry a cached GUID. Never look up a target Player or call
// mutable RandomPlayerbotMgr on a session/WHO worker. Missing means ALLOW.
bool IndividualProgression::HasF14VerifiedRandomBotGuid(ObjectGuid const& guid)
{
    if (!guid)
        return false;
    std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);
    auto const it = _f14VerifiedRandomBotGroupCache.find(guid.GetCounter());
    if (it == _f14VerifiedRandomBotGroupCache.end() || !it->second)
        return false;
    auto const epoch = _f14VerifiedRandomBotLoginEpoch.find(guid.GetCounter());
    return epoch != _f14VerifiedRandomBotLoginEpoch.end() && epoch->second != 0;
}

// Read-only, incarnation-safe actor identity. Never consult mutable Playerbots
// state on visibility/session workers. Zero means no current verified login.
std::uint64_t IndividualProgression::GetF14VerifiedRandomBotLoginEpoch(ObjectGuid const& guid)
{
    if (!guid || !guid.IsPlayer())
        return 0;

    std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);
    uint32 const lowGuid = guid.GetCounter();
    auto const verified = _f14VerifiedRandomBotGroupCache.find(lowGuid);
    if (verified == _f14VerifiedRandomBotGroupCache.end() || !verified->second)
        return 0;

    auto const epoch = _f14VerifiedRandomBotLoginEpoch.find(lowGuid);
    return epoch == _f14VerifiedRandomBotLoginEpoch.end() ? 0 : epoch->second;
}

bool IndividualProgression::BlocksF14VerifiedStageFloorDowngrade(Player* player, uint8 requestedStage)
{
    // Cheap early exits keep the historical behaviour byte-for-byte when the
    // option is OFF. Identity is the login-thread verified epoch only; race,
    // bot account and headless are additional requirements, never proof.
    if (!enabled || !f14ProtectVerifiedRandomBotStageFloor || !player || !player->IsInWorld())
        return false;
    bool const identity = isBotAccount(player) &&
        (player->getRace() == RACE_DRAENEI || player->getRace() == RACE_BLOODELF) &&
        player->GetSession() && player->GetSession()->IsHeadless() &&
        HasF14VerifiedRandomBotIdentity(player);
    // The cap is deliberately NOT an input: refusing is a no-op, never a grant.
    return IndividualProgressionF14StageFloorPolicy::BlocksVerifiedFloorDowngrade(
        true, identity, GetPlayerProgressionFromQuests(player), requestedStage);
}

bool IndividualProgression::ShouldSkipF14VerifiedRandomBotGroupDowngrade(Player* player, uint8 targetStage)
{
    // Preserve all historical behaviour unless this independent pilot is
    // deliberately enabled. Group-policy ON already refuses cross-era joins.
    if (!enabled || !f14ProtectVerifiedRandomBotGroupFloor || enforceGroupRules ||
        !player || !player->IsInWorld() || !isBotAccount(player) ||
        !player->GetSession() || !player->GetSession()->IsHeadless() ||
        !IndividualProgressionGroupPolicy::IsExpansionRace(player->getRace()))
        return false;

    {
        std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);
        auto const it = _f14VerifiedRandomBotGroupCache.find(player->GetGUID().GetCounter());
        if (it == _f14VerifiedRandomBotGroupCache.end() || !it->second)
            return false;
    }
    return IndividualProgressionF14LoginPolicy::SkipVerifiedGroupDowngrade(
        GetPlayerProgressionFromQuests(player), targetStage, player->GetLevel());
}

void IndividualProgression::SyncBotsProgressionToLeader(Group* group)
{
    // Disabling Individual Progression must also disable its group-time quest writes.
    if (!enabled || !group)
        return;

    ObjectGuid leaderGuid = group->GetLeaderGUID();
    if (!leaderGuid)
        return;

    Player* leader = ObjectAccessor::FindPlayer(leaderGuid);
    if (!leader || !isNormalAccount(leader))
        return;

    // With group enforcement ON the leader's era is the effective (paid) one;
    // the established OFF sync keeps the raw stage.
    uint8 refProgress = enforceGroupRules ? GetEffectiveProgression(leader) : GetPlayerProgressionFromQuests(leader);

    // Preserve the historical tier-0 skip with the switch OFF. When grouping
    // enforcement is ON, Vanilla bots must also sync down to a stage-0 leader.
    if (!refProgress && !enforceGroupRules)
        return;

    int32 const limit = progressionLimit;
    if (enforceGroupRules && (limit < 0 || limit > IndividualProgressionGroupPolicy::LastSupportedStage ||
                              refProgress > IndividualProgressionGroupPolicy::LastSupportedStage ||
                              (limit && refProgress > limit)))
        return;
    uint8 const permittedState = enforceGroupRules
        ? IndividualProgressionGroupPolicy::EffectiveStage(refProgress, limit)
        : refProgress;

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || isNormalAccount(member))
            continue;

        // When group enforcement is enabled, never force a bot/excluded account
        // across expansion boundaries or admit a TBC-race member to Vanilla.
        // The global positive-RandomBot starting-floor rule is tracked
        // separately under F14; do not implement it via account-regex guesses.
        uint8 const memberStage = GetPlayerProgressionFromQuests(member);
        if (enforceGroupRules)
        {
            if (!IndividualProgressionGroupPolicy::MayGroup(
                    refProgress, leader->getRace(), memberStage, member->getRace(), limit))
                continue;

            // A no-op sync must not remove and re-add rewarded progression quests.
            if (memberStage == permittedState)
                continue;
        }

        // F14 default-OFF: no Playerbots identity access on group/map thread.
        // When the login-verified TBC-race bot has earned its minimum stage,
        // never downgrade it below the earned TBC floor 8 at any level.
        if (ShouldSkipF14VerifiedRandomBotGroupDowngrade(member, permittedState))
            continue;

        ForceUpdateProgressionState(member, static_cast<ProgressionState>(permittedState));
    }
}

void IndividualProgression::checkIPPhasing(Player* player, uint32 newArea)
{
    if (!player || !player->IsInWorld())
        return;

    player->RemoveAura(IPP_PHASE);
    player->RemoveAura(IPP_PHASE_II);
    player->RemoveAura(IPP_PHASE_III);
    player->RemoveAura(IPP_PHASE_IV);
    player->RemoveAura(IPP_PHASE_V);
    player->RemoveAura(IPP_PHASE_VI);

    switch (newArea) {
        case AREA_DARKSHORE:
        case AREA_GROVE_OF_THE_ANCIENTS:
        case AREA_WILDBEND_RIVER:
        case AREA_TWILIGHT_VALE:
        case AREA_SCARAB_WALL:
        case AREA_SCARAB_DAIS:
            if (hasPassedProgression(player, PROGRESSION_PRE_AQ) && isBeforeProgression(player, PROGRESSION_AQ_WAR))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            break;
        case AREA_SILITHUS:
            if (hasPassedProgression(player, PROGRESSION_PRE_AQ) && isBeforeProgression(player, PROGRESSION_AQ_WAR))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            else if (hasPassedProgression(player, PROGRESSION_AQ_WAR))
            {
                player->CastSpell(player, IPP_PHASE_II, false);
            }
            break;
        case AREA_HIVE_ASHI:
        case AREA_HIVE_ZORA:
        case AREA_HIVE_REGAL:
            if (hasPassedProgression(player, PROGRESSION_PRE_AQ) && isBeforeProgression(player, PROGRESSION_AQ_WAR))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            else if (hasPassedProgression(player, PROGRESSION_AQ_WAR))
            {
                player->CastSpell(player, IPP_PHASE_II, false);
            }
            break;
        case AREA_BOUGH_SHADOW:
        case AREA_SERADANE:
        case AREA_DREAM_BOUGH:
        case AREA_JADEMIR_LAKE:
        case AREA_TWILIGHT_GROVE:
            if (hasPassedProgression(player, PROGRESSION_ONYXIA))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            break;
        case AREA_SILVERMOON_CITY:
            if (isBeforeProgression(player, PROGRESSION_TBC_TIER_4))
            {
                player->CastSpell(player, IPP_PHASE_II, false);
            }
            break;
        case AREA_DUROTAR:
        case AREA_ROCKTUSK_FARM:
        case AREA_TIRISFAL_GLADES:
        case AREA_RUINS_OF_LORDAERON_B:
        case AREA_MULGORE:
        case AREA_VALLEY_OF_HEROES:
        case AREA_ELWYNN_FOREST:
        case AREA_DUN_MOROGH:
        case AREA_TELDRASSIL:
        case AREA_EASTERN_PLAGUELANDS:
        case AREA_PESTILENT_SCAR:
        case AREA_THE_MARRIS_STEAD:
        case AREA_CORINS_CROSSING:
        case AREA_BLASTED_LANDS:
        case AREA_RISE_OF_THE_DEFILER:
        case AREA_DREADMAUL_HOLD:
        case AREA_DREADMAUL_POST:
        case AREA_SERPENTS_COIL:
        case AREA_TANARIS:
        case AREA_GADGETZAN:
        case AREA_ABYSSAL_SANDS:
        case AREA_BROKEN_PILLAR:
        case AREA_WINTERSPRING:
        case AREA_TIMBERMAW_POST:
        case AREA_FROSTSABER_ROCK:
        case AREA_ICE_THISTLE_HILLS:
        case AREA_MAZTHORIL:
        case AREA_AZSHARA:
        case AREA_HALDARR_ENCAMPMENT:
        case AREA_THE_SHATTERED_STRAND:
        case AREA_SOUTHRIDGE_BEACH:
        case AREA_BURNING_STEPPES:
        case AREA_DRACO_DAR:
        case AREA_BLACKROCK_MOUNTAIN:
        case AREA_DREADMAUL_ROCK:
        case AREA_RUINS_OF_THAURISSAN:
            if (hasPassedProgression(player, PROGRESSION_AQ) && isBeforeProgression(player, PROGRESSION_NAXX40))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            break;
        case AREA_ARGENT_TOURNAMENT_GROUNDS:
        case AREA_ARGENT_SUNREAVER_PAVILION:
        case AREA_ARGENT_SILVER_COVENANT_PAVILION:
        case AREA_THE_RING_OF_CHAMPIONS:
        case AREA_THE_ASPIRANTS_RING:
        case AREA_THE_ARGENT_VALIANTS_RING:
        case AREA_THE_ALLIANCE_VALIANTS_RING:
        case AREA_THE_HORDE_VALIANTS_RING:
        case AREA_ARGENT_PAVILION:
            if (hasPassedProgression(player, PROGRESSION_WOTLK_TIER_2))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            break;
        case AREA_SUNWELL_PLATEAU:
        case AREA_SUNWELL_PLATEAU_B:
        case AREA_ISLE_OF_QUEL_DANAS:
        case AREA_MAGISTERS_TERRACE:
        case AREA_SHATTERED_SUN_STAGING:
        case AREA_SILVERMOONS_PRIDE:
        case AREA_SUNS_REACH_SANCTUM:
        case AREA_SUNS_REACH_HARBOR:
        case AREA_SUNS_REACH_ARMORY:
        case AREA_DAWNSTAR_VILLAGE:
        case AREA_THE_DAWNING_SQUARE:
            if (!isBotAccount(player))
                player->RemoveAura(SONG_OF_VICTORY);

            if (isBotAccount(player) || player->GetReputationRank(FACTION_SHATTERED_SUN) >= REP_REVERED)
            {
                player->CastSpell(player, IPP_PHASE_II, false);
                player->CastSpell(player, IPP_PHASE_III, false);
                player->CastSpell(player, IPP_PHASE_IV, false);

                if (!isBotAccount(player))
                {
                    if (player->GetQuestStatus(QUEST_SANCTUM_WARDS) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_DISCOVERING_ROOTS) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_ERRATIC_BEHAVIOR) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_MISSING_MAGISTRIX) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_DISTRACTION_DEAD_SCAR) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_MAKING_READY) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_BATTLE_FOR_ARMORY) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_TAKING_THE_HARBOR) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_INTERCEPT_REINFORCEMENTS) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_ATAMAL_ARMAMENTS) == QUEST_STATUS_REWARDED)
                    {
                        player->AddAura(SONG_OF_VICTORY, player);
                    }
                }
            }
            else if (player->GetReputationRank(FACTION_SHATTERED_SUN) >= REP_HONORED)
            {
                player->CastSpell(player, IPP_PHASE_II, false);
                player->CastSpell(player, IPP_PHASE_III, false);
                player->CastSpell(player, IPP_PHASE_VI, false);
            }
            else if (player->GetReputationRank(FACTION_SHATTERED_SUN) >= REP_FRIENDLY)
            {
                player->CastSpell(player, IPP_PHASE_II, false);
                player->CastSpell(player, IPP_PHASE_V, false);
                player->CastSpell(player, IPP_PHASE_VI, false);
            }
            else // if (player->GetReputationRank(FACTION_SHATTERED_SUN) < REP_FRIENDLY)
            {
                player->CastSpell(player, IPP_PHASE, false);
                player->CastSpell(player, IPP_PHASE_V, false);
                player->CastSpell(player, IPP_PHASE_VI, false);
            }
            break;		
        case AREA_THE_DARK_PORTAL:
            if (hasPassedProgression(player, PROGRESSION_AQ) && isBeforeProgression(player, PROGRESSION_NAXX40))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            else if (hasPassedProgression(player, PROGRESSION_NAXX40) && isBeforeProgression(player, PROGRESSION_PRE_TBC))
            {
                player->CastSpell(player, IPP_PHASE_II, false);
            }
            break;
        case AREA_PURGATION_ISLE:
            if (isBeforeProgression(player, PROGRESSION_AQ))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            else if (hasPassedProgression(player, PROGRESSION_AQ))
            {
                player->CastSpell(player, IPP_PHASE_II, false);
            }
            break;
        case AREA_IRONTREE_WOOD:
            if (player->getClass() == CLASS_HUNTER && ((player->GetQuestStatus(QUEST_THE_ANCIENT_LEAF) == QUEST_STATUS_INCOMPLETE) || (player->GetQuestStatus(QUEST_THE_ANCIENT_LEAF) == QUEST_STATUS_REWARDED)))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            break;
        case AREA_SHATTRATH_CITY:
            if (hasPassedProgression(player, PROGRESSION_TBC_TIER_4))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            if (player->GetQuestStatus(QUEST_MANA_CELLS) == QUEST_STATUS_REWARDED)
            {
                player->CastSpell(player, IPP_PHASE_III, false);
            }
            break;
        case AREA_THRONE_OF_KILJAEDEN:
            if (isBeforeProgression(player, PROGRESSION_TBC_TIER_4))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            else if (hasPassedProgression(player, PROGRESSION_TBC_TIER_4))
            {
                player->CastSpell(player, IPP_PHASE_II, false);
            }
            break;
        case AREA_FOREST_SONG:
            if (hasPassedProgression(player, PROGRESSION_PRE_TBC))
            {
                player->CastSpell(player, IPP_PHASE, false);
            }
            break;
        case AREA_STORMWIND_CITY:
            if (!hasPassedProgression(player, PROGRESSION_PRE_TBC))
            {
                player->CastSpell(player, IPP_PHASE_II, false);
            }
            else if (hasPassedProgression(player, PROGRESSION_TBC_TIER_5))
            {
                player->CastSpell(player, IPP_PHASE_III, false);
            }
            break;
        case AREA_ORGRIMMAR:
            if (hasPassedProgression(player, PROGRESSION_TBC_TIER_5))
            {
                player->CastSpell(player, IPP_PHASE_III, false);
            }
            break;
        case AREA_UNDERCITY:
            if ((player->GetQuestStatus(BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_REWARDED) || (player->GetQuestStatus(BATTLE_UNDERCITY_ALLIANCE) == QUEST_STATUS_REWARDED))
            {
                player->CastSpell(player, IPP_PHASE_III, false);
            }
            else
            {
                player->CastSpell(player, IPP_PHASE_II, false);
            }
            break;
        default:

            uint32 mapid = player->GetMapId();

            if (mapid == MAP_VAULT_OF_ARCHAVON)
            {
                if (hasPassedProgression(player, PROGRESSION_WOTLK_TIER_1) && isBeforeProgression(player, PROGRESSION_WOTLK_TIER_2))
                {
                    player->CastSpell(player, IPP_PHASE, false);
                    break;
                }
                else if (hasPassedProgression(player, PROGRESSION_WOTLK_TIER_2) && isBeforeProgression(player, PROGRESSION_WOTLK_TIER_3))
                {
                    player->CastSpell(player, IPP_PHASE, false);
                    player->CastSpell(player, IPP_PHASE_II, false);
                    break;
                }
                else if (hasPassedProgression(player, PROGRESSION_WOTLK_TIER_3))
                {
                    player->CastSpell(player, IPP_PHASE, false);
                    player->CastSpell(player, IPP_PHASE_II, false);
                    player->CastSpell(player, IPP_PHASE_III, false);
                    break;
                }
            }
            if (mapid == MAP_MAGISTERS_TERRACE || mapid == MAP_THE_SUNWELL)
            {
                if (!isBotAccount(player))
                {
                    player->RemoveAura(SONG_OF_VICTORY);

                    if (player->GetQuestStatus(QUEST_SANCTUM_WARDS) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_DISCOVERING_ROOTS) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_ERRATIC_BEHAVIOR) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_MISSING_MAGISTRIX) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_DISTRACTION_DEAD_SCAR) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_MAKING_READY) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_BATTLE_FOR_ARMORY) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_TAKING_THE_HARBOR) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_INTERCEPT_REINFORCEMENTS) == QUEST_STATUS_REWARDED &&
                        player->GetQuestStatus(QUEST_ATAMAL_ARMAMENTS) == QUEST_STATUS_REWARDED)
                    {
                        player->AddAura(SONG_OF_VICTORY, player);
                    }
                }
            }
            if ((mapid == MAP_SHADOWFANG_KEEP) ||
                (mapid == MAP_RAZORFEN_DOWNS)  ||
                (mapid == MAP_SCARLET_MONASTERY) ||
                (mapid == MAP_SCHOLOMANCE) ||
                (mapid == MAP_STRATHOLME) ||
                (mapid == MAP_DIRE_MAUL))
            {
                if (earlyScourgeBosses || (hasPassedProgression(player, PROGRESSION_AQ) && isBeforeProgression(player, PROGRESSION_NAXX40)))
                {
                    player->CastSpell(player, IPP_PHASE, false);
                    break;
                }
            }
    }
}

void IndividualProgression::checkIPProgression(Player* killer)
{
    if (!enabled || disableDefaultProgression)
        return;

    if (!killer || !killer->IsInWorld())
        return;

    uint8 currentState = GetPlayerProgressionFromQuests(killer);

    static const std::vector<std::pair<uint16, ProgressionState>> achievementMap =
    {
        { HALION_KILL,        PROGRESSION_WOTLK_TIER_5   },
        { LICH_KING_KILL,     PROGRESSION_WOTLK_TIER_4   },
        { ANUB_ARAK_KILL,     PROGRESSION_WOTLK_TIER_3   },
        { KEL_THUZAD_KILL,    PROGRESSION_WOTLK_TIER_1   },
        { KIL_JAEDEN_KILL,    PROGRESSION_TBC_TIER_5     },
//      { ZUL_JIN_KILL,       PROGRESSION_TBC_TIER_4     },
        { ILLIDAN_KILL,       PROGRESSION_TBC_TIER_4     },
        { KAEL_THAS_KILL,     PROGRESSION_TBC_TIER_2     },
        { MALCHEZAAR_KILL,    PROGRESSION_TBC_TIER_1     },
        { KEL_THUZAD_40_KILL, PROGRESSION_NAXX40         },
        { C_THUN_KILL,        PROGRESSION_AQ             },
        { NEFARIAN_KILL,      PROGRESSION_BLACKWING_LAIR },
        { ONYXIA_KILL,        PROGRESSION_ONYXIA         },
        { RAGNAROS_KILL,      PROGRESSION_MOLTEN_CORE    }
    };

    for (auto const& [achievementId, progState] : achievementMap)
    {
        if (killer->HasAchieved(achievementId))
        {
            if (currentState < progState)
                UpdateProgressionState(killer, progState);
            return;
        }
    }
}

bool IndividualProgression::checkCustomKillProgression(Player* killer, Creature* killed)
{
    if (!enabled)
        return false;

    if (!killed || !killer || !killer->IsInWorld())
        return false;

    uint32 entry = killed->GetEntry();

    if (hasCustomProgressionValue(entry))
    {
        UpdateProgressionState(killer, static_cast<ProgressionState>(customProgressionMap[entry]));
        return true;
    }

    return false;
}

void IndividualProgression::checkKillProgression(Player* killer, Creature* killed)
{
    if (!enabled)
        return;

    if (!killed || !killer || !killer->IsInWorld())
        return;

    uint32 entry = killed->GetEntry();

    /*
    if (hasCustomProgressionValue(entry))
    {
        UpdateProgressionState(killer, static_cast<ProgressionState>(customProgressionMap[entry]));
        return;
    }

    if (disableDefaultProgression)
        return;
    */

    static const std::unordered_map<uint32, ProgressionState> bossMap =
    {
        { RAGNAROS,     PROGRESSION_MOLTEN_CORE    },
        { ONYXIA_40,    PROGRESSION_ONYXIA         },
        { NEFARIAN,     PROGRESSION_BLACKWING_LAIR },
        { CTHUN,        PROGRESSION_AQ             },
        { KELTHUZAD_40, PROGRESSION_NAXX40         },
        { MALCHEZAAR,   PROGRESSION_TBC_TIER_1     },
        { KAELTHAS,     PROGRESSION_TBC_TIER_2     },
        { ILLIDAN,      PROGRESSION_TBC_TIER_4     },
//      { ZULJIN,       PROGRESSION_TBC_TIER_4     },
        { KILJAEDEN,    PROGRESSION_TBC_TIER_5     },
        { KELTHUZAD,    PROGRESSION_WOTLK_TIER_1   },
        { YOGGSARON,    PROGRESSION_WOTLK_TIER_2   },
        { ANUBARAK,     PROGRESSION_WOTLK_TIER_3   },
        { LICH_KING,    PROGRESSION_WOTLK_TIER_4   },
        { HALION,       PROGRESSION_WOTLK_TIER_5   }
    };

    auto bossKill = bossMap.find(entry);
    if (bossKill != bossMap.end())
    {
        ProgressionState prog = bossKill->second;
        if (!progressionLimit || (progressionLimit >= prog))
            UpdateProgressionState(killer, prog);

        if (entry == KELTHUZAD_40)
            UpdateProgressionAchievements(killer, KEL_THUZAD_40_KILL);
    }
}

void IndividualProgression::UpdateProgressionAchievements(Player* player, uint16 achievementID)
{
    if (!achievementID || !player || !player->IsInWorld())
        return;

    AchievementEntry const* entry = sAchievementStore.LookupEntry(achievementID);
    if (!entry)
        return;

    player->CompletedAchievement(entry);
}

uint32 IndividualProgression::GetVanillaPvpKillRequirement(uint8 rank) const
{
    uint32 const killRequirements[IPP_PVP_RANK_COUNT] =
    {
        VanillaPvpKillRank1,  VanillaPvpKillRank2,  VanillaPvpKillRank3,  VanillaPvpKillRank4,
        VanillaPvpKillRank5,  VanillaPvpKillRank6,  VanillaPvpKillRank7,  VanillaPvpKillRank8,
        VanillaPvpKillRank9,  VanillaPvpKillRank10, VanillaPvpKillRank11, VanillaPvpKillRank12,
        VanillaPvpKillRank13, VanillaPvpKillRank14
    };

    return rank < IPP_PVP_RANK_COUNT ? killRequirements[rank] : 0;
}

// Highest rank the player's lifetime honorable kills entitle them to, or -1 for no rank at all.
int8 IndividualProgression::GetEarnedVanillaPvpRank(Player* player) const
{
    if (!player || !player->IsInWorld())
        return -1;

    uint32 kills = player->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS);

    for (int8 rank = int8(IPP_PVP_RANK_COUNT) - 1; rank >= 0; --rank)
        if (kills >= GetVanillaPvpKillRequirement(rank))
            return rank;

    return -1;
}

// Highest rank the hidden rank quests say the player has actually been granted, or -1 for none.
int8 IndividualProgression::GetRecordedVanillaPvpRank(Player* player) const
{
    if (!player || !player->IsInWorld())
        return -1;

    for (int8 rank = int8(IPP_PVP_RANK_COUNT) - 1; rank >= 0; --rank)
        if (player->GetQuestStatus(IPP_PVP_QUEST_BASE + rank + 1) == QUEST_STATUS_REWARDED)
            return rank;

    return -1;
}

// Removes the rank titles the player is not entitled to, leaving the one rank they currently hold.
void IndividualProgression::CleanUpVanillaPvpTitles(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    TeamId teamId = player->GetTeamId(true);

    if (teamId != TEAM_ALLIANCE && teamId != TEAM_HORDE)
        return;

    // Ranks are given up entirely once the player leaves Vanilla, unless configured to persist.
    bool dropAllRanks = !VanillaPvpTitlesKeepPostVanilla && hasPassedProgression(player, PROGRESSION_PRE_TBC);

    // What the player was actually granted, capped by what their kills still entitle them to
    // so that raising the configured requirements takes ranks back off them.
    // Characters from before the rank quests existed have no record at all, so fall back to their kills for those.
    int8 recordedRank = GetRecordedVanillaPvpRank(player);
    int8 entitledRank = GetEarnedVanillaPvpRank(player);
    int8 heldRank = (recordedRank < 0 || recordedRank > entitledRank) ? entitledRank : recordedRank;

    int8 earnedRank = dropAllRanks ? -1 : heldRank;

    // Vanilla gave a player the title of their current rank and nothing else,
    // which matters because killing a high ranked player paid more honor and a Grand Marshal had no way to hide behind a Private title.
    // Ranks below the current one are taken away unless the server opts out with VanillaPvpKeepAllEarnedTitles.
    for (int8 rank = 0; rank < int8(IPP_PVP_RANK_COUNT); ++rank)
    {
        if (rank == earnedRank || (VanillaPvpKeepAllEarnedTitles && rank < earnedRank))
            continue;

        CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(TitleData[rank].TitleId[teamId]);

        if (titleEntry && player->HasTitle(titleEntry))
            player->SetTitle(titleEntry, true);
    }

    // Keep the hidden rank quests in step with the ranks the player still holds.
    for (int8 rank = int8(IPP_PVP_RANK_COUNT) - 1; rank > earnedRank; --rank)
    {
        uint32 questId = IPP_PVP_QUEST_BASE + rank + 1;

        if (player->GetQuestStatus(questId) == QUEST_STATUS_REWARDED)
            player->RemoveRewardedQuest(questId);
    }
}

void IndividualProgression::AwardEarnedVanillaPvpTitles(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    // New ranks stop being earned at the Dark Portal unless the config extends it past Vanilla.
    if (!isBeforeProgression(player, PROGRESSION_PRE_TBC) && !VanillaPvpTitlesEarnPostVanilla)
        return;

    TeamId teamId = player->GetTeamId(true);
    if (teamId != TEAM_ALLIANCE && teamId != TEAM_HORDE)
        return;

    int8 earnedRank = GetEarnedVanillaPvpRank(player);
    if (earnedRank < 0)
        return;

    CharTitlesEntry const* highestTitle = sCharTitlesStore.LookupEntry(TitleData[earnedRank].TitleId[teamId]);
    if (!highestTitle)
        return;

    // Record every rank the player has reached in its hidden quest so the `conditions` table can gate ranked gear on it.
    // SetTitle does nothing when the title is already known, so this settles after the first pass instead of churning on every zone change.
    for (int8 rank = 0; rank <= earnedRank; ++rank)
    {
        // Vanilla showed the current rank and nothing else, so that is the only title handed out unless the server keeps all of them.
        // A lower rank whose achievement is missing is still granted for this pass,
        // because ACHIEVEMENT_CRITERIA_TYPE_OWN_RANK is credited off a known title.
        // CleanUpVanillaPvpTitles takes it back straight after, and once the achievement is there the grant stops happening.
        bool grantTitle = VanillaPvpKeepAllEarnedTitles || rank == earnedRank
            || !player->HasAchieved(AchievementData[rank].AchievementId[teamId]);

        if (grantTitle)
        {
            if (CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(TitleData[rank].TitleId[teamId]))
                player->SetTitle(titleEntry);
        }

        uint32 questId = IPP_PVP_QUEST_BASE + rank + 1;

        if (player->GetQuestStatus(questId) != QUEST_STATUS_REWARDED)
        {
            if (Quest const* quest = sObjectMgr->GetQuestTemplate(questId))
            {
                player->AddQuest(quest, nullptr);
                player->CompleteQuest(questId);
                player->RewardQuest(quest, 0, player, false, false);
            }
        }
    }

    // The core credits each rank achievement from ACHIEVEMENT_CRITERIA_TYPE_OWN_RANK, but only fires that when a title actually changes.
    // Ask for it directly so a player whose title is already in place, yet lost the achievement to the old cleanup code, gets it credited back.
    player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_OWN_RANK);

    constexpr uint32 ALLIANCE_PVP_RANK_OFFSET = 4; // rank 1-4 are not used, need to add 4 to align with rank 1 = title ID 5
    constexpr uint32 HORDE_PVP_RANK_OFFSET = 10;   // horde titles start at ID 15, need to subtract 10 to align with rank 1 = title ID 5

    uint32 lifetimeRank = teamId == TEAM_ALLIANCE
        ? highestTitle->ID + ALLIANCE_PVP_RANK_OFFSET
        : highestTitle->ID - HORDE_PVP_RANK_OFFSET;

    player->SetByteValue(PLAYER_FIELD_BYTES, PLAYER_FIELD_BYTES_OFFSET_LIFETIME_MAX_PVP_RANK, uint8(lifetimeRank));

    uint32 chosenTitleId = player->GetUInt32Value(PLAYER_CHOSEN_TITLE);
    bool chosenIsPvpRank = chosenTitleId != 0 && chosenTitleId < 29; // PvP Titles go from 1 to 28.

    // A player showing a rank title is moved to their current rank, since that is the only rank title they keep.
    // With VanillaPvpKeepAllEarnedTitles on, a lower rank they picked themselves has to stick,
    // and they are only moved when the rank they had picked is one they no longer hold.
    // Bots never pick for themselves, so they always show their current rank. A non-PvP title is left alone either way.
    if (isBotAccount(player) || (chosenIsPvpRank && (!VanillaPvpKeepAllEarnedTitles || !player->HasTitle(chosenTitleId))))
        player->SetCurrentTitle(highestTitle);
}

class IndividualPlayerProgression_WorldScript : public WorldScript
{
private:
    static void LoadConfig()
    {
#if defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) && COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT >= 1
        // Revoke BEFORE reloading the non-atomic legacy options. An old
        // actor identity must not reappear if .reload disables then
        // re-enables IP without an intervening bot login.
        sIndividualProgression->BeginF14ActorPublicationConfigReload();
#endif
        sIndividualProgression->customProgressionMap.clear();
        sIndividualProgression->enabled = sConfigMgr->GetOption<bool>("IndividualProgression.Enable", true);
        sIndividualProgression->vanillaPowerAdjustment = sConfigMgr->GetOption<float>("IndividualProgression.VanillaPowerAdjustment", 1);
        sIndividualProgression->vanillaHealingAdjustment = sConfigMgr->GetOption<float>("IndividualProgression.VanillaHealingAdjustment", 1);
        sIndividualProgression->tbcPowerAdjustment = sConfigMgr->GetOption<float>("IndividualProgression.TBCPowerAdjustment", 1);
        sIndividualProgression->tbcHealingAdjustment = sConfigMgr->GetOption<float>("IndividualProgression.TBCHealingAdjustment", 1);
        sIndividualProgression->questXpFix = sConfigMgr->GetOption<bool>("IndividualProgression.QuestXPFix", true);
        sIndividualProgression->requireNaxxStrath = sConfigMgr->GetOption<bool>("IndividualProgression.RequireNaxxStrathEntrance", false);
        sIndividualProgression->doableNaxx40Bosses_4H = sConfigMgr->GetOption<bool>("IndividualProgression.doableNaxx40Bosses_4H", false);
        sIndividualProgression->doableNaxx40Bosses_Gluth = sConfigMgr->GetOption<bool>("IndividualProgression.doableNaxx40Bosses_Gluth", false);
        sIndividualProgression->doableNaxx40Bosses_Patchwerk = sConfigMgr->GetOption<bool>("IndividualProgression.doableNaxx40Bosses_Patchwerk", false);
        sIndividualProgression->doableNaxx40Bosses_Razuvious = sConfigMgr->GetOption<bool>("IndividualProgression.doableNaxx40Bosses_Razuvious", false);
        sIndividualProgression->enforceGroupRules = sConfigMgr->GetOption<bool>("IndividualProgression.EnforceGroupRules", false);
        sIndividualProgression->f14PreserveVerifiedRandomBotStageOnLogin = sConfigMgr->GetOption<bool>("IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin", false);
        sIndividualProgression->f14ProtectVerifiedRandomBotGroupFloor = sConfigMgr->GetOption<bool>(
            "IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor", false);
        sIndividualProgression->f14ForceVerifiedRandomBotTbcFloorOnLogin = sConfigMgr->GetOption<bool>(
            "IndividualProgression.F14.ForceVerifiedRandomBotTbcFloorOnLogin", false);
        sIndividualProgression->f14ProtectVerifiedRandomBotStageFloor = sConfigMgr->GetOption<bool>(
            "IndividualProgression.F14.ProtectVerifiedRandomBotStageFloor", false);
        // F14 observer + WHO pilot remains OFF until the separately reviewed,
        // immediate, per-viewer Core visibility-refresh bridge is present.
        sIndividualProgression->f14ExperimentalRndbotPlayerVisibility = sConfigMgr->GetOption<bool>(
            "IndividualProgression.F14.ExperimentalRndbotPlayerVisibility", false);
#if !defined(COSMIC_F14_OBSERVER_HOOK_VERSION) || !defined(COSMIC_F14_WHO_FILTER_HOOK_VERSION) || !defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION) || COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION < 1
        if (sIndividualProgression->f14ExperimentalRndbotPlayerVisibility)
        {
            LOG_ERROR("module", "F14 player observer pilot requires Core observer, cached WHO and immediate refresh hooks; forcing OFF");
            sIndividualProgression->f14ExperimentalRndbotPlayerVisibility = false;
        }
#endif
#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")
        if (sIndividualProgression->f14PreserveVerifiedRandomBotStageOnLogin)
        {
            LOG_ERROR("module", "IndividualProgression F14 saved-RandomBot login safeguard requested, but required Playerbots API header is missing: feature is NOT active; forcing OFF");
            sIndividualProgression->f14PreserveVerifiedRandomBotStageOnLogin = false;
        }
        if (sIndividualProgression->f14ProtectVerifiedRandomBotGroupFloor)
        {
            LOG_ERROR("module", "IndividualProgression F14 verified RandomBot group floor requires Playerbots API header; forcing OFF");
            sIndividualProgression->f14ProtectVerifiedRandomBotGroupFloor = false;
        }
        if (sIndividualProgression->f14ProtectVerifiedRandomBotStageFloor)
        {
            LOG_ERROR("module", "IndividualProgression F14 verified RandomBot stage floor requires Playerbots API header; forcing OFF");
            sIndividualProgression->f14ProtectVerifiedRandomBotStageFloor = false;
        }
        if (sIndividualProgression->f14ForceVerifiedRandomBotTbcFloorOnLogin)
        {
            LOG_ERROR("module", "IndividualProgression F14 verified racial RandomBot TBC stage floor requires Playerbots API header; forcing OFF");
            sIndividualProgression->f14ForceVerifiedRandomBotTbcFloorOnLogin = false;
        }
        if (sIndividualProgression->f14ExperimentalRndbotPlayerVisibility)
        {
            LOG_ERROR("module", "F14 observer pilot requires positive Playerbots RandomBot identity; forcing OFF");
            sIndividualProgression->f14ExperimentalRndbotPlayerVisibility = false;
        }
#endif
#if !defined(COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION) || COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION < 1
        // The source-only F14 models are NOT sufficient. Never move racial
        // RandomBots to TBC until the Core refresh bridge is compiled in.
        if (sIndividualProgression->f14ForceVerifiedRandomBotTbcFloorOnLogin)
        {
            LOG_ERROR("module", "IndividualProgression F14 racial RandomBot TBC stage floor requires Core visibility refresh integration; forcing OFF");
            sIndividualProgression->f14ForceVerifiedRandomBotTbcFloorOnLogin = false;
        }
#endif
        // F14 attack/corpse lifecycle registry: bookkeeping only (never hides,
        // never refreshes, never writes stages). Every (re)load starts empty.
        sIndividualProgression->f14ExperimentalAttackLifecycleRegistry = sConfigMgr->GetOption<bool>(
            "IndividualProgression.F14.ExperimentalAttackLifecycleRegistry", false);
#if !defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION) || COSMIC_F14_ATTACK_EVENT_HOOK_VERSION < 1
        if (sIndividualProgression->f14ExperimentalAttackLifecycleRegistry)
        {
            LOG_ERROR("module", "F14 attack lifecycle registry requires the Core attack/corpse event hooks; forcing OFF");
            sIndividualProgression->f14ExperimentalAttackLifecycleRegistry = false;
        }
#endif
#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")
        if (sIndividualProgression->f14ExperimentalAttackLifecycleRegistry)
        {
            LOG_ERROR("module", "F14 attack lifecycle registry requires positive Playerbots RandomBot identity; forcing OFF");
            sIndividualProgression->f14ExperimentalAttackLifecycleRegistry = false;
        }
#endif
        // The registry is its own verified-identity source: when ON (and not
        // forced OFF above), OnPlayerLogin records positive headless BE/Draenei
        // RandomBot login epochs without enabling any other F14 feature.
        sIndividualProgression->ResetF14LifecycleRegistry();
        sIndividualProgression->fishingFix = sConfigMgr->GetOption<bool>("IndividualProgression.FishingFix", true);
        sIndividualProgression->VanillaHunterPets = sConfigMgr->GetOption<bool>("IndividualProgression.VanillaHunterPets", true);
        sIndividualProgression->WarlockDemonTrainers = sConfigMgr->GetOption<bool>("IndividualProgression.WarlockDemonTrainers", true);
        sIndividualProgression->simpleConfigOverride = sConfigMgr->GetOption<bool>("IndividualProgression.SimpleConfigOverride", true);
        sIndividualProgression->progressionLimit = sConfigMgr->GetOption<uint8>("IndividualProgression.ProgressionLimit", 0);
        sIndividualProgression->startingProgression = sConfigMgr->GetOption<uint8>("IndividualProgression.StartingProgression", 0);
        sIndividualProgression->questMoneyAtLevelCap = sConfigMgr->GetOption<bool>("IndividualProgression.QuestMoneyAtLevelCap", true);
        sIndividualProgression->repeatableVanillaQuestsXp = sConfigMgr->GetOption<bool>("IndividualProgression.RepeatableVanillaQuestsXP", true);
        sIndividualProgression->disableDefaultProgression = sConfigMgr->GetOption<bool>("IndividualProgression.DisableDefaultProgression", false);
        sIndividualProgression->tbcRacesProgressionLevel = sConfigMgr->GetOption<uint8>("IndividualProgression.TbcRacesUnlockProgression", 0);
        sIndividualProgression->tbcRacesStartingProgression = sConfigMgr->GetOption<uint8>("IndividualProgression.tbcRacesStartingProgression", 0);
        sIndividualProgression->deathKnightProgressionLevel = sConfigMgr->GetOption<uint8>("IndividualProgression.DeathKnightUnlockProgression", 13);
        sIndividualProgression->deathKnightStartingProgression = sConfigMgr->GetOption<uint8>("IndividualProgression.DeathKnightStartingProgression", 13);
        sIndividualProgression->RequiredZulGurubProgression = sConfigMgr->GetOption<uint8>("IndividualProgression.RequiredZulGurubProgression", 3);
        sIndividualProgression->RequiredZulAmanProgression = sConfigMgr->GetOption<uint8>("IndividualProgression.RequiredZulAmanProgression", 12);
        sIndividualProgression->LoadCustomProgressionEntries(sConfigMgr->GetOption<std::string>("IndividualProgression.CustomProgression", ""));
        sIndividualProgression->earlyDungeonSet2 = sConfigMgr->GetOption<bool>("IndividualProgression.AllowEarlyDungeonSet2", false);
        sIndividualProgression->earlyScourgeBosses = sConfigMgr->GetOption<bool>("IndividualProgression.AllowEarlyScourgeBosses", false);
        sIndividualProgression->tbcArenaSeason = sConfigMgr->GetOption<uint8>("IndividualProgression.TBC.ArenaSeason", 1);
        sIndividualProgression->wotlkArenaSeason = sConfigMgr->GetOption<uint8>("IndividualProgression.WotLK.ArenaSeason", 5);
        sIndividualProgression->VanillaPvpKillRank1 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank1", 100);
        sIndividualProgression->VanillaPvpKillRank2 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank2", 200);
        sIndividualProgression->VanillaPvpKillRank3 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank3", 400);
        sIndividualProgression->VanillaPvpKillRank4 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank4", 800);
        sIndividualProgression->VanillaPvpKillRank5 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank5", 1400);
        sIndividualProgression->VanillaPvpKillRank6 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank6", 2000);
        sIndividualProgression->VanillaPvpKillRank7 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank7", 3000);
        sIndividualProgression->VanillaPvpKillRank8 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank8", 4500);
        sIndividualProgression->VanillaPvpKillRank9 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank9", 6000);
        sIndividualProgression->VanillaPvpKillRank10 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank10", 8000);
        sIndividualProgression->VanillaPvpKillRank11 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank11", 10000);
        sIndividualProgression->VanillaPvpKillRank12 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank12", 13000);
        sIndividualProgression->VanillaPvpKillRank13 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank13", 18000);
        sIndividualProgression->VanillaPvpKillRank14 = sConfigMgr->GetOption<uint32>("IndividualProgression.VanillaPvpKillRequirement.Rank14", 24000);
        sIndividualProgression->VanillaPvpTitlesKeepPostVanilla = sConfigMgr->GetOption<bool>("IndividualProgression.VanillaPvpTitlesPersistAfterVanilla", true);
        sIndividualProgression->VanillaPvpTitlesEarnPostVanilla = sConfigMgr->GetOption<bool>("IndividualProgression.VanillaPvpEarnTitlesAfterVanilla", false);
        sIndividualProgression->VanillaPvpKeepAllEarnedTitles = sConfigMgr->GetOption<bool>("IndividualProgression.VanillaPvpKeepAllEarnedTitles", false);
        sIndividualProgression->BotAccountsEarnPvPTitles = sConfigMgr->GetOption<bool>("IndividualProgression.BotAccountsEarnPvPTitles", false);
        sIndividualProgression->DisableRDF = sConfigMgr->GetOption<bool>("IndividualProgression.DisableRDF", false);
        sIndividualProgression->DisableQuestMarkers = sConfigMgr->GetOption<bool>("IndividualProgression.DisableQuestMarkers", true);
        sIndividualProgression->MaxMonsterSight = sConfigMgr->GetOption<bool>("IndividualProgression.MaxMonsterSight", true);
        sIndividualProgression->BotOnlyAdjustments = sConfigMgr->GetOption<bool>("IndividualProgression.BotOnlyAdjustments", false);
        sIndividualProgression->excludedAccountsRegex = sConfigMgr->GetOption<std::string>("IndividualProgression.ExcludedAccountsRegex", "");
        sIndividualProgression->botAccountsRegex = sConfigMgr->GetOption<std::string>("IndividualProgression.BotAccountsRegex", "^RNDBOT.*");
        sIndividualProgression->LoadAccountRegexes();
        sIndividualProgression->EnableSetRepCommand = sConfigMgr->GetOption<bool>("IndividualProgression.EnableSetRepCommand", false);
        sIndividualProgression->EnableAllSpellRanks = sConfigMgr->GetOption<bool>("IndividualProgression.EnableAllSpellRanks", false);
        sIndividualProgression->LimitedSetRepCommand = sConfigMgr->GetOption<bool>("IndividualProgression.LimitedSetRepCommand", true);
        sIndividualProgression->sharedFactionIdsRegex = sConfigMgr->GetOption<std::string>("IndividualProgression.sharedFactionIdsRegex", "59|270|349|509|510|529|576|589|609|729|730|749|889|890|909");
        sIndividualProgression->BotAccountsMaxLevel = sConfigMgr->GetOption<uint8>("IndividualProgression.BotAccountsMaxLevel", 80);
        sIndividualProgression->LockLaterExpansionGear = sConfigMgr->GetOption<bool>("IndividualProgression.LockLaterExpansionGear", false);
        sIndividualProgression->ProfessionTierCap = sConfigMgr->GetOption<bool>("IndividualProgression.ProfessionTierCap", false);
        sIndividualProgression->AllowVanillaRaidAccess = sConfigMgr->GetOption<bool>("IndividualProgression.AllowVanillaRaidAccess", false);
        sIndividualProgression->AutoAttune = sConfigMgr->GetOption<bool>("IndividualProgression.AutoAttune", false);
        sIndividualProgression->paidExpansionEnabled = sConfigMgr->GetOption<bool>("IndividualProgression.PaidExpansion.Enable", false);
        sIndividualProgression->paidExpansionGrandfatherStage13 = sConfigMgr->GetOption<bool>("IndividualProgression.PaidExpansion.GrandfatherStage13", false);
        sIndividualProgression->paidExpansionVendorName = sConfigMgr->GetOption<std::string>("IndividualProgression.PaidExpansion.VendorName", "Chronomancer Vezrath");
#if defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) && COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT >= 1
        // The atomic actor-source gate is published only after LoadConfig.
        // The old identity map stays EMPTY until a newly verified login.
        sIndividualProgression->FinishF14ActorPublicationConfigReload(sIndividualProgression->enabled);
#endif
    }

    static void LoadXpValues()
    {
        if (sIndividualProgression->enabled && sIndividualProgression->questXpFix)
        {
            LOG_INFO("module", "Loading Quest XP cache....");
            uint32 questXpAmount = 0;
            QueryResult result = WorldDatabase.Query("SELECT entry, xpValue FROM custom_quest_xp");
            if (result)
            {
                do
                {
                    uint32 questId = (*result)[0].Get<uint32>();
                    uint32 xpValue = (*result)[1].Get<uint32>();
                    sIndividualProgression->questXpMap.insert({questId, xpValue});
                    questXpAmount++;
                } while (result->NextRow());
            }
            LOG_INFO("module", "Loaded {} quest XP values into cache", questXpAmount);
        }
    }

public:
    IndividualPlayerProgression_WorldScript() : WorldScript("IndividualProgression_WorldScript") { }

    void OnBeforeConfigLoad(bool reload) override
    {
        // The paid expansion switch is read at startup only: online characters'
        // purchase flags are cached at login, so toggling it by .reload could
        // briefly treat a paid character as unpaid.
        bool const paidExpansionAtStartup = sIndividualProgression->paidExpansionEnabled;
        bool const grandfatherAtStartup = sIndividualProgression->paidExpansionGrandfatherStage13;
        LoadConfig();
        if (reload)
        {
            sIndividualProgression->paidExpansionEnabled = paidExpansionAtStartup;
            sIndividualProgression->paidExpansionGrandfatherStage13 = grandfatherAtStartup;
        }
        LoadXpValues();
    }

    void OnAfterConfigLoad(bool reload) override
    {
        // Paid expansion flags live in the Core character settings store.
        // Startup: without it a purchase could not be remembered, so the
        // feature stays OFF and established progression continues.
        // Reload: keep the startup value. Turning the gate off here would make
        // every unpaid stage 13 Wrath-owned; purchases are refused instead.
        bool const requested = sIndividualProgression->paidExpansionEnabled;
        bool const storeSaving = sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED);
        sIndividualProgression->paidExpansionEnabled =
            IndividualProgressionExpansionPolicy::FeatureEnabledAfterConfigLoad(reload, requested, storeSaving);
        if (requested && !storeSaving)
        {
            if (!reload)
                LOG_ERROR("module", "IndividualProgression.PaidExpansion.Enable requires worldserver EnablePlayerSettings = 1: paid expansion feature is NOT active; forcing OFF");
            else
                LOG_ERROR("module", "IndividualProgression: EnablePlayerSettings was turned off by .reload while PaidExpansion is on: the paid gate stays on, purchases are refused, and characters who log in meanwhile read as unpaid (fail closed) until it is restored and the server restarted");
        }

        // Owner-configured starts and unlocks that hand out an era without a
        // purchase. Warn only; historical start behaviour is not changed.
        if (!reload && sIndividualProgression->paidExpansionEnabled)
        {
            auto const* ip = sIndividualProgression;
            // An account gated at stage 13 cannot create a Death Knight while
            // unpaid (effective stage 12). Warn only if the unlock threshold
            // is absent or below Wrath access and the DK would start in Wrath.
            if (ip->deathKnightStartingProgression >= IndividualProgressionExpansionPolicy::WrathAccessStage &&
                (ip->deathKnightProgressionLevel == 0 ||
                 ip->deathKnightProgressionLevel < IndividualProgressionExpansionPolicy::WrathAccessStage))
                LOG_WARN("module", "IndividualProgression.PaidExpansion: DeathKnightStartingProgression = {} with DeathKnightUnlockProgression = {} may give Wrath without a purchase", ip->deathKnightStartingProgression, ip->deathKnightProgressionLevel);
            // Check TBC independently: a DK starting at 8..12 can bypass a
            // lower account creation threshold without owning TBC.
            if (ip->deathKnightStartingProgression >= IndividualProgressionExpansionPolicy::TbcEntryStage &&
                (ip->deathKnightProgressionLevel == 0 ||
                 ip->deathKnightProgressionLevel < IndividualProgressionExpansionPolicy::TbcEntryStage))
                LOG_WARN("module", "IndividualProgression.PaidExpansion: DeathKnightStartingProgression = {} with DeathKnightUnlockProgression = {} may give TBC without a purchase", ip->deathKnightStartingProgression, ip->deathKnightProgressionLevel);
            // With an account unlock threshold of stage 8, creation already
            // requires TBC access. Only a lower/absent threshold bypasses it.
            if (ip->tbcRacesStartingProgression >= IndividualProgressionExpansionPolicy::TbcEntryStage &&
                (ip->tbcRacesProgressionLevel == 0 ||
                 ip->tbcRacesProgressionLevel < IndividualProgressionExpansionPolicy::TbcEntryStage))
                LOG_WARN("module", "IndividualProgression.PaidExpansion: tbcRacesStartingProgression = {} with TbcRacesUnlockProgression = {} may give TBC without a purchase", ip->tbcRacesStartingProgression, ip->tbcRacesProgressionLevel);
            // A TBC-only account can also create a character starting at 13
            // unless the race unlock requires Wrath ownership independently.
            if (ip->tbcRacesStartingProgression >= IndividualProgressionExpansionPolicy::WrathAccessStage &&
                (ip->tbcRacesProgressionLevel == 0 ||
                 ip->tbcRacesProgressionLevel < IndividualProgressionExpansionPolicy::WrathAccessStage))
                LOG_WARN("module", "IndividualProgression.PaidExpansion: tbcRacesStartingProgression = {} with TbcRacesUnlockProgression = {} may give Wrath without a purchase", ip->tbcRacesStartingProgression, ip->tbcRacesProgressionLevel);
            if (ip->startingProgression >= IndividualProgressionExpansionPolicy::TbcEntryStage)
                LOG_WARN("module", "IndividualProgression.PaidExpansion: StartingProgression = {} gives every new character an expansion without a purchase", ip->startingProgression);
            if (ip->tbcRacesProgressionLevel > 0 && ip->tbcRacesProgressionLevel < IndividualProgressionExpansionPolicy::TbcEntryStage)
                LOG_WARN("module", "IndividualProgression.PaidExpansion: TbcRacesUnlockProgression = {} lets an unpaid account unlock Blood Elf/Draenei creation; set it to 8 or higher to require a TBC purchase", ip->tbcRacesProgressionLevel);
        }

        if (sIndividualProgression->simpleConfigOverride)
        {
            sWorld->setIntConfig(CONFIG_WATER_BREATH_TIMER, 60000);
            sWorld->setBoolConfig(CONFIG_LOW_LEVEL_REGEN_BOOST, false);
            sWorld->setBoolConfig(CONFIG_DBC_ENFORCE_ITEM_ATTRIBUTES, false);
        }

        if (sIndividualProgression->MaxMonsterSight)
            sWorld->setFloatConfig(CONFIG_SIGHT_MONSTER, 80.0f);
        
        if (sIndividualProgression->DisableRDF)
            sWorld->setIntConfig(CONFIG_LFG_OPTIONSMASK, 4);

        if (sIndividualProgression->DisableQuestMarkers)
        {
            sWorld->setBoolConfig(CONFIG_OBJECT_QUEST_MARKERS, false);
            sWorld->setBoolConfig(CONFIG_OBJECT_SPARKLES, false);
        }
    }
};

void AddSC_ip_paid_expansion();

// Add all scripts in one
// F14 lifecycle consumer lives in IndividualProgressionF14LifecycleScript.cpp;
// registered here so the upstream Grim loader stays byte-identical.
void AddSC_mod_individual_progression_f14_lifecycle();

void AddSC_mod_individual_progression()
{
    new IndividualPlayerProgression_WorldScript();
    AddSC_ip_paid_expansion();
    AddSC_mod_individual_progression_f14_lifecycle();
}
