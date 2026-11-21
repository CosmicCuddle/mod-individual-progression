#include "IndividualProgression.h"
#include "IndividualProgressionF14GroupSyncView.h"
#include "GroupMgr.h"
#include "GroupScript.h"
#include "PlayerScript.h"
#include "WorldScript.h"
#include "WorldSession.h"
#include <mutex>

namespace
{
    namespace View = IndividualProgressionF14GroupSyncView;
    std::mutex ViewLock;
    View::Snapshots<> GroupViews;

    bool OptionOn()
    {
        return sIndividualProgression->enabled && sIndividualProgression->f14ProtectVerifiedRandomBotGroupFloor;
    }

    void Request(ObjectGuid group)
    {
        if (!OptionOn() || !group)
            return;
        std::lock_guard<std::mutex> lock(ViewLock);
        GroupViews.Request(group.GetRawValue());
    }

    void Refresh()
    {
#if !defined(COSMIC_F14_SESSION_REBIND_EXPERIMENT) || COSMIC_F14_SESSION_REBIND_EXPERIMENT < 1 || \
    !defined(COSMIC_F14_SESSION_DISCONNECT_EXPERIMENT) || COSMIC_F14_SESSION_DISCONNECT_EXPERIMENT < 1
        // A cached human leader is unusable without connection invalidation.
        // Preserve earned quests and the approved TBC floor; do not borrow Wrath.
        std::lock_guard<std::mutex> unavailableLock(ViewLock);
        GroupViews.Clear();
        return;
#endif
        std::vector<View::Snapshots<>::Work> work;
        {
            std::lock_guard<std::mutex> lock(ViewLock);
            if (!OptionOn())
            {
                GroupViews.Clear();
                return;
            }
            work = GroupViews.TakeWork();
        }
        // WorldScript::OnUpdate follows completion of the map workers.
        // Only values crossed the boundary; resolve current objects here.
        for (auto const& item : work)
        {
            ObjectGuid const groupGuid(item.first);
            Group* group = sGroupMgr->GetGroupByGUID(groupGuid.GetCounter());
            if (!group || group->GetGUID() != groupGuid)
                continue;
            Player* leader = ObjectAccessor::FindPlayer(group->GetLeaderGUID());
            if (!leader || leader->GetGroup() != group || !leader->IsInWorld() || !leader->GetSession() ||
                leader->GetSession()->IsHeadless() || leader->GetSession()->IsSocketClosed() ||
                leader->IsGameMaster() || !sIndividualProgression->isNormalAccount(leader))
                continue;
            uint8 const stage = sIndividualProgression->GetEffectiveProgression(leader);
            std::lock_guard<std::mutex> lock(ViewLock);
            GroupViews.Publish(item, leader->GetGUID().GetRawValue(), stage);
        }
    }

    class GroupViewWorld final : public WorldScript
    {
    public:
        GroupViewWorld() : WorldScript("IndividualProgression_F14GroupViewWorld",
            {WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_AFTER_CONFIG_LOAD}) { }

        void OnUpdate(uint32 /*diff*/) override { Refresh(); }

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            std::lock_guard<std::mutex> lock(ViewLock);
            GroupViews.Clear();
        }
    };

    class GroupViewGroups final : public GroupScript
    {
    public:
        GroupViewGroups() : GroupScript("IndividualProgression_F14GroupViewGroups",
            {GROUPHOOK_ON_ADD_MEMBER, GROUPHOOK_ON_REMOVE_MEMBER, GROUPHOOK_ON_CHANGE_LEADER,
             GROUPHOOK_ON_DISBAND}) { }

        void OnAddMember(Group* group, ObjectGuid /*guid*/) override
        {
            sIndividualProgression->RequestF14GroupSyncView(group);
        }

        void OnRemoveMember(Group* group, ObjectGuid /*guid*/, RemoveMethod /*method*/,
                            ObjectGuid /*kicker*/, char const* /*reason*/) override
        {
            sIndividualProgression->RequestF14GroupSyncView(group);
        }

        void OnChangeLeader(Group* group, ObjectGuid /*newLeader*/, ObjectGuid /*oldLeader*/) override
        {
            sIndividualProgression->RequestF14GroupSyncView(group);
        }

        void OnDisband(Group* group) override
        {
            if (group)
            {
                std::lock_guard<std::mutex> lock(ViewLock);
                GroupViews.Forget(group->GetGUID().GetRawValue());
            }
        }
    };

    class GroupViewSessions final : public PlayerScript
    {
    public:
        GroupViewSessions() : PlayerScript("IndividualProgression_F14GroupViewSessions",
            {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_BEFORE_UPDATE
#if defined(COSMIC_F14_SESSION_REBIND_EXPERIMENT) && COSMIC_F14_SESSION_REBIND_EXPERIMENT >= 1
             , PLAYERHOOK_ON_F14_SESSION_REBOUND
#endif
#if defined(COSMIC_F14_SESSION_DISCONNECT_EXPERIMENT) && COSMIC_F14_SESSION_DISCONNECT_EXPERIMENT >= 1
             , PLAYERHOOK_ON_F14_SESSION_DISCONNECTED
#endif
            }) { }

        void OnPlayerLogin(Player* player) override { Changed(player); }
        void OnPlayerLogout(Player* player) override { Changed(player); }
        void OnPlayerBeforeUpdate(Player* player, uint32 /*diff*/) override
        {
            if (!OptionOn() || !player || !player->IsInWorld() || !player->GetSession() ||
                player->GetSession()->IsHeadless())
                return;
            Group* group = player->GetGroup();
            if (!group || group->GetLeaderGUID() != player->GetGUID())
                return;
            // Only the owned human leader is read here. Catch quest changes
            // outside IP's writers without reading another map's Player.
            uint8 const current = sIndividualProgression->GetEffectiveProgression(player);
            std::lock_guard<std::mutex> lock(ViewLock);
            uint8 published = 0;
            auto const guid = group->GetGUID().GetRawValue();
            GroupViews.EnsureRequested(guid);
            if (GroupViews.Read(guid, player->GetGUID().GetRawValue(), published) && current != published)
                GroupViews.Request(guid);
        }
#if defined(COSMIC_F14_SESSION_REBIND_EXPERIMENT) && COSMIC_F14_SESSION_REBIND_EXPERIMENT >= 1
        void OnF14PlayerSessionRebound(Player* player) override { Changed(player); }
#endif
#if defined(COSMIC_F14_SESSION_DISCONNECT_EXPERIMENT) && COSMIC_F14_SESSION_DISCONNECT_EXPERIMENT >= 1
        void OnF14PlayerSessionDisconnected(Player* player) override { Changed(player); }
#endif
    private:
        static void Changed(Player* player)
        {
            if (player)
                sIndividualProgression->RequestF14GroupSyncView(player->GetGroup());
        }
    };
}

bool IndividualProgression::UsesF14TemporaryGroupProgression(Player const* player)
{
    return OptionOn() && player && player->IsInWorld() && player->GetSession() &&
        player->GetSession()->IsHeadless() && !player->IsGameMaster() &&
        IndividualProgressionGroupPolicy::IsExpansionRace(player->getRace()) &&
        HasF14VerifiedRandomBotIdentity(player);
}

void IndividualProgression::RequestF14GroupSyncView(Group* group)
{
    // Called only with a group whose lifetime the invoking Core hook owns.
    // This does not resolve a leader/player on a map or callback thread.
    if (group)
        Request(group->GetGUID());
}

bool IndividualProgression::TryGetF14TemporaryGroupProgression(Player* player, uint8 earned, uint8& effective)
{
    if (!UsesF14TemporaryGroupProgression(player))
        return false;
    Group* group = player->GetGroup();
    if (!group)
        return false; // exact earned progression is exposed on leave

    uint8 stage = 0;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(ViewLock);
        auto const groupGuid = group->GetGUID().GetRawValue();
        GroupViews.EnsureRequested(groupGuid);
        found = GroupViews.Read(groupGuid, group->GetLeaderGUID().GetRawValue(), stage);
    }
    // Direct rewarded-quest edits outside IP are detected at the leader's
    // next update, not atomically. Until a Core mutation hook closes that
    // window, cached leader facts must not grant an unearned Wrath era.
    if (found && stage >= IndividualProgressionGroupPolicy::FirstWrathStage &&
        earned < IndividualProgressionGroupPolicy::FirstWrathStage)
        stage = IndividualProgressionGroupPolicy::FirstTbcStage;
    effective = found ? View::EffectiveStage(player->getRace(), earned, stage, progressionLimit)
                      : View::UnavailableStage(earned);
    return true;
}

void AddSC_mod_individual_progression_f14_group_sync_view()
{
    new GroupViewWorld();
    new GroupViewGroups();
    new GroupViewSessions();
}
