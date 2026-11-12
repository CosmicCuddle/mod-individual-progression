// CosmicCuddle F14 current Core lifecycle observer. DEFAULT OFF.
// Does not register any positive visibility query or touch restoration debt.
#if defined(COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT >= 1
#if !defined(COSMIC_F14_PRE_MAP_REMOVAL_EXPERIMENT) || COSMIC_F14_PRE_MAP_REMOVAL_EXPERIMENT < 1
#error "Current F14 lifecycle observer requires the Core pre-map-removal hook"
#endif
#if !defined(COSMIC_F14_NOTIFIER_PASS_LIFECYCLE_EXPERIMENT) || COSMIC_F14_NOTIFIER_PASS_LIFECYCLE_EXPERIMENT < 1
#error "Current F14 lifecycle observer requires the Core notifier pass hooks"
#endif
#if defined(COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT >= 1
#error "Do not combine the legacy and current F14 lifecycle observers"
#endif
#include "IndividualProgression.h"
#include "IndividualProgressionF14CurrentViewerSession.h"
#include "IndividualProgressionF14HumanViewerEpoch.h"
#include "AllMapScript.h"
#include "DataMap.h"
#include "Map.h"
#include "Player.h"
#include "PlayerScript.h"
#include "WorldObjectScript.h"
#include "WorldSession.h"
#include "Log.h"
#include <atomic>

namespace
{
    namespace Session = IndividualProgressionF14CurrentViewerSession;
    namespace Epoch = IndividualProgressionF14HumanViewerEpoch;
    constexpr char const* MarkerKey = "IndividualProgression.F14.CurrentCoreViewerSession";
    struct Marker final : DataMap::Base
    {
        // Only observer-owned metadata is mutable through a const Player
        // callback. Never modify gameplay state or retain a Core pointer.
        mutable Session::State state;
    };
    Epoch::Issuer& LoginIssuer() { static Epoch::Issuer issuer; return issuer; }
    Epoch::Issuer& MapIssuer() { static Epoch::Issuer issuer; return issuer; }

    Marker const* GetMarker(Player const* player)
    {
        return player ? player->CustomData.Get<Marker>(MarkerKey) : nullptr;
    }
    bool EligibleSession(Player const* player)
    {
        return player && player->GetGUID().IsPlayer() && player->GetSession() &&
            !player->GetSession()->IsHeadless() && !player->IsGameMaster();
    }
    Session::MapKey Key(Map const* map) { return {map->GetId(), map->GetInstanceId()}; }
    void Enter(Player const* player, Map const* map)
    {
        if (Marker const* marker = GetMarker(player))
        {
            if (!EligibleSession(player) || !map || !player->IsInWorld() || player->GetMap() != map)
            {
                marker->state.InvalidateMap();
                return;
            }
            marker->state.Enter(player->GetGUID().GetRawValue(), Key(map), MapIssuer().Issue());
        }
    }
    void Logout(Player const* player)
    {
        if (Marker const* marker = GetMarker(player))
            marker->state.Logout();
    }
    Session::Identity Current(Player const* player)
    {
        if (!EligibleSession(player) || !player->IsInWorld())
            return {};
        Marker const* marker = GetMarker(player);
        Map const* map = player->GetMap();
        return marker && map ? marker->state.Read(player->GetGUID().GetRawValue(), Key(map)) : Session::Identity{};
    }

    class CurrentViewerLogin final : public PlayerScript
    {
    public:
        CurrentViewerLogin() : PlayerScript("IndividualProgression_F14CurrentViewerLogin",
            {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_BEFORE_LOGOUT, PLAYERHOOK_ON_LOGOUT}) { }
        void OnPlayerLogin(Player* player) override
        {
            Logout(player);
            if (!EligibleSession(player) || !sIndividualProgression->isNormalAccount(player))
                return;
            std::uint64_t const epoch = LoginIssuer().Issue();
            if (!epoch)
                return;
            Marker* marker = player->CustomData.GetDefault<Marker>(MarkerKey);
            marker->state.BindLogin(player->GetGUID().GetRawValue(), epoch);
            // Initial map insertion may precede login. Initial visibility
            // is ordinary Core behavior; identity starts only now.
            if (player->IsInWorld())
                Enter(player, player->GetMap());
        }
        void OnPlayerBeforeLogout(Player* player) override { Logout(player); }
        void OnPlayerLogout(Player* player) override { Logout(player); }
    };

    class CurrentViewerMap final : public AllMapScript
    {
    public:
        CurrentViewerMap() : AllMapScript("IndividualProgression_F14CurrentViewerMap",
            {ALLMAPHOOK_ON_PLAYER_ENTER_ALL, ALLMAPHOOK_ON_F14_BEFORE_PLAYER_MAP_REMOVAL}) { }
        void OnPlayerEnterAll(Map* map, Player* player) override { Enter(player, map); }
        void OnF14BeforePlayerMapRemoval(Map const* map, Player const* player, bool /*remove*/) override
        {
            if (Marker const* marker = GetMarker(player))
            {
                if (!map || !player->IsInWorld() || player->GetMap() != map)
                {
                    marker->state.InvalidateMap();
                    return;
                }
                marker->state.Leave(player->GetGUID().GetRawValue(), Key(map));
            }
        }
    };

    class CurrentViewerPass final : public WorldObjectScript
    {
    public:
        CurrentViewerPass() : WorldObjectScript("IndividualProgression_F14CurrentViewerPass",
            {WORLDOBJECTHOOK_ON_F14_VIEWER_VISIBILITY_PASS_BEGIN}) { }
        void OnF14ViewerVisibilityPassBegin(Player const* viewer, uint64 token) override
        {
            if (!token || !Current(viewer))
                return;
            if (_observed.fetch_add(1, std::memory_order_relaxed) == 0)
                LOG_INFO("module", "IP F14 current Core observer sampled a viewer identity at pass begin; no visibility or restoration authority");
        }
    private:
        std::atomic<std::uint64_t> _observed{0};
    };
}
#endif

void AddSC_mod_individual_progression_f14_current_core_lifecycle()
{
#if defined(COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT >= 1
    new CurrentViewerLogin();
    new CurrentViewerMap();
    new CurrentViewerPass();
#endif
}
