// CosmicCuddle F14 current Core lifecycle observer. DEFAULT OFF.
// Does not register any positive visibility query or touch restoration debt.
#if defined(COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT >= 1
#if defined(COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT >= 1
#if !defined(COSMIC_F14_SESSION_REBIND_EXPERIMENT) || COSMIC_F14_SESSION_REBIND_EXPERIMENT < 1
#error "Current F14 session-rebind observer requires the Core reconnect hook"
#endif
#endif
#if defined(COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT) && COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT >= 1
#if !defined(COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT) || COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT < 1
#error "Current F14 pass identity correlation requires reconnect-safe current viewer identity"
#endif
#endif
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
#if !defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) || COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT < 1
#error "Current F14 rights refresh requires current combat ownership"
#endif
#endif
#if defined(COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT >= 1
#if !defined(COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT) || COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT < 1
#error "Token-bound initial OUT observer requires exact current pass identity correlation"
#endif
#if !defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) || COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT < 1
#error "Token-bound initial OUT observer requires current verified RandomBot combat ownership"
#endif
#if !defined(COSMIC_F14_TOKEN_BOUND_CREATURE_OUT_QUERY_EXPERIMENT) || COSMIC_F14_TOKEN_BOUND_CREATURE_OUT_QUERY_EXPERIMENT < 1
#error "Token-bound initial OUT observer requires the Core18 token-bearing query ABI"
#endif
#endif
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
#include "IndividualProgressionF14OwnedRestoreShards.h"
#if defined(COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT) && COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT >= 1
#include "IndividualProgressionF14CurrentPassIdentity.h"
#endif
#if defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) && COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT >= 1
#if !defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) || COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT < 1
#error "Current F14 combat ownership requires login-thread verified actor publication"
#endif
#include "IndividualProgressionF14CurrentCombatOwnership.h"
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
#include "IndividualProgressionF14CurrentRightsRefresh.h"
#endif
#include "AllCreatureScript.h"
#include "Creature.h"
#include "Unit.h"
#include "UnitScript.h"
#include <mutex>
#endif
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

    // Experimental sizing ONLY. No payload is allocated by these callbacks:
    // there is deliberately no runtime Open/Reserve/Commit path. One owner
    // survives Map lifetime; future positive wiring requires approved sizing,
    // authenticated map ownership, R2 and a separate integration review.
    using DepartureStore = IndividualProgressionF14OwnedRestoreShards::Store<8, 64>;
    DepartureStore& RestorationStore()
    {
        static DepartureStore store;
        return store;
    }
    void BeforeDeparture(Player const* player)
    {
        if (!player)
            return;
        // Negative invalidation uses full raw GUID, not a stale cached map
        // generation or normal-account proof. It never grants bot/human rights.
        RestorationStore().BeforeViewerDeparture(
            player->GetGUID().IsPlayer() ? player->GetGUID().GetRawValue() : 0);
    }

#if defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) && COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT >= 1
    namespace Combat = IndividualProgressionF14CurrentCombatOwnership;
    constexpr char const* CombatActorMarkerKey = "IndividualProgression.F14.CurrentCombatActor";
    constexpr char const* CombatCreatureMarkerKey = "IndividualProgression.F14.CurrentCombatCreature";

    struct CombatActorMarker final : DataMap::Base
    {
        mutable std::mutex lock;
        mutable std::uint64_t guid = 0;
        mutable std::uint64_t loginEpoch = 0;
        mutable std::uint64_t mapGeneration = 0;
        mutable Combat::MapKey map{};
    };

    struct CombatCreatureMarker final : DataMap::Base
    {
        std::uint64_t incarnation = 0;
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
        IndividualProgressionF14CurrentRightsRefresh::Pending rightsRefresh;
#endif
    };

    Epoch::Issuer& CombatMapIssuer() { static Epoch::Issuer issuer; return issuer; }
    Epoch::Issuer& CombatCreatureIssuer() { static Epoch::Issuer issuer; return issuer; }

    using CurrentCombatTracker = Combat::Tracker<64>;
    CurrentCombatTracker& CombatTracker()
    {
        static CurrentCombatTracker tracker;
        return tracker;
    }

    Combat::MapKey CombatMap(Map const* map)
    {
        return map ? Combat::MapKey{map->GetId(), map->GetInstanceId()} : Combat::MapKey{};
    }

    bool PotentialCombatActor(Player const* player)
    {
        return player && player->GetGUID().IsPlayer() && player->IsInWorld() &&
            player->GetSession() && player->GetSession()->IsHeadless() &&
            !player->IsGameMaster() &&
            (player->getRace() == RACE_BLOODELF || player->getRace() == RACE_DRAENEI);
    }

    Combat::ActorKey CurrentCombatActor(Player* player)
    {
        if (!PotentialCombatActor(player) || !player->GetMap())
            return {};

        IndividualProgression::F14VerifiedActorPublication published;
        if (!sIndividualProgression->TryReadF14VerifiedActorPublication(player->GetGUID(), published) ||
            published.fullGuid != player->GetGUID() || !published.loginEpoch || !published.accountId ||
            published.accountId != player->GetSession()->GetAccountId() ||
            published.race != player->getRace())
            return {};

        CombatActorMarker* marker =
            player->CustomData.GetDefault<CombatActorMarker>(CombatActorMarkerKey);
        std::lock_guard<std::mutex> guard(marker->lock);
        Combat::MapKey const map = CombatMap(player->GetMap());
        if (marker->guid != player->GetGUID().GetRawValue() ||
            marker->loginEpoch != published.loginEpoch)
        {
            marker->guid = player->GetGUID().GetRawValue();
            marker->loginEpoch = published.loginEpoch;
            marker->mapGeneration = 0;
            marker->map = {};
        }

        if (!marker->mapGeneration || !(marker->map == map))
        {
            std::uint64_t const generation = CombatMapIssuer().Issue();
            if (!generation)
                return {};
            marker->mapGeneration = generation;
            marker->map = map;
        }
        return {marker->guid, marker->loginEpoch, marker->mapGeneration, marker->map};
    }

    Combat::ActorKey ReadCombatActor(Player const* player)
    {
        if (!player)
            return {};
        CombatActorMarker const* marker =
            player->CustomData.Get<CombatActorMarker>(CombatActorMarkerKey);
        if (!marker)
            return {};
        std::lock_guard<std::mutex> guard(marker->lock);
        return {marker->guid, marker->loginEpoch, marker->mapGeneration, marker->map};
    }

#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
    void RequestActorKeyCreatureRefresh(Map* map, Combat::ActorKey const& actor,
        IndividualProgressionF14CurrentRightsRefresh::Reason reason);
    void RequestActorCurrentCreatureRefresh(
        Player const* player, IndividualProgressionF14CurrentRightsRefresh::Reason reason);
#endif

    void EndCombatActorTarget(Player const* player)
    {
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
        RequestActorCurrentCreatureRefresh(player,
            IndividualProgressionF14CurrentRightsRefresh::Reason::AttackOrTargetChanged);
#endif
        Combat::ActorKey const actor = ReadCombatActor(player);
        if (actor)
            CombatTracker().EndActor(actor);
    }

    void LeaveCombatActorMap(Player const* player, Map const* map)
    {
        if (!player)
            return;
        CombatActorMarker const* marker =
            player->CustomData.Get<CombatActorMarker>(CombatActorMarkerKey);
        if (!marker)
            return;
        std::lock_guard<std::mutex> guard(marker->lock);
        Combat::ActorKey const actor{marker->guid, marker->loginEpoch,
            marker->mapGeneration, marker->map};
        if (!actor)
            return;
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
        RequestActorKeyCreatureRefresh(player->GetMap(), actor,
            IndividualProgressionF14CurrentRightsRefresh::Reason::CombatStateChanged);
#endif
        CombatTracker().EndActor(actor);
        if (!map || !(marker->map == CombatMap(map)))
        {
            marker->guid = 0;
            marker->loginEpoch = 0;
        }
        marker->mapGeneration = 0;
        marker->map = {};
    }

    void EndCombatActorSession(Player const* player)
    {
        if (!player)
            return;
        CombatActorMarker const* marker =
            player->CustomData.Get<CombatActorMarker>(CombatActorMarkerKey);
        if (!marker)
            return;
        std::lock_guard<std::mutex> guard(marker->lock);
        Combat::ActorKey const actor{marker->guid, marker->loginEpoch,
            marker->mapGeneration, marker->map};
        if (actor)
        {
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
            RequestActorKeyCreatureRefresh(player->GetMap(), actor,
                IndividualProgressionF14CurrentRightsRefresh::Reason::CombatStateChanged);
#endif
            CombatTracker().EndActor(actor);
            CombatTracker().ReleaseActor(actor);
        }
        marker->guid = 0;
        marker->loginEpoch = 0;
        marker->mapGeneration = 0;
        marker->map = {};
    }

    Creature* CurrentCombatCreature(Player const* actor, Unit* target)
    {
        if (!actor || !target || !actor->GetMap() || !target->IsInWorld())
            return nullptr;
        Creature* creature = target->ToCreature();
        if (!creature || !creature->IsAlive() || creature->IsPet() ||
            creature->IsTotem() || creature->IsControlledByPlayer() ||
            creature->GetCharmerOrOwnerPlayerOrPlayerItself() ||
            creature->GetMap() != actor->GetMap())
            return nullptr;
        return creature;
    }

    Combat::CreatureKey CombatCreatureKey(Creature* creature, bool create)
    {
        if (!creature)
            return {};
        CombatCreatureMarker* marker = create
            ? creature->CustomData.GetDefault<CombatCreatureMarker>(CombatCreatureMarkerKey)
            : creature->CustomData.Get<CombatCreatureMarker>(CombatCreatureMarkerKey);
        if (!marker)
            return {};
        if (create && !marker->incarnation)
            marker->incarnation = CombatCreatureIssuer().Issue();
        return {creature->GetGUID().GetRawValue(), marker->incarnation,
            {creature->GetMapId(), creature->GetInstanceId()}};
    }

#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
    namespace RightsRefresh = IndividualProgressionF14CurrentRightsRefresh;

    void RequestCreatureRightsRefresh(Creature* creature, RightsRefresh::Reason reason)
    {
        if (!creature || !creature->IsInWorld())
            return;
        CombatCreatureMarker* marker =
            creature->CustomData.Get<CombatCreatureMarker>(CombatCreatureMarkerKey);
        // Only a creature incarnation already observed by the current combat
        // tracker may create refresh work. Unknown creatures stay ordinary Core.
        if (!marker || !marker->incarnation)
            return;
        marker->rightsRefresh.Request(reason);
    }

    Creature* ResolveCurrentCombatCreature(Map* map, Combat::CreatureKey const& key)
    {
        if (!map || !key || key.map.mapId != map->GetId() ||
            key.map.instanceId != map->GetInstanceId())
            return nullptr;
        ObjectGuid const guid(key.guid);
        if (!guid.IsAnyTypeCreature())
            return nullptr;
        Creature* creature = map->GetCreature(guid);
        if (!creature || !creature->IsInWorld())
            return nullptr;
        Combat::CreatureKey const live = CombatCreatureKey(creature, false);
        return live && live == key ? creature : nullptr;
    }

    void RequestActorKeyCreatureRefresh(Map* map, Combat::ActorKey const& actor, RightsRefresh::Reason reason)
    {
        if (!map || !actor)
            return;
        Combat::ActorState const state = CombatTracker().InspectActor(actor.guid);
        if (!state.found || !state.active || !(state.actor == actor))
            return;
        if (Creature* creature = ResolveCurrentCombatCreature(map, state.creature))
            RequestCreatureRightsRefresh(creature, reason);
    }

    void RequestActorCurrentCreatureRefresh(Player const* player, RightsRefresh::Reason reason)
    {
        Combat::ActorKey const actor = ReadCombatActor(player);
        if (!actor || !player)
            return;
        RequestActorKeyCreatureRefresh(player->GetMap(), actor, reason);
    }

    void RequestParticipantRightsRefresh(Unit* unit, RightsRefresh::Reason reason)
    {
        if (!unit)
            return;
        if (Creature* creature = unit->ToCreature())
            RequestCreatureRightsRefresh(creature, reason);
        if (Player* owner = unit->GetCharmerOrOwnerPlayerOrPlayerItself())
            RequestActorCurrentCreatureRefresh(owner, reason);
    }

    void RequestInteractionRightsRefresh(Unit* source, Unit* target, RightsRefresh::Reason reason)
    {
        RequestParticipantRightsRefresh(target, reason);
        RequestParticipantRightsRefresh(source, reason);
    }

    bool ConsumeCreatureRightsRefresh(Creature* creature)
    {
        if (!creature)
            return false;
        CombatCreatureMarker* marker =
            creature->CustomData.Get<CombatCreatureMarker>(CombatCreatureMarkerKey);
        return marker && marker->rightsRefresh.Take() != 0;
    }

    void ClearCreatureRightsRefresh(Creature* creature)
    {
        if (creature)
            if (CombatCreatureMarker* marker =
                    creature->CustomData.Get<CombatCreatureMarker>(CombatCreatureMarkerKey))
                marker->rightsRefresh.Clear();
    }
#endif

    void ObserveCombatTarget(Player* player, Unit* target)
    {
        Combat::ActorKey const actor = CurrentCombatActor(player);
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
        Combat::ActorState const prior = actor ? CombatTracker().InspectActor(actor.guid) : Combat::ActorState{};
#endif
        if (!actor)
        {
            EndCombatActorSession(player);
            return;
        }

        Creature* creature = CurrentCombatCreature(player, target);
        if (!creature)
        {
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
            if (prior.found && prior.active && prior.actor == actor)
                if (Creature* oldCreature = ResolveCurrentCombatCreature(player->GetMap(), prior.creature))
                    RequestCreatureRightsRefresh(oldCreature, RightsRefresh::Reason::AttackOrTargetChanged);
#endif
            CombatTracker().EndActor(actor);
            return;
        }

        Combat::CreatureKey const key = CombatCreatureKey(creature, true);
        if (!key || !(key.map == actor.map))
        {
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
            if (prior.found && prior.active && prior.actor == actor)
                if (Creature* oldCreature = ResolveCurrentCombatCreature(player->GetMap(), prior.creature))
                    RequestCreatureRightsRefresh(oldCreature, RightsRefresh::Reason::AttackOrTargetChanged);
#endif
            CombatTracker().EndActor(actor);
            return;
        }

#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
        if (prior.found && prior.active && prior.actor == actor && !(prior.creature == key))
            if (Creature* oldCreature = ResolveCurrentCombatCreature(player->GetMap(), prior.creature))
                RequestCreatureRightsRefresh(oldCreature, RightsRefresh::Reason::AttackOrTargetChanged);
        RequestCreatureRightsRefresh(creature, RightsRefresh::Reason::AttackOrTargetChanged);
#endif

        static std::atomic<std::uint64_t> first{0};
        Combat::ObserveResult const result = CombatTracker().Observe(actor, key);
        if ((result == Combat::ObserveResult::Recorded ||
             result == Combat::ObserveResult::Replaced) &&
            first.fetch_add(1, std::memory_order_relaxed) == 0)
            LOG_INFO("module", "IP F14 current combat observer recorded first authenticated RandomBot target; no concealment or restoration authority");
    }

    void RetireCombatCreature(Creature* creature)
    {
        Combat::CreatureKey const key = CombatCreatureKey(creature, false);
        if (!key)
            return;
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
        RequestCreatureRightsRefresh(creature, RightsRefresh::Reason::CreatureRetired);
#endif
        CombatTracker().RetireCreature(key);
        if (CombatCreatureMarker* marker =
                creature->CustomData.Get<CombatCreatureMarker>(CombatCreatureMarkerKey))
            marker->incarnation = 0;
    }

#endif
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

#if defined(COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT) && COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT >= 1
    namespace PassIdentity = IndividualProgressionF14CurrentPassIdentity;
    using CurrentPassTracker = PassIdentity::Tracker<64>;

    CurrentPassTracker& PassTracker()
    {
        static CurrentPassTracker tracker;
        return tracker;
    }

    PassIdentity::ViewerIdentity CurrentPassViewer(Player const* viewer)
    {
        Session::Identity const identity = Current(viewer);
        if (!identity)
            return {};
        return {identity.guid, identity.epoch, identity.generation,
            {identity.map.mapId, identity.map.instanceId}};
    }

    void RevokePassViewer(Player const* viewer)
    {
        if (viewer && viewer->GetGUID().IsPlayer())
            PassTracker().RevokeViewer(viewer->GetGUID().GetRawValue());
    }
#endif

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

        void OnPlayerBeforeLogout(Player* player) override
        {
            BeforeDeparture(player);
            Logout(player);
        }
        void OnPlayerLogout(Player* player) override
        {
            BeforeDeparture(player);
            Logout(player);
        }
    };

    class CurrentViewerMap final : public AllMapScript
    {
    public:
        CurrentViewerMap() : AllMapScript("IndividualProgression_F14CurrentViewerMap",
            {ALLMAPHOOK_ON_PLAYER_ENTER_ALL, ALLMAPHOOK_ON_F14_BEFORE_PLAYER_MAP_REMOVAL}) { }
        void OnPlayerEnterAll(Map* map, Player* player) override { Enter(player, map); }
        void OnF14BeforePlayerMapRemoval(Map const* map, Player const* player, bool /*remove*/) override
        {
#if defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) && COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT >= 1
            LeaveCombatActorMap(player, map);
#endif
            BeforeDeparture(player);
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
#if defined(COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT) && COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT >= 1
    class CurrentPassIdentityObserver final : public WorldObjectScript
    {
    public:
        CurrentPassIdentityObserver() : WorldObjectScript("IndividualProgression_F14CurrentPassIdentity",
            {WORLDOBJECTHOOK_ON_F14_VIEWER_VISIBILITY_PASS_BEGIN,
             WORLDOBJECTHOOK_ON_F14_VIEWER_VISIBILITY_PASS_BATCH_SEND_ATTEMPT,
             WORLDOBJECTHOOK_ON_F14_VIEWER_VISIBILITY_PASS_END}) { }

        void OnF14ViewerVisibilityPassBegin(Player const* viewer, uint64 token) override
        {
            auto const result = PassTracker().Begin(token, CurrentPassViewer(viewer));
            if (result == PassIdentity::Result::Recorded &&
                _first.fetch_add(1, std::memory_order_relaxed) == 0)
                LOG_INFO("module", "IP F14 current pass identity correlated first notifier token to exact viewer epoch/map generation; no concealment authority");
            LogPoisonOnce(result);
        }

        void OnF14ViewerVisibilityPassBatchSendAttempt(Player const* viewer, uint64 token) override
        {
            LogPoisonOnce(PassTracker().ObserveBatch(token, CurrentPassViewer(viewer)));
        }

        void OnF14ViewerVisibilityPassEnd(Player const* viewer, uint64 token) override
        {
            LogPoisonOnce(PassTracker().End(token, CurrentPassViewer(viewer)));
        }

    private:
        void LogPoisonOnce(PassIdentity::Result result)
        {
            if ((result == PassIdentity::Result::Mismatch ||
                 result == PassIdentity::Result::Poisoned) &&
                _poisonLogged.fetch_add(1, std::memory_order_relaxed) == 0)
                LOG_ERROR("module", "IP F14 current pass identity correlation became uncertain; future concealment must remain fail-visible");
        }
        std::atomic<std::uint64_t> _first{0};
        std::atomic<std::uint64_t> _poisonLogged{0};
    };

    class CurrentPassIdentityLifecycle final : public PlayerScript
    {
    public:
        CurrentPassIdentityLifecycle() : PlayerScript("IndividualProgression_F14CurrentPassIdentityLifecycle",
            {PLAYERHOOK_ON_BEFORE_LOGOUT, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_F14_SESSION_REBOUND}) { }
        void OnPlayerBeforeLogout(Player* player) override { RevokePassViewer(player); }
        void OnPlayerLogout(Player* player) override { RevokePassViewer(player); }
        void OnF14PlayerSessionRebound(Player* player) override { RevokePassViewer(player); }
    };

    class CurrentPassIdentityMap final : public AllMapScript
    {
    public:
        CurrentPassIdentityMap() : AllMapScript("IndividualProgression_F14CurrentPassIdentityMap",
            {ALLMAPHOOK_ON_F14_BEFORE_PLAYER_MAP_REMOVAL}) { }
        void OnF14BeforePlayerMapRemoval(Map const* /*map*/, Player const* player, bool /*remove*/) override
        {
            RevokePassViewer(player);
        }
    };
#endif
#if defined(COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT >= 1
    class CurrentTokenBoundInitialOutObserver final : public WorldObjectScript
    {
    public:
        CurrentTokenBoundInitialOutObserver() : WorldObjectScript("IndividualProgression_F14TokenBoundInitialOutObserver",
            {WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_INITIAL_OUT_TOKEN_QUERY}) { }

        bool OnF14ShouldSuppressCreatureInitialOutForViewer(
            Player const* viewer, WorldObject const* target, uint64 passToken) override
        {
            // This bridge is intentionally OBSERVER-ONLY. It proves that the
            // Core18 nomination carries the exact currently active notifier
            // token and that the target has current verified RandomBot attack
            // ownership. Complete pair rights + restoration reservation remain
            // absent, therefore the answer is ALWAYS FALSE (visible).
            if (!viewer || !target || !passToken)
                return false;

            PassIdentity::ViewerIdentity const identity = CurrentPassViewer(viewer);
            if (!PassTracker().IsActive(passToken, identity))
                return false;

            Creature const* creature = target->ToCreature();
            if (!creature || !creature->IsAlive() || !viewer->GetMap() ||
                creature->GetMap() != viewer->GetMap())
                return false;

            Combat::CreatureKey const key =
                CombatCreatureKey(const_cast<Creature*>(creature), false);
            if (!key || key.map.mapId != identity.map.mapId ||
                key.map.instanceId != identity.map.instanceId ||
                CombatTracker().ActiveAttackers(key) == 0)
                return false;

            if (_observed.fetch_add(1, std::memory_order_relaxed) == 0)
                LOG_INFO("module", "IP F14 observed first token-bound initial OUT nomination with exact viewer identity and current verified RandomBot attack ownership; concealment still forced FALSE");
            return false;
        }

    private:
        std::atomic<std::uint64_t> _observed{0};
    };
#endif

#if defined(COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT >= 1
    class CurrentViewerSessionRebind final : public PlayerScript
    {
    public:
        CurrentViewerSessionRebind() : PlayerScript("IndividualProgression_F14CurrentViewerSessionRebind",
            {PLAYERHOOK_ON_F14_SESSION_REBOUND}) { }

        void OnF14PlayerSessionRebound(Player* player) override
        {
            // A different authenticated client now owns this same in-world
            // Player object. Preserve old restoration obligations before
            // invalidating identity; then authenticate and issue a fresh
            // viewer login epoch/map generation for the new session.
            BeforeDeparture(player);
            Logout(player);
            if (!EligibleSession(player) || !sIndividualProgression->isNormalAccount(player))
                return;
            std::uint64_t const epoch = LoginIssuer().Issue();
            if (!epoch)
                return;
            Marker* marker = player->CustomData.GetDefault<Marker>(MarkerKey);
            marker->state.BindLogin(player->GetGUID().GetRawValue(), epoch);
            if (player->IsInWorld())
                Enter(player, player->GetMap());
        }
    };
#endif

#if defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) && COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT >= 1
    class CurrentCombatPlayer final : public PlayerScript
    {
    public:
        CurrentCombatPlayer() : PlayerScript("IndividualProgression_F14CurrentCombatPlayer",
            {PLAYERHOOK_ON_PLAYER_ENTER_COMBAT, PLAYERHOOK_ON_PLAYER_LEAVE_COMBAT,
             PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_PLAYER_JUST_DIED,
             PLAYERHOOK_ON_BEFORE_LOGOUT, PLAYERHOOK_ON_LOGOUT}) { }

        void OnPlayerEnterCombat(Player* player, Unit* /*enemy*/) override
        {
            // Entering combat is not proof that the bot initiated an attack.
            // Only Unit::Attack current victim is positive target ownership.
            ObserveCombatTarget(player, player ? player->GetVictim() : nullptr);
        }

        void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
        {
            if (!PotentialCombatActor(player) &&
                !player->CustomData.Get<CombatActorMarker>(CombatActorMarkerKey))
                return;
            if (Unit* victim = player->GetVictim())
                ObserveCombatTarget(player, victim);
            else if (!player->IsInCombat())
                EndCombatActorTarget(player);
        }

        void OnPlayerLeaveCombat(Player* player) override { EndCombatActorTarget(player); }
        void OnPlayerJustDied(Player* player) override { EndCombatActorTarget(player); }
        void OnPlayerBeforeLogout(Player* player) override { EndCombatActorSession(player); }
        void OnPlayerLogout(Player* player) override { EndCombatActorSession(player); }
    };

#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
    class CurrentRightsRefreshUnit final : public UnitScript
    {
    public:
        CurrentRightsRefreshUnit() : UnitScript("IndividualProgression_F14CurrentRightsRefreshUnit", true,
            {UNITHOOK_ON_HEAL, UNITHOOK_ON_DAMAGE,
             UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
             UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN, UNITHOOK_MODIFY_HEAL_RECEIVED,
             UNITHOOK_ON_AURA_APPLY, UNITHOOK_ON_AURA_REMOVE, UNITHOOK_ON_UNIT_UPDATE,
             UNITHOOK_ON_UNIT_ENTER_EVADE_MODE, UNITHOOK_ON_UNIT_ENTER_COMBAT,
             UNITHOOK_ON_UNIT_EXIT_COMBAT, UNITHOOK_ON_UNIT_DEATH}) { }

        void OnHeal(Unit* healer, Unit* receiver, uint32& /*gain*/) override
        {
            RequestInteractionRightsRefresh(healer, receiver, RightsRefresh::Reason::HealOrAssist);
        }

        void OnDamage(Unit* attacker, Unit* victim, uint32& /*damage*/) override
        {
            RequestInteractionRightsRefresh(attacker, victim, RightsRefresh::Reason::DirectDamage);
        }

        void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& /*damage*/, SpellInfo const* /*spellInfo*/) override
        {
            RequestInteractionRightsRefresh(attacker, target, RightsRefresh::Reason::PeriodicOrSpellDamage);
        }

        void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& /*damage*/, SpellInfo const* /*spellInfo*/) override
        {
            RequestInteractionRightsRefresh(attacker, target, RightsRefresh::Reason::PeriodicOrSpellDamage);
        }

        void ModifyHealReceived(Unit* target, Unit* healer, uint32& /*heal*/, SpellInfo const* /*spellInfo*/) override
        {
            RequestInteractionRightsRefresh(healer, target, RightsRefresh::Reason::HealOrAssist);
        }

        void OnAuraApply(Unit* unit, Aura* /*aura*/) override
        {
            RequestParticipantRightsRefresh(unit, RightsRefresh::Reason::AuraChanged);
        }

        void OnAuraRemove(Unit* unit, AuraApplication* /*aurApp*/, AuraRemoveMode /*mode*/) override
        {
            RequestParticipantRightsRefresh(unit, RightsRefresh::Reason::AuraChanged);
        }

        void OnUnitUpdate(Unit* unit, uint32 /*diff*/) override
        {
            Creature* creature = unit ? unit->ToCreature() : nullptr;
            if (!creature || !creature->IsInWorld() || !ConsumeCreatureRightsRefresh(creature))
                return;
            // Map-thread only: no lifecycle/tracker lock is held here.
            // This requests ordinary Core reevaluation; it is not client ACK
            // and does not clear restoration debt or authorize concealment.
            creature->UpdateObjectVisibility(true);
            if (_first.fetch_add(1, std::memory_order_relaxed) == 0)
                LOG_INFO("module", "IP F14 current rights-change observer requested first owning-map creature visibility reevaluation; concealment remains forced FALSE");
        }

        void OnUnitEnterEvadeMode(Unit* unit, uint8 /*evadeReason*/) override
        {
            RequestParticipantRightsRefresh(unit, RightsRefresh::Reason::CombatStateChanged);
        }

        void OnUnitEnterCombat(Unit* unit, Unit* victim) override
        {
            RequestInteractionRightsRefresh(unit, victim, RightsRefresh::Reason::CombatStateChanged);
        }

        void OnUnitExitCombat(Unit* unit) override
        {
            RequestParticipantRightsRefresh(unit, RightsRefresh::Reason::CombatStateChanged);
        }

        void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
        {
            RequestParticipantRightsRefresh(unit, RightsRefresh::Reason::CreatureRetired);
        }

    private:
        std::atomic<std::uint64_t> _first{0};
    };
#endif

    class CurrentCombatUnit final : public UnitScript
    {
    public:
        CurrentCombatUnit() : UnitScript("IndividualProgression_F14CurrentCombatUnit",
            true, {UNITHOOK_ON_UNIT_DEATH}) { }

        void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
        {
            if (!unit)
                return;
            if (Player* player = unit->ToPlayer())
                EndCombatActorTarget(player);
            if (Creature* creature = unit->ToCreature())
                RetireCombatCreature(creature);
        }
    };

    class CurrentCombatCreatureRemoval final : public AllCreatureScript
    {
    public:
        CurrentCombatCreatureRemoval() : AllCreatureScript("IndividualProgression_F14CurrentCombatCreature") { }
        void OnCreatureRemoveWorld(Creature* creature) override
        {
            RetireCombatCreature(creature);
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
            // Removed objects cannot service a future Unit::Update. Ordinary
            // Core removal handles client unlink; discard the pending bit.
            ClearCreatureRightsRefresh(creature);
#endif
        }
    };
#endif
}
#endif

void AddSC_mod_individual_progression_f14_current_core_lifecycle()
{
#if defined(COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT >= 1
    new CurrentViewerLogin();
#if defined(COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT >= 1
    new CurrentViewerSessionRebind();
#endif
    new CurrentViewerMap();
    new CurrentViewerPass();
#if defined(COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT) && COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT >= 1
    new CurrentPassIdentityObserver();
    new CurrentPassIdentityLifecycle();
    new CurrentPassIdentityMap();
#endif
#if defined(COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT >= 1
    new CurrentTokenBoundInitialOutObserver();
#endif
#if defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) && COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT >= 1
    new CurrentCombatPlayer();
#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT) && COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT >= 1
    new CurrentRightsRefreshUnit();
#endif
    new CurrentCombatUnit();
    new CurrentCombatCreatureRemoval();
#endif
#endif
}
