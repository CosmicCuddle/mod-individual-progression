/*
 * CosmicCuddle Individual Progression — paid expansion entitlement.
 *
 * Default OFF (IndividualProgression.PaidExpansion.Enable = 0). Rules live in
 * IndividualProgressionExpansionPolicy.h; this file gathers per-character
 * facts, stores purchases and exposes the API a vendor NPC (Character
 * Services / Chronomancer Vezrath) must use.
 *
 * Storage: Core character settings (table character_settings, saved in the
 * same SaveToDB transaction as money and quest status). Requires worldserver
 * EnablePlayerSettings = 1; otherwise the feature forces itself OFF.
 */

#include "IndividualProgression.h"
#include "World.h"

#include <limits>
#include <mutex>

using namespace IndividualProgressionExpansionPolicy;

namespace
{
    // Settings row name and slots. Values: FlagNone / FlagPaid / FlagGrandfathered.
    constexpr char const* PaidExpansionSettingSource = "ip_paid_expansion";
    constexpr uint32 SettingTbc = 0;
    constexpr uint32 SettingWrath = 1;
    constexpr uint32 SettingGrandfatherEvaluated = 2;

    constexpr uint8 PaidExpansionBitTbc = 0x1;
    constexpr uint8 PaidExpansionBitWrath = 0x2;

    // Only purchase slots are mirrored in the cache.
    uint8 BitForSetting(uint32 index)
    {
        switch (index)
        {
            case SettingTbc: return PaidExpansionBitTbc;
            case SettingWrath: return PaidExpansionBitWrath;
            default: return 0;
        }
    }

    // Rewarded stage read without IndividualProgression's IsInWorld guard,
    // for use before the character is added to a map. Same rule as
    // GetPlayerProgressionFromQuests otherwise.
    uint8 RawStageBeforeMap(Player* player)
    {
        uint8 stage = 0;
        for (uint8 i = PROGRESSION_MOLTEN_CORE; i <= PROGRESSION_WOTLK_TIER_5; ++i)
            if (player->GetQuestStatus(66000 + i) == QUEST_STATUS_REWARDED)
                stage = i;
        return stage;
    }

    char const* EraName(Era era)
    {
        return era == Era::Tbc ? "The Burning Crusade" : "Wrath of the Lich King";
    }
}

// Startup snapshot: paidExpansionEnabled is forced OFF at startup when the
// settings store is unavailable and cannot be turned on by .reload. Reading
// the live store setting here would hand out free Wrath if it were switched
// off mid-session; purchases check the live setting separately instead.
bool IndividualProgression::IsPaidExpansionActive() const
{
    return enabled && paidExpansionEnabled;
}

static bool PaidExpansionStoreSaving()
{
    return sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED);
}

uint8 IndividualProgression::GetConfiguredStartStage(Player* player) const
{
    // Mirrors the normal-account start rules in IndividualProgressionPlayer.cpp
    // OnPlayerLogin, so DK / TBC-race / server start privileges stay intact.
    int start = startingProgression;
    if ((player->getRace() == RACE_DRAENEI || player->getRace() == RACE_BLOODELF) && tbcRacesStartingProgression > start)
        start = tbcRacesStartingProgression;
    if (player->getClass() == CLASS_DEATH_KNIGHT && deathKnightStartingProgression > start)
        start = deathKnightStartingProgression;
    if (start < 0)
        return 0;
    return start > LastSupportedStage ? LastSupportedStage : static_cast<uint8>(start);
}

CharacterFacts IndividualProgression::GetExpansionFacts(Player* player)
{
    CharacterFacts facts;
    facts.featureActive = IsPaidExpansionActive();
    if (!player)
        return facts;

    facts.rawStage = GetPlayerProgressionFromQuests(player);
    facts.progressionLimit = progressionLimit;
    if (!facts.featureActive)
        return facts;

    facts.gated = isNormalAccount(player);
    facts.startStage = GetConfiguredStartStage(player);

    uint8 bits = 0;
    {
        std::shared_lock<std::shared_mutex> lock(_paidExpansionLock);
        auto const itr = _paidExpansionFlags.find(player->GetGUID().GetCounter());
        if (itr != _paidExpansionFlags.end())
            bits = itr->second;
    }
    facts.tbcPaid = bits & PaidExpansionBitTbc;
    facts.wrathPaid = bits & PaidExpansionBitWrath;
    return facts;
}

uint8 IndividualProgression::GetEffectiveProgression(Player* player)
{
    uint8 const raw = GetPlayerProgressionFromQuests(player);
    // Fast path: OFF, or below the only stage the feature can cap.
    if (raw < WrathAccessStage || !IsPaidExpansionActive())
        return raw;
    return EffectiveStage(GetExpansionFacts(player));
}

bool IndividualProgression::IsAutomaticGrantAllowed(Player* player, uint8 targetStage)
{
    if (targetStage < TbcEntryStage || !IsPaidExpansionActive())
        return true;
    return AutomaticGrantAllowed(GetExpansionFacts(player), targetStage);
}

bool IndividualProgression::HasExpansionEntitlement(Player* player, Era era)
{
    return Owns(GetExpansionFacts(player), era);
}

uint8 IndividualProgression::GetMaxServiceGrantableStage(Player* player)
{
    return MaxServiceGrantableStage(GetExpansionFacts(player));
}

uint8 IndividualProgression::GetAccountEffectiveProgression(uint32 accountId)
{
    if (!sIndividualProgression->IsPaidExpansionActive())
        return GetAccountProgression(accountId);

    // Same characters and markers as GetAccountProgression, plus each
    // character's stored purchase slots.
    QueryResult result = CharacterDatabase.Query(
        "SELECT MAX(q.quest), s.data FROM characters c "
        "JOIN character_queststatus_rewarded q ON q.guid = c.guid AND q.quest BETWEEN {} AND {} "
        "LEFT JOIN character_settings s ON s.guid = c.guid AND s.source = '{}' "
        "WHERE c.account = {} GROUP BY c.guid, s.data;",
        66000 + PROGRESSION_MOLTEN_CORE, 66000 + PROGRESSION_WOTLK_TIER_5, PaidExpansionSettingSource, accountId);

    uint8 best = 0;
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint8 const raw = static_cast<uint8>(fields[0].Get<uint32>() - 66000);
            std::string const data = fields[1].IsNull() ? std::string() : fields[1].Get<std::string>();
            uint8 const stage = EffectiveAccountStage(raw, SettingsSlotSet(data, SettingWrath));
            if (stage > best)
                best = stage;
        } while (result->NextRow());
    }
    return best;
}

void IndividualProgression::SetPaidExpansionFlag(Player* player, uint32 index, uint32 value)
{
    player->UpdatePlayerSetting(PaidExpansionSettingSource, index, value);

    uint8 const bit = BitForSetting(index);
    if (!bit)
        return;
    std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
    uint8& bits = _paidExpansionFlags[player->GetGUID().GetCounter()];
    if (value != FlagNone)
        bits |= bit;
    else
        bits &= ~bit;
}

void IndividualProgression::LoadPaidExpansionEntitlement(Player* player)
{
    if (!player)
        return;

    uint32 const guid = player->GetGUID().GetCounter();
    if (!IsPaidExpansionActive())
    {
        std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
        _paidExpansionFlags.erase(guid);
        return;
    }

    // Only characters at or past the final Vanilla milestone can hold a
    // purchase; skip the others so no purchase slots are read for them.
    uint8 bits = 0;
    if (RawStageBeforeMap(player) >= VanillaFinalStage)
    {
        if (player->GetPlayerSetting(PaidExpansionSettingSource, SettingTbc).value != FlagNone)
            bits |= PaidExpansionBitTbc;
        if (player->GetPlayerSetting(PaidExpansionSettingSource, SettingWrath).value != FlagNone)
            bits |= PaidExpansionBitWrath;
    }

    std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
    if (bits)
        _paidExpansionFlags[guid] = bits;
    else
        _paidExpansionFlags.erase(guid);
}


PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)
{
    if (!player || !player->IsInWorld() || costCopper > static_cast<uint32>(std::numeric_limits<int32>::max()))
        return PurchaseResult::InvalidRequest;

    // Money is always saved; the purchase flag only if the store is saving.
    if (IsPaidExpansionActive() && !StoreWritesAllowed(true, PaidExpansionStoreSaving()))
        return PurchaseResult::FeatureInactive;

    CharacterFacts const facts = GetExpansionFacts(player);
    bool entryAvailable = true;
    if (IsKnownEra(eraValue) && static_cast<Era>(eraValue) == Era::Tbc)
    {
        // Never take money if either the stage-8 marker or the actual
        // Into the Breach (10259) entry quest is missing from this world.
        // Visibility/share/reward gating is implemented separately.
        entryAvailable = sObjectMgr->GetQuestTemplate(66000 + TbcEntryStage) != nullptr &&
                         sObjectMgr->GetQuestTemplate(INTO_THE_BREACH) != nullptr;
        // A pre-rewarded, nonrepeatable 10259 at raw stage 7 would strand
        // the buyer: it cannot be handed in again. Until an owner-reviewed
        // recovery rule exists, refuse the SALE before any money is moved.
        // This is a release safeguard, not a completed migration solution.
        if (player->GetQuestStatus(INTO_THE_BREACH) == QUEST_STATUS_REWARDED)
            entryAvailable = false;
    }

    return CheckPurchase(facts, eraValue, player->HasEnoughMoney(costCopper), entryAvailable);
}

PurchaseResult IndividualProgression::PurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)
{
    // 1. Validate everything before money moves.
    PurchaseResult const check = CanPurchaseExpansion(player, eraValue, costCopper);
    if (check != PurchaseResult::Success)
        return check;

    Era const era = static_cast<Era>(eraValue);
    uint32 const index = era == Era::Tbc ? SettingTbc : SettingWrath;
    uint8 const rawBeforePurchase = GetPlayerProgressionFromQuests(player);

    // 2. Charge, then record the purchase.
    player->ModifyMoney(-static_cast<int32>(costCopper));
    SetPaidExpansionFlag(player, index, FlagPaid);

    // 3. Buying TBC unlocks the Into the Breach (10259) hand-in; it does
    //    NOT grant stage 8. Buying Wrath opens effective stage 13 by setting
    //    its entitlement flag, without granting 14. Quest availability and
    //    already-rewarded recovery need separate integration before release.
    //    Never write a progression marker as a side effect of payment.

    // 4. Verify payment semantics, NOT the TBC quest hand-in. A paid
    //    stage-7 character must remain at stage 7 immediately after buying.
    //    On any failure undo the flag and refund.
    bool const opened = era == Era::Tbc
        ? GetPlayerProgressionFromQuests(player) == rawBeforePurchase &&
          GetExpansionFacts(player).tbcPaid
        : GetEffectiveProgression(player) >= WrathAccessStage;
    if (!opened)
    {
        SetPaidExpansionFlag(player, index, FlagNone);
        player->ModifyMoney(static_cast<int32>(costCopper));
        LOG_ERROR("module", "IndividualProgression: {} purchase for character {} could not be applied; refunded {} copper",
            EraName(era), player->GetGUID().GetCounter(), costCopper);
        return PurchaseResult::EntryUnavailable;
    }

    // 5. Money, quest markers and the flag are saved in one transaction.
    player->SaveToDB(false, false);
    // Purchase record: the "module" log channel (Server.log by default).
    LOG_INFO("module", "IndividualProgression: character {} purchased {} for {} copper",
        player->GetGUID().GetCounter(), EraName(era), costCopper);
    return PurchaseResult::Success;
}

// "First login" means the character's first login with the feature ON. Every
// gated character is marked evaluated then, whatever the option, so turning
// GrandfatherStage13 on later never grants Wrath retroactively.
void IndividualProgression::EvaluatePaidExpansionGrandfather(Player* player)
{
    // With the store not saving, the "evaluated" marker of an already
    // evaluated character would read as unset and could grant Wrath.
    if (!player || !StoreWritesAllowed(IsPaidExpansionActive(), PaidExpansionStoreSaving()))
        return;

    CharacterFacts const facts = GetExpansionFacts(player);
    if (!facts.gated)
        return;

    bool const evaluated = player->GetPlayerSetting(PaidExpansionSettingSource, SettingGrandfatherEvaluated).value != FlagNone;
    if (ShouldGrandfatherWrath(facts, paidExpansionGrandfatherStage13, evaluated))
    {
        SetPaidExpansionFlag(player, SettingWrath, FlagGrandfathered);
        LOG_INFO("module", "IndividualProgression: character {} grandfathered Wrath entitlement at stage {}",
            player->GetGUID().GetCounter(), facts.rawStage);
    }
    if (!evaluated)
        SetPaidExpansionFlag(player, SettingGrandfatherEvaluated, 1);
}

void IndividualProgression::SendPaidExpansionReminder(Player* player)
{
    if (!player || !player->GetSession() || !IsPaidExpansionActive())
        return;

    CharacterFacts const facts = GetExpansionFacts(player);
    for (Era era : {Era::Tbc, Era::Wrath})
    {
        if (!ShouldRemind(facts, era))
            continue;
        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cffffff00You have completed the requirements to unlock {}! Visit {} to purchase access.|r",
            EraName(era), paidExpansionVendorName);
    }
}

class IndividualProgression_PaidExpansionPlayerScript : public PlayerScript
{
public:
    IndividualProgression_PaidExpansionPlayerScript() : PlayerScript("IndividualProgression_PaidExpansion",
        { PLAYERHOOK_ON_SEND_INITIAL_PACKETS_BEFORE_ADD_TO_MAP, PLAYERHOOK_ON_LOGIN,
          PLAYERHOOK_ON_BEFORE_QUEST_COMPLETE }) { }

    // Defence in depth: Core's CompleteQuest invokes this veto BEFORE setting
    // quest COMPLETE. Conditions.ScriptName is not evaluated by our pinned
    // Core; and the reward hook fires too late to stop an unpaid quest reward.
    // Does not touch other quests, OFF mode, GM/start-stage exemptions or bots.
    bool OnPlayerBeforeQuestComplete(Player* player, uint32 questId) override
    {
        if (questId != INTO_THE_BREACH || !player ||
            !sIndividualProgression->IsPaidExpansionActive() ||
            !sIndividualProgression->isNormalAccount(player) ||
            sIndividualProgression->HasExpansionEntitlement(player, Era::Tbc))
            return true;

        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffffff00Purchase The Burning Crusade from Chronomancer Vezrath before completing Into the Breach.|r");
        return false;
    }

    // Login thread, after the character (and its settings) loaded from the
    // database and before it enters a map; also on reconnect and worldport.
    void OnPlayerSendInitialPacketsBeforeAddToMap(Player* player, WorldPacket& /*data*/) override
    {
        sIndividualProgression->LoadPaidExpansionEntitlement(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        sIndividualProgression->EvaluatePaidExpansionGrandfather(player);
        sIndividualProgression->SendPaidExpansionReminder(player);
    }

    // No logout hook: erasing on OnPlayerLogout left a window where a paid,
    // still in-world character read as unpaid. The entry (purchase bits only)
    // is replaced on every load, so a stale one is never used.
};

void AddSC_ip_paid_expansion()
{
    new IndividualProgression_PaidExpansionPlayerScript();
}
