/*
 * CosmicCuddle Individual Progression: cache-only stage snapshot (API v1,
 * Coordination #59). See IndividualProgressionStageSnapshot.h for the
 * contract. Publication happens ONLY in PlayerScript::OnPlayerUpdate, on the
 * map thread that owns the player; every other event only invalidates.
 */

#include "IndividualProgression.h"
#include "AllCommandScript.h"
#include "WorldSessionMgr.h"

#include <mutex>
#include <shared_mutex>
#include <string_view>

namespace SnapshotApi = IndividualProgressionStageSnapshot;

bool IndividualProgression::TryGetAccountMatch(uint32 accountId, AccountMatch& match)
{
    std::shared_lock<std::shared_mutex> lock(_accountMatchLock);
    auto const itr = _accountMatchCache.find(accountId);
    if (itr == _accountMatchCache.end())
        return false;
    match = itr->second;
    return true;
}

IndividualProgressionStageSnapshot::Snapshot IndividualProgression::GetStageSnapshot(ObjectGuid const& guid) const
{
    std::shared_lock<std::shared_mutex> lock(_stageSnapshotLock);
    return _stageSnapshots.Get(guid.GetRawValue());
}

void IndividualProgression::RegisterStageSnapshot(Player* player)
{
    if (!player)
        return;
    std::unique_lock<std::shared_mutex> lock(_stageSnapshotLock);
    _stageSnapshots.Register(player->GetGUID().GetRawValue());
}

void IndividualProgression::EraseStageSnapshot(ObjectGuid const& guid)
{
    std::unique_lock<std::shared_mutex> lock(_stageSnapshotLock);
    _stageSnapshots.Erase(guid.GetRawValue());
}

void IndividualProgression::InvalidateStageSnapshot(ObjectGuid const& guid)
{
    std::unique_lock<std::shared_mutex> lock(_stageSnapshotLock);
    _stageSnapshots.Invalidate(guid.GetRawValue());
}

void IndividualProgression::InvalidateAllStageSnapshots()
{
    std::unique_lock<std::shared_mutex> lock(_stageSnapshotLock);
    _stageSnapshots.InvalidateAll();
}

void IndividualProgression::PublishStageSnapshotIfDirty(Player* player)
{
    if (!player || !player->IsInWorld())
        return;
    uint64 const key = player->GetGUID().GetRawValue();
    SnapshotApi::Ticket ticket;
    {
        std::shared_lock<std::shared_mutex> lock(_stageSnapshotLock);
        ticket = _stageSnapshots.BeginPublish(key, _stageSnapshots.EpochOf(key));
    }
    if (!ticket.valid)
        return; // clean, or not registered (fast path every update)

    // Computed on the owning thread, with no snapshot lock held and no DB:
    // the account class comes from IP's cache only.
    SnapshotApi::PublishInput input;
    input.ipEnabled = enabled;
    AccountMatch match;
    WorldSession* session = player->GetSession();
    input.accountResolved = session && TryGetAccountMatch(session->GetAccountId(), match);
    input.gatedHuman = input.accountResolved && !match.excluded && !match.bot;
    input.rawStage = GetPlayerProgressionFromQuests(player);
    input.effectiveStage = input.rawStage;
    if (input.accountResolved && IsPaidExpansionActive())
    {
        // Same facts as GetExpansionFacts, without its account lookup.
        IndividualProgressionExpansionPolicy::CharacterFacts facts;
        facts.featureActive = true;
        facts.gated = input.gatedHuman;
        facts.rawStage = input.rawStage;
        facts.progressionLimit = progressionLimit;
        facts.startStage = GetConfiguredStartStage(player);
        uint8 bits = 0;
        {
            std::shared_lock<std::shared_mutex> lock(_paidExpansionLock);
            auto const itr = _paidExpansionFlags.find(player->GetGUID().GetCounter());
            if (itr != _paidExpansionFlags.end())
                bits = itr->second;
        }
        facts.tbcPaid = bits & 0x1;
        facts.wrathPaid = bits & 0x2;
        input.effectiveStage = IndividualProgressionExpansionPolicy::EffectiveStage(facts);
    }

    std::unique_lock<std::shared_mutex> lock(_stageSnapshotLock);
    _stageSnapshots.Publish(key, ticket, input);
}

void IndividualProgression::WarmAccountCacheForOnlineSessions()
{
    // World thread after a config reload: the account cache was cleared.
    // GetAccountMatch may query the login DB here (as at login); the snapshot
    // lock is NOT held. Owners then republish on their next update.
    for (auto const& [accountId, session] : sWorldSessionMgr->GetAllSessions())
        if (session && session->GetPlayer())
            GetAccountMatch(accountId);
}

class IndividualProgression_StageSnapshotPlayerScript : public PlayerScript
{
public:
    IndividualProgression_StageSnapshotPlayerScript() : PlayerScript("IndividualProgression_StageSnapshot",
        { PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST
#if defined(COSMIC_PLAYER_REWARDED_QUEST_HOOK_VERSION) && COSMIC_PLAYER_REWARDED_QUEST_HOOK_VERSION >= 1
          , PLAYERHOOK_ON_BEFORE_REWARDED_QUEST_CHANGE
#endif
        }) { }

#if defined(COSMIC_PLAYER_REWARDED_QUEST_HOOK_VERSION) && COSMIC_PLAYER_REWARDED_QUEST_HOOK_VERSION >= 1
    // Proposed Core pre-change hook (tests/core_patches/rewarded_quest_prechange_hook.patch):
    // every actual add/remove of a rewarded quest, before it happens, on the
    // mutating thread. Only markers invalidate; no Player mutation here.
    void OnPlayerBeforeRewardedQuestChange(Player* player, uint32 questId, bool /*rewarded*/) override
    {
        if (player && SnapshotApi::IsStageMarkerQuest(questId))
            sIndividualProgression->InvalidateStageSnapshot(player->GetGUID());
    }
#endif

    void OnPlayerLogin(Player* player) override
    {
        // New session epoch; Unknown until the owner thread publishes.
        sIndividualProgression->RegisterStageSnapshot(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (player)
            sIndividualProgression->EraseStageSnapshot(player->GetGUID());
    }

    void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
    {
        sIndividualProgression->PublishStageSnapshotIfDirty(player);
    }

    // Marker rewards made outside IP (quest hand-in, GM .quest reward): Core
    // calls this after the marker is set. A reward only raises the stage, so
    // the snapshot was conservative until now; invalidate for republish.
    void OnPlayerCompleteQuest(Player* player, Quest const* quest) override
    {
        if (player && quest && SnapshotApi::IsStageMarkerQuest(quest->GetQuestId()))
            sIndividualProgression->InvalidateStageSnapshot(player->GetGUID());
    }
};

// Marker removals made outside IP (GM .quest remove/complete/reward naming a
// marker): invalidate everything BEFORE Core runs the command. The quest-clear
// spell effect (Core SpellEffects.cpp, RemoveRewardedQuest) has no hook and
// remains a documented Core-lane gap.
class IndividualProgression_StageSnapshotCommandScript : public AllCommandScript
{
public:
    IndividualProgression_StageSnapshotCommandScript() : AllCommandScript("IndividualProgression_StageSnapshotCommand",
        { ALLCOMMANDHOOK_ON_TRY_EXECUTE_COMMAND }) { }

    bool OnTryExecuteCommand(ChatHandler& /*handler*/, std::string_view cmdStr) override
    {
        if (SnapshotApi::MentionsStageMarker(cmdStr))
            sIndividualProgression->InvalidateAllStageSnapshots();
        return true; // never blocks a command
    }
};

void AddSC_mod_individual_progression_stage_snapshot()
{
    new IndividualProgression_StageSnapshotPlayerScript();
    new IndividualProgression_StageSnapshotCommandScript();
}
