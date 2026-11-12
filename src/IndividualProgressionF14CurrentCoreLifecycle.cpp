// CosmicCuddle F14 current Core lifecycle observer. DEFAULT OFF.
// Does not register any positive visibility query or touch restoration debt.
#if defined(COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_CURRENT_CORE_LIFECYCLE_OBSERVER_EXPERIMENT >= 1
#if defined(COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT) && COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT >= 1
#if !defined(COSMIC_F14_SESSION_REBIND_EXPERIMENT) || COSMIC_F14_SESSION_REBIND_EXPERIMENT < 1
#error "Current F14 session-rebind observer requires the Core reconnect hook"
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
#if defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) && COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT >= 1
#if !defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) || COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT < 1
#error "Current F14 combat ownership requires login-thread verified actor publication"
#endif
#include "IndividualProgressionF14CurrentCombatOwnership.h"
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

    void EndCombatActorTarget(Player const* player)
    {
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
            CombatTracker().EndActor(actor);
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

    void ObserveCombatTarget(Player* player, Unit* target)
    {
        Combat::ActorKey const actor = CurrentCombatActor(player);
        if (!actor)
        {
            EndCombatActorSession(player);
            return;
        }

        Creature* creature = CurrentCombatCreature(player, target);
        if (!creature)
        {
            CombatTracker().EndActor(actor);
            return;
        }

        Combat::CreatureKey const key = CombatCreatureKey(creature, true);
        if (!key || !(key.map == actor.map))
        {
            CombatTracker().EndActor(actor);
            return;
        }

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

    class CurrentCombatCreatureScript final : public AllCreatureScript
    {
    public:
        CurrentCombatCreatureScript() : AllCreatureScript("IndividualProgression_F14CurrentCombatCreature") { }
        void OnCreatureRemoveWorld(Creature* creature) override
        {
            RetireCombatCreature(creature);
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
#if defined(COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT) && COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT >= 1
    new CurrentCombatPlayer();
    new CurrentCombatUnit();
    new CurrentCombatCreatureScript();
#endif
#endif
}
