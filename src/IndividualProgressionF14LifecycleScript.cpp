/*
 * CosmicCuddle IP F14 — attack/corpse lifecycle event consumer (DEFAULT OFF).
 *
 * Observes the optional Core prototype hooks (COSMIC_F14_ATTACK_EVENT_HOOK_VERSION)
 * plus the existing AllCreatureScript::OnCreatureRemoveWorld hook, and keeps
 * the pure IndividualProgressionF14LifecycleRegistry up to date.
 *
 * Bookkeeping ONLY. It hides nothing, sends no packets, requests no visibility
 * refresh, writes no stage/SQL/settings and never calls Playerbots. Actor
 * identity comes from the login-thread cache (GetF14VerifiedRandomBotLoginEpoch),
 * read under this file's registry mutex, so a logout racing an attack can
 * never resurrect an expired epoch.
 *
 * Pointer rule: every Unit or Creature pointer from a callback is turned into value
 * tokens before return and never stored. The only per-creature state is a
 * token stamped in the Creature's own CustomData, which dies with the object.
 *
 * Lock order: registry mutex -> F14 identity shared lock (inside the getter).
 * The logout path releases the identity lock BEFORE calling
 * ExpireF14LifecycleActor, so the order can never invert.
 */
#include "IndividualProgression.h"
#include "IndividualProgressionF14LifecycleRegistry.h"
#include "AllCreatureScript.h"
#include "Creature.h"
#include "DataMap.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <array>
#include <mutex>

namespace
{
    namespace Registry = IndividualProgressionF14LifecycleRegistry;
    namespace Policy = IndividualProgressionF14MultiAttackerPolicy;

    std::mutex& LifecycleLock()
    {
        static std::mutex lock;
        return lock;
    }

    Registry::Registry& LifecycleRegistry()
    {
        static Registry::Registry registry;
        return registry;
    }

    bool& PoisonLogged()
    {
        static bool logged = false;
        return logged;
    }

    // Must be called with LifecycleLock() held.
    void LogPoisonOnce()
    {
        Registry::Registry const& registry = LifecycleRegistry();
        if (!registry.IsPoisoned() || PoisonLogged())
            return;
        PoisonLogged() = true;
        LOG_ERROR("module", "IndividualProgression F14 lifecycle registry disabled (reason {}); all creatures stay visible until config reload",
            static_cast<uint32>(registry.GetPoisonReason()));
    }
}

void IndividualProgression::ResetF14LifecycleRegistry()
{
    std::lock_guard<std::mutex> guard(LifecycleLock());
    LifecycleRegistry().Reset();
    PoisonLogged() = false;
}

void IndividualProgression::ExpireF14LifecycleActor(std::uint64_t loginEpoch)
{
    // Runs even when the option is OFF: harmless on an empty registry and
    // guarantees an epoch recorded before a toggle can never outlive logout.
    if (!loginEpoch)
        return;
    std::lock_guard<std::mutex> guard(LifecycleLock());
    std::size_t const affected = LifecycleRegistry().ExpireActor(loginEpoch);
    if (affected)
        LOG_DEBUG("module", "IndividualProgression F14 lifecycle: login epoch {} expired from {} creature roster(s)", loginEpoch, affected);
}

IndividualProgression::F14LifecycleStatus IndividualProgression::GetF14LifecycleStatus()
{
    std::lock_guard<std::mutex> guard(LifecycleLock());
    Registry::Registry const& registry = LifecycleRegistry();
    F14LifecycleStatus status;
    status.enabled = f14ExperimentalAttackLifecycleRegistry;
    status.poisoned = registry.IsPoisoned();
    status.trackedCreatures = registry.Size();
    status.capacity = registry.Capacity();
    Registry::Counters const& c = registry.GetCounters();
    status.verifiedBegins = c.verifiedBegins;
    status.verifiedEnds = c.verifiedEnds;
    status.unverifiedMarks = c.unverifiedMarks;
    status.expiryFanout = c.expiryFanout;
    status.retired = c.corpseRetired + c.respawnRetired + c.evadeRetired + c.removedRetired + c.staleKeyReplaced;
    return status;
}

#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION) && COSMIC_F14_ATTACK_EVENT_HOOK_VERSION >= 1
namespace
{
    constexpr char const* StampKey = "IndividualProgression.F14.LifecycleIncarnation";

    struct F14CreatureIncarnation : public DataMap::Base
    {
        std::uint64_t token = 0;
    };

    // A non-pet, non-player-controlled creature in the world. Pets, guardians,
    // totems and charmed units of players are participants, never entries.
    Creature* AsWorldCreature(Unit* unit)
    {
        if (!unit || !unit->IsInWorld())
            return nullptr;
        Creature* creature = unit->ToCreature();
        if (!creature || creature->IsPet() || creature->IsTotem() ||
            creature->IsControlledByPlayer() || creature->GetCharmerOrOwnerPlayerOrPlayerItself())
            return nullptr;
        return creature;
    }

    Registry::CreatureKey KeyOf(Creature const* creature)
    {
        Registry::CreatureKey key;
        key.mapId = creature->GetMapId();
        key.instanceId = creature->GetInstanceId();
        key.guidRaw = creature->GetGUID().GetRawValue();
        return key;
    }

    std::uint64_t StampOf(Creature* creature)
    {
        if (F14CreatureIncarnation* stamp = creature->CustomData.Get<F14CreatureIncarnation>(StampKey))
            return stamp->token;
        return 0;
    }

    void SetStamp(Creature* creature, std::uint64_t token)
    {
        if (token == StampOf(creature))
            return;
        creature->CustomData.GetDefault<F14CreatureIncarnation>(StampKey)->token = token;
    }

    // Verified current RandomBot login epoch, or 0. Players only: a pet,
    // minion or charmed unit is unverified because no owner-resolution
    // contract exists yet. Must be called with LifecycleLock() held.
    std::uint64_t ActorTokenOf(Unit const* unit)
    {
        if (!unit || !unit->IsPlayer())
            return 0;
        return sIndividualProgression->GetF14VerifiedRandomBotLoginEpoch(unit->GetGUID());
    }

    bool LifecycleEnabled()
    {
        return sIndividualProgression->enabled && sIndividualProgression->f14ExperimentalAttackLifecycleRegistry;
    }
}

class IndividualProgressionF14LifecycleScript : public UnitScript
{
public:
    IndividualProgressionF14LifecycleScript()
        : UnitScript("IndividualProgression_F14Lifecycle", true,
                     {UNITHOOK_ON_F14_ATTACK_TARGET_TRANSITION,
                      UNITHOOK_ON_F14_CREATURE_CORPSE_READY,
                      UNITHOOK_ON_F14_CREATURE_RESPAWNED}) { }

    // Synchronous, inside Unit::Attack / Unit::AttackStop on the owning map
    // update. Pointers are valid only for this call and are never stored.
    void OnF14AttackTargetTransition(Unit* attacker, Unit* oldVictim, Unit* newVictim) override
    {
        if (!LifecycleEnabled() || !attacker || oldVictim == newVictim)
            return;

        Creature* oldCreature = AsWorldCreature(oldVictim);
        Creature* newCreature = AsWorldCreature(newVictim);
        Creature* selfCreature = AsWorldCreature(attacker);
        if (!oldCreature && !newCreature && !selfCreature)
            return;

        std::lock_guard<std::mutex> guard(LifecycleLock());
        Registry::Registry& registry = LifecycleRegistry();

        // A tracked creature choosing (or dropping) its own victim.
        if (selfCreature)
        {
            std::uint64_t const stamp = StampOf(selfCreature);
            if (stamp)
            {
                if (!newVictim)
                {
                    if (registry.OnCreatureStoppedAttacking(KeyOf(selfCreature), stamp))
                        SetStamp(selfCreature, 0);
                }
                else
                    registry.OnCreatureTargetChange(KeyOf(selfCreature), stamp, ActorTokenOf(newVictim) != 0);
            }
        }

        // A world creature attacking another creature is an unverified
        // participant of that creature; only a player can be verified.
        std::uint64_t const actor = selfCreature ? 0 : ActorTokenOf(attacker);

        if (oldCreature)
        {
            if (std::uint64_t const stamp = StampOf(oldCreature))
                registry.OnParticipantEnd(KeyOf(oldCreature), stamp, actor);
        }

        if (newCreature)
        {
            std::uint64_t const stamp = StampOf(newCreature);
            if (actor || stamp)
            {
                // Complete current attacker set of the creature (Core already
                // linked this attacker before the hook fired). Fixed-size and
                // allocation-free under the mutex: one slot beyond the policy
                // limit is enough for the registry to mark overflow uncertain.
                std::array<std::uint64_t, Policy::MaxTrackedAttackers + 1> others{};
                std::size_t otherCount = 0;
                for (Unit* other : newCreature->getAttackers())
                {
                    if (!other || other == attacker)
                        continue;
                    if (otherCount == others.size())
                        break; // > MaxTrackedAttackers: registry fails visible
                    others[otherCount++] = ActorTokenOf(other);
                }

                Registry::BeginResult const result = registry.OnParticipantBegin(
                    KeyOf(newCreature), stamp, actor, others.data(), otherCount);
                SetStamp(newCreature, result.stamp);
                if (result.created)
                    LOG_DEBUG("module", "IndividualProgression F14 lifecycle: tracking creature {} (incarnation {})",
                        newCreature->GetGUID().ToString(), result.stamp);
            }
        }

        LogPoisonOnce();
    }

    void OnF14CreatureCorpseReady(Creature* creature) override
    {
        RetireIncarnation(creature, false);
    }

    void OnF14CreatureRespawned(Creature* creature) override
    {
        RetireIncarnation(creature, true);
    }

private:
    static void RetireIncarnation(Creature* creature, bool respawn)
    {
        // Runs even when the option is OFF so a stamp can never survive
        // death or respawn into a later enabled period.
        if (!sIndividualProgression->enabled || !creature)
            return;
        std::lock_guard<std::mutex> guard(LifecycleLock());
        std::uint64_t const stamp = StampOf(creature);
        if (!stamp)
            return;
        LifecycleRegistry().OnCreatureCorpseOrRespawn(KeyOf(creature), stamp, respawn);
        SetStamp(creature, 0);
        LogPoisonOnce();
    }
};

// Existing Core hook (Creature::RemoveFromWorld calls it first, while the
// creature still has its map, instance id and GUID): despawn, grid or
// instance unload and RemoveFromMap retire the incarnation before the
// object can be destroyed or its GUID reused. No hiding, packets or stages.
class IndividualProgressionF14LifecycleRemovalScript : public AllCreatureScript
{
public:
    IndividualProgressionF14LifecycleRemovalScript()
        : AllCreatureScript("IndividualProgression_F14LifecycleRemoval") { }

    void OnCreatureRemoveWorld(Creature* creature) override
    {
        // Runs even when the option is OFF; only stamped creatures matter.
        if (!sIndividualProgression->enabled || !creature)
            return;
        std::lock_guard<std::mutex> guard(LifecycleLock());
        std::uint64_t const stamp = StampOf(creature);
        if (!stamp)
            return;
        LifecycleRegistry().OnCreatureRemovedFromWorld(KeyOf(creature), stamp);
        SetStamp(creature, 0);
        LogPoisonOnce();
    }
};
#endif

void AddSC_mod_individual_progression_f14_lifecycle()
{
#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION) && COSMIC_F14_ATTACK_EVENT_HOOK_VERSION >= 1
    new IndividualProgressionF14LifecycleScript();
    new IndividualProgressionF14LifecycleRemovalScript();
#endif
}
