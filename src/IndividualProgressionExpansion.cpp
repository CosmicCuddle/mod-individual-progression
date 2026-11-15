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
#include <ctime>
#include "AllCreatureScript.h"
#include "ScriptDefines/ServerScript.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ScriptedGossip.h"

#include <limits>
#include <mutex>

using namespace IndividualProgressionExpansionPolicy;
namespace ReceiptPolicy = IndividualProgressionReceiptPolicy;
namespace PurchaseState = IndividualProgressionPaidPurchaseState;

namespace
{
    // Settings row name and slots. Values: FlagNone / FlagPaid / FlagGrandfathered.
    constexpr char const* PaidExpansionSettingSource = "ip_paid_expansion";
    constexpr uint32 SettingTbc = 0;
    constexpr uint32 SettingWrath = 1;
    constexpr uint32 SettingGrandfatherEvaluated = 2;

    constexpr uint8 PaidExpansionBitTbc = 0x1;
    constexpr uint8 PaidExpansionBitWrath = 0x2;

    // Purchase bits are TBC/Wrath only; the cached receipt mirrors all slots.
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

    // Owner-final rule: OFF, bots/excluded accounts and historic TBC starting
    // privileges are unchanged; only unpaid normal humans are refused.
    bool MustRefuseUnpaidTbcQuest(Player* player)
    {
        return player && sIndividualProgression->IsPaidExpansionActive() &&
            sIndividualProgression->isNormalAccount(player) &&
            !sIndividualProgression->HasExpansionEntitlement(player, Era::Tbc);
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

static ReceiptPolicy::Decoded ReadStrictPaidLedger(uint32 guid)
{
    // Read untouched RAW data: Core GetPlayerSetting may drop malformed tokens.
    QueryResult result = CharacterDatabase.Query(
        "SELECT data FROM character_settings WHERE guid = {} AND source = '{}' LIMIT 1",
        guid, PaidExpansionSettingSource);
    if (!result)
        return ReceiptPolicy::DecodeRaw({}, false);
    Field* fields = result->Fetch();
    return ReceiptPolicy::DecodeRaw(fields[0].IsNull() ? std::string() :
        fields[0].Get<std::string>(), true);
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


bool IndividualProgression::MustHoldPaidTbcQuestReward(Player* player)
{
    // Source-only safeguard while the paid feature is OFF by default.
    // Check the real per-character paid bit, not race alone or an inferred
    // entitlement, so privileged TBC races/DKs remain on their old path.
    return player && IsPaidExpansionActive() && disableDefaultProgression &&
           PaidTbcRewardWouldStrand(GetExpansionFacts(player), disableDefaultProgression);
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
            ReceiptPolicy::Decoded const decoded = ReceiptPolicy::DecodeRaw(data, !fields[1].IsNull());
            bool const verifiedWrath = decoded &&
                decoded.ledger.fields[ReceiptPolicy::WrathFlag] != ReceiptPolicy::FlagNone;
            uint8 const stage = EffectiveAccountStage(raw, verifiedWrath);
            if (stage > best)
                best = stage;
        } while (result->NextRow());
    }
    return best;
}

void IndividualProgression::SetPaidExpansionFlag(Player* player, uint32 index, uint32 value)
{
    // Stage snapshot (#59): the effective stage may change; invalidate first.
    InvalidateStageSnapshot(player->GetGUID());
    player->UpdatePlayerSetting(PaidExpansionSettingSource, index, value);

    uint8 const bit = BitForSetting(index);
    std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
    uint32 const guid = player->GetGUID().GetCounter();
    auto const receipt = _paidExpansionLedgers.find(guid);
    // The evaluation marker (slot 2) is not a purchase bit, but MUST
    // stay current in the ledger that PurchaseExpansion later writes whole.
    if (receipt != _paidExpansionLedgers.end() && index < ReceiptPolicy::Slots)
        receipt->second.fields[index] = value;
    if (!bit)
        return;
    uint8& bits = _paidExpansionFlags[guid];
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
        _paidExpansionLedgers.erase(guid);
        _paidExpansionPurchases.erase(guid);
        return;
    }
    // This hook also runs on far teleport and reconnect. While a purchase is
    // unresolved the DB row may not hold the staged receipt yet: keep the
    // cache. A Pending purchase whose callback lived on another session can
    // no longer be acknowledged: it becomes Unconfirmed (never refunded).
    {
        std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
        auto const purchase = _paidExpansionPurchases.find(guid);
        if (purchase != _paidExpansionPurchases.end())
        {
            if (PurchaseState::CallbackLost(purchase->second, reinterpret_cast<std::uintptr_t>(player->GetSession())))
            {
                purchase->second.state = PurchaseState::State::Unconfirmed;
                LOG_ERROR("module", "IndividualProgression: paid purchase for character {} lost its commit callback (session changed); left unconfirmed, no refund or retry",
                    guid);
            }
            if (!PurchaseState::ReloadMayReplaceCache(purchase->second))
                return;
        }
    }
    bool const commissionGate = paidExpansionCommissionMissingReceipt &&
        StoreWritesAllowed(true, PaidExpansionStoreSaving());
    ReceiptPolicy::Decoded const stored = ReadStrictPaidLedger(guid);
    ReceiptPolicy::Decoded const ledger = PurchaseState::CommissionIfAbsent(stored, commissionGate);
    if (ledger && !stored)
    {
        // Truly absent row only: stage the canonical empty v1 receipt in the
        // Player settings map; it is saved with the character.
        for (uint32 slot = 0; slot < ReceiptPolicy::Slots; ++slot)
            player->UpdatePlayerSetting(PaidExpansionSettingSource, slot, ledger.ledger.fields[slot]);
        LOG_INFO("module", "IndividualProgression: commissioned empty paid receipt for character {} (no row existed)", guid);
    }
    uint8 bits = 0;
    if (ledger && RawStageBeforeMap(player) >= VanillaFinalStage)
    {
        if (ledger.ledger.fields[ReceiptPolicy::TbcFlag] != ReceiptPolicy::FlagNone)
            bits |= PaidExpansionBitTbc;
        if (ledger.ledger.fields[ReceiptPolicy::WrathFlag] != ReceiptPolicy::FlagNone)
            bits |= PaidExpansionBitWrath;
    }
    std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
    if (ledger)
        _paidExpansionLedgers[guid] = ledger.ledger;
    else
    {
        _paidExpansionLedgers.erase(guid);
        LOG_WARN("module", "IndividualProgression: refusing invalid/missing paid ledger for character {} (status {})",
            guid, static_cast<uint32>(ledger.status));
    }
    if (bits)
        _paidExpansionFlags[guid] = bits;
    else
        _paidExpansionFlags.erase(guid);
}


PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)
{
    if (!player || !player->IsInWorld() || player->IsBeingTeleportedFar() || !player->GetSession() ||
        costCopper > static_cast<uint32>(std::numeric_limits<int32>::max()))
        return PurchaseResult::InvalidRequest;

    // Money is always saved; the purchase flag only if the store is saving.
    if (IsPaidExpansionActive() && !StoreWritesAllowed(true, PaidExpansionStoreSaving()))
        return PurchaseResult::FeatureInactive;

    CharacterFacts const facts = GetExpansionFacts(player);
    if (IsPaidExpansionActive())
    {
        std::shared_lock<std::shared_mutex> lock(_paidExpansionLock);
        if (_paidExpansionLedgers.find(player->GetGUID().GetCounter()) == _paidExpansionLedgers.end())
            return PurchaseResult::EntryUnavailable;
        // Duplicate click, or another era, while a purchase is unresolved.
        auto const purchase = _paidExpansionPurchases.find(player->GetGUID().GetCounter());
        if (purchase != _paidExpansionPurchases.end() && !PurchaseState::MayStartPurchase(purchase->second))
            return PurchaseResult::PurchasePending;
    }
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
        // Paid TBC must be advanced by the reward of quest 10259.
        // If the operator disabled default IP progression, that reward
        // cannot award stage 8. Do not sell a permanently stalled TBC
        // entitlement: refuse the offer before any monetary debit.
        // Wrath is unaffected because its purchase opens effective 13
        // without an additional progression-quest hand-in.
        if (disableDefaultProgression)
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
    ReceiptPolicy::Ledger stagedLedger;
    {
        std::shared_lock<std::shared_mutex> lock(_paidExpansionLock);
        auto const it = _paidExpansionLedgers.find(player->GetGUID().GetCounter());
        if (it == _paidExpansionLedgers.end())
            return PurchaseResult::EntryUnavailable;
        stagedLedger = it->second;
    }
    std::time_t const timestamp = std::time(nullptr);
    if (timestamp <= 0 || static_cast<uint64>(timestamp) > std::numeric_limits<uint32>::max() ||
        !ReceiptPolicy::RecordPurchase(stagedLedger,
            era == Era::Tbc ? ReceiptPolicy::Era::Tbc : ReceiptPolicy::Era::Wrath,
            costCopper, static_cast<uint32>(timestamp)))
        return PurchaseResult::EntryUnavailable;

    // 2. Reserve the purchase BEFORE any debit, atomically with the check:
    //    a duplicate click or a second era is refused while unresolved.
    uint32 const guid = player->GetGUID().GetCounter();
    WorldSession* const session = player->GetSession();
    uint64 token = 0;
    {
        std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
        PurchaseState::Record& record = _paidExpansionPurchases[guid];
        if (!PurchaseState::MayStartPurchase(record))
            return PurchaseResult::PurchasePending;
        token = ++_paidExpansionNextToken;
        record = PurchaseState::Submit(eraValue, token, reinterpret_cast<std::uintptr_t>(session));
    }

    // 3. Charge and stage ALL eight validated v1 slots in the Player
    //    settings map. Core's UpdatePlayerSetting creates a missing vector
    //    with index+1 elements, so every slot is written. The entitlement
    //    bit is NOT set here: access opens only on a confirmed commit.
    //    Buying TBC unlocks the Into the Breach (10259) hand-in; it does NOT
    //    grant stage 8. Buying Wrath opens effective stage 13 by its flag,
    //    without granting 14. Never write a progression marker as a side
    //    effect of payment.
    player->ModifyMoney(-static_cast<int32>(costCopper));
    for (uint32 slot = 0; slot < ReceiptPolicy::Slots; ++slot)
        player->UpdatePlayerSetting(PaidExpansionSettingSource, slot, stagedLedger.fields[slot]);
    {
        std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
        _paidExpansionLedgers[guid] = stagedLedger;
    }

    // 4. One transaction: Core's SaveToDB always writes the money and every
    //    settings source together, so gold and receipt commit (or fail)
    //    together, here and in any later ordinary save. Acknowledge only
    //    through the commit callback; never refund, retry or reset on failure.
    CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
    player->SaveToDB(trans, false, false);
    session->AddTransactionCallback(CharacterDatabase.AsyncCommitTransaction(trans)).AfterComplete(
        [session, guid, token](bool committed)
        {
            sIndividualProgression->CompletePaidExpansionPurchase(session, guid, token, committed);
        });
    // Purchase record: the "module" log channel (Server.log by default).
    LOG_INFO("module", "IndividualProgression: character {} submitted {} purchase for {} copper (awaiting commit)",
        guid, EraName(era), costCopper);
    return PurchaseResult::Submitted;
}

void IndividualProgression::CompletePaidExpansionPurchase(WorldSession* session, uint32 guid, uint64 token, bool committed)
{
    PurchaseState::Completion completion = PurchaseState::Completion::Ignored;
    uint8 eraValue = 0;
    {
        std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
        auto const record = _paidExpansionPurchases.find(guid);
        if (record == _paidExpansionPurchases.end())
            return; // logged out: the next login reads the DB
        completion = PurchaseState::Complete(record->second, token, committed);
        eraValue = record->second.era;
        if (completion == PurchaseState::Completion::GrantAccess)
        {
            // Stage snapshot (#59): the effective stage changes; invalidate
            // before the bit becomes observable (snapshot lock is separate
            // and never nested the other way round).
            if (session && session->GetPlayer() && session->GetPlayer()->GetGUID().GetCounter() == guid)
                InvalidateStageSnapshot(session->GetPlayer()->GetGUID());
            _paidExpansionFlags[guid] |= static_cast<Era>(eraValue) == Era::Tbc ? PaidExpansionBitTbc : PaidExpansionBitWrath;
        }
    }
    if (completion == PurchaseState::Completion::Ignored)
        return;

    Era const era = static_cast<Era>(eraValue);
    Player* player = session ? session->GetPlayer() : nullptr;
    bool const sameCharacter = player && player->GetGUID().GetCounter() == guid;
    if (completion == PurchaseState::Completion::GrantAccess)
    {
        LOG_INFO("module", "IndividualProgression: character {} purchase of {} committed", guid, EraName(era));
        if (sameCharacter)
        {
            // Access just changed (Wrath: effective 12 -> 13): the same
            // refresh as any legitimate progression change. Only here, after
            // GrantAccess; never at Submitted/Pending/Unconfirmed.
            if (player->IsInWorld())
            {
                CheckAdjustments(player);
                checkIPPhasing(player, player->GetAreaId());
            }
            ChatHandler(session).PSendSysMessage("|cff00ff00Your purchase of {} is complete.|r", EraName(era));
        }
        return;
    }
    LOG_ERROR("module", "IndividualProgression: character {} purchase of {} could not be confirmed; NOT refunded or retried (reconcile from the database)",
        guid, EraName(era));
    if (sameCharacter)
        ChatHandler(session).PSendSysMessage(
            "|cffff0000Your {} purchase could not be confirmed yet. Do not buy it again: it will be checked when you next log in. Contact server staff if it is still missing.|r",
            EraName(era));
}

IndividualProgressionPaidPurchaseState::View IndividualProgression::GetPaidPurchaseState(Player* player, Era era)
{
    if (!player || !IsPaidExpansionActive())
        return PurchaseState::View::None;
    uint32 const guid = player->GetGUID().GetCounter();
    std::shared_lock<std::shared_mutex> lock(_paidExpansionLock);
    PurchaseState::Record record;
    auto const purchase = _paidExpansionPurchases.find(guid);
    if (purchase != _paidExpansionPurchases.end())
        record = purchase->second;
    uint8 bits = 0;
    auto const flags = _paidExpansionFlags.find(guid);
    if (flags != _paidExpansionFlags.end())
        bits = flags->second;
    bool const bit = bits & (era == Era::Tbc ? PaidExpansionBitTbc : PaidExpansionBitWrath);
    return PurchaseState::ViewFor(record, static_cast<uint8>(era), bit);
}

void IndividualProgression::ForgetPaidExpansionPurchase(Player* player)
{
    if (!player)
        return;
    std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);
    _paidExpansionPurchases.erase(player->GetGUID().GetCounter());
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
    {
        std::shared_lock<std::shared_mutex> lock(_paidExpansionLock);
        if (_paidExpansionLedgers.find(player->GetGUID().GetCounter()) == _paidExpansionLedgers.end())
            return;
    }

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
        // A stage-7 character whose nonrepeatable 10259 was already
        // rewarded cannot buy passage until the legacy quest is reconciled.
        // Do not repeatedly direct them to a vendor that must refuse sale.
        if (era == Era::Tbc && facts.rawStage < TbcEntryStage &&
            player->GetQuestStatus(INTO_THE_BREACH) == QUEST_STATUS_REWARDED)
        {
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffffff00Into the Breach is already completed, but your progression is still stage 7. Contact server staff to review TBC passage. No purchase was processed.|r");
            continue;
        }
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
          PLAYERHOOK_ON_BEFORE_QUEST_COMPLETE, PLAYERHOOK_ON_PLAYER_QUEST_ACCEPT,
          PLAYERHOOK_ON_LOGOUT }) { }

    // Defence in depth: Core's CompleteQuest invokes this veto BEFORE setting
    // quest COMPLETE. Conditions.ScriptName is not evaluated by our pinned
    // Core; and the reward hook fires too late to stop an unpaid quest reward.
    // No change to OFF, bots/excluded accounts or start-stage exemptions.
    // A GM targeting an unpaid normal character does NOT bypass this hook
    // via .quest complete; the audited .ip set override remains available.
    bool OnPlayerBeforeQuestComplete(Player* player, uint32 questId) override
    {
        // A formerly paid raw-stage-7 human must not irreversibly consume
        // nonrepeatable quest 10259 when the default stage writer is OFF.
        // Keep the quest incomplete and the paid receipt untouched.
        if (questId == INTO_THE_BREACH &&
            sIndividualProgression->MustHoldPaidTbcQuestReward(player))
        {
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffffff00Into the Breach cannot be completed while default progression is disabled. Your quest and TBC purchase are preserved.|r");
            return false;
        }

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

    // Core 8daffdabe has no pre-take quest veto. Undo only the attempted
    // 10259 accept on unpaid normal accounts immediately after Core AddQuest.
    // Clearing the log slot is mandatory: RemoveActiveQuest alone leaves a
    // dangling slot, and the later completion check must see no active quest.
    void OnPlayerQuestAccept(Player* player, Quest const* quest) override
    {
        if (!quest || quest->GetQuestId() != INTO_THE_BREACH ||
            !MustRefuseUnpaidTbcQuest(player))
            return;

        uint16 const slot = player->FindQuestSlot(INTO_THE_BREACH);
        if (slot < MAX_QUEST_LOG_SIZE)
            player->SetQuestSlot(slot, 0);

        player->RemoveActiveQuest(INTO_THE_BREACH, true);
        player->RemoveTimedAchievement(ACHIEVEMENT_TIMED_TYPE_QUEST, INTO_THE_BREACH);
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffffff00Purchase The Burning Crusade from Chronomancer Vezrath before taking Into the Breach.|r");
        LOG_INFO("module", "IndividualProgression: refused unpaid character {} quest 10259 acceptance",
            player->GetGUID().GetCounter());
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

        // Core has no pre-RewardQuest veto: a saved COMPLETE from an offline GM
        // command or before this feature was enabled could otherwise be rewarded
        // while the unpaid stage-8 grant is denied, permanently stranding 10259.
        // Defer ONLY that saved status; IncompleteQuest preserves kill progress,
        // quest ownership, stage markers, and already-REWARDED quest history.
        if (player && sIndividualProgression->IsPaidExpansionActive() &&
            sIndividualProgression->isNormalAccount(player) &&
            !sIndividualProgression->HasExpansionEntitlement(player, Era::Tbc) &&
            player->GetQuestStatus(INTO_THE_BREACH) == QUEST_STATUS_COMPLETE)
        {
            player->IncompleteQuest(INTO_THE_BREACH);
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffffff00Into the Breach has been kept in your quest log without losing objective progress. Purchase TBC passage before claiming its reward.|r");
            LOG_INFO("module", "IndividualProgression: deferred unpaid character {} quest 10259 completion at login",
                player->GetGUID().GetCounter());
        }

        sIndividualProgression->SendPaidExpansionReminder(player);
    }

    // Purchase bits are NOT erased at logout: that left a window where a
    // paid, still in-world character read as unpaid; they are replaced on
    // every load. Only the unresolved-purchase record is forgotten here: its
    // callback dies with the session and the next login reads the DB.
    void OnPlayerLogout(Player* player) override
    {
        sIndividualProgression->ForgetPaidExpansionPurchase(player);
    }
};

// Hide the ordinary quest-giver menu for a normal unpaid human without
// changing the NPC's ScriptName or any world SQL. Core's direct query packet
// still exposes details; the acceptance undo above is the authoritative gate.
// The '!' icon is NOT hidden by this AllCreatureScript hook.
class IndividualProgression_PaidExpansionQuestGossip : public AllCreatureScript
{
public:
    IndividualProgression_PaidExpansionQuestGossip() :
        AllCreatureScript("IndividualProgression_PaidExpansionQuestGossip") { }

    bool CanCreatureGossipHello(Player* player, Creature* creature) override
    {
        if (!creature || creature->GetEntry() != 19942 ||
            !MustRefuseUnpaidTbcQuest(player) ||
            player->GetQuestStatus(INTO_THE_BREACH) != QUEST_STATUS_NONE)
            return false;

        ClearGossipMenuFor(player);
        if (player->GetSession())
            ChatHandler(player->GetSession()).PSendSysMessage(
                "|cffffff00Purchase The Burning Crusade from Chronomancer Vezrath before taking Into the Breach.|r");
        CloseGossipMenuFor(player);
        return true;
    }
};

// Defence-in-depth for a quest already COMPLETE before default progression
// was disabled. Pinned Core's choose-reward handler calls RewardQuest directly;
// it does NOT invoke OnPlayerBeforeQuestComplete again. Only block the normal
// client packet when the same narrowly verified paid human would be stranded.
// Other direct Core reward call sites still need an authoritative pre-reward
// veto before the paid feature can be enabled on a real realm.
class IndividualProgression_PaidTbcQuestRewardPacketGuard final : public ServerScript
{
public:
    IndividualProgression_PaidTbcQuestRewardPacketGuard()
        : ServerScript("IndividualProgression_PaidTbcQuestRewardPacketGuard",
            {SERVERHOOK_CAN_PACKET_RECEIVE}) { }

    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        if (packet.GetOpcode() != CMSG_QUESTGIVER_CHOOSE_REWARD || !session ||
            !session->GetPlayer() || packet.size() < 16)
            return true;

        Player* player = session->GetPlayer();
        if (!sIndividualProgression->MustHoldPaidTbcQuestReward(player))
            return true;

        // Core QuestHandler.cpp consumes guid (8 bytes), quest id and reward
        // index in precisely this order. Never mutate the caller's read cursor.
        WorldPacket copy(packet);
        copy.rpos(0);
        ObjectGuid questGiver;
        uint32 questId = 0;
        uint32 reward = 0;
        copy >> questGiver >> questId >> reward;
        if (questId != INTO_THE_BREACH)
            return true;

        // The quest must remain COMPLETE/unrewarded; do not alter its
        // objectives, bought entitlement, money or progression markers.
        // Core must never run RewardQuest for this client packet.
        // A packet veto is otherwise silent; explain why the hand-in was
        // refused so the player doesn't mistake it for a lost purchase.
        ChatHandler(session).PSendSysMessage(
            "|cffffff00Into the Breach cannot be rewarded while default progression is disabled. Your quest and TBC purchase are preserved.|r");
        return false;
    }
};

void AddSC_ip_paid_expansion()
{
    new IndividualProgression_PaidExpansionPlayerScript();
    new IndividualProgression_PaidExpansionQuestGossip();
    new IndividualProgression_PaidTbcQuestRewardPacketGuard();
}
