/*
 * CosmicCuddle IP F14 — attack/corpse lifecycle event consumer (DEFAULT OFF).
 *
 * Observes the optional Core prototype hooks (COSMIC_F14_ATTACK_EVENT_HOOK_VERSION)
 * plus the existing AllCreatureScript::OnCreatureRemoveWorld hook, and keeps
 * the pure IndividualProgressionF14LifecycleRegistry up to date.
 *
 * Stage 2: existing UnitScript damage / periodic / heal / combat / evade
 * hooks are INVALIDATION evidence only, and a bounded complete threat +
 * combat + attacker + tap snapshot fills the creature half of the pure
 * F14CombatVisibilityPolicy::CreatureSnapshot. The observer half and the
 * Core refresh bridge do not exist here, so nothing can be concealed.
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
    status.retired = c.corpseRetired + c.respawnRetired + c.evadeRetired + c.removedRetired +
                     c.leftCombatRetired + c.staleKeyReplaced;
    status.evidenceInvalidations = c.evidenceInvalidations + c.assistInvalidations;
    status.snapshotsApplied = c.snapshotsApplied;
    status.snapshotsVerified = c.snapshotsVerified;
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

    // Conservative first-pilot creature scope. Anything rare, elite, boss,
    // scripted (ScriptName or AIName such as SmartAI/escort), summoned,
    // vehicle, charmed, pet/guardian/totem, trigger, guard, civilian or
    // carrying any NPC flag (quest giver, vendor, ...) fails visible.
    bool IsPlainUnscriptedNonEliteNpc(Creature const* creature)
    {
        CreatureTemplate const* tmpl = creature->GetCreatureTemplate();
        return tmpl && tmpl->rank == CREATURE_ELITE_NORMAL &&
               !creature->isWorldBoss() && !creature->IsDungeonBoss() &&
               !creature->IsSummon() && !creature->IsVehicle() && !creature->IsCharmed() &&
               !creature->IsPet() && !creature->IsGuardian() && !creature->IsTotem() &&
               !creature->IsTrigger() && !creature->IsGuard() && !creature->IsCivilian() &&
               creature->GetScriptId() == 0 && creature->GetAIName().empty() &&
               creature->GetNpcFlags() == UNIT_NPC_FLAG_NONE;
    }

    // Complete, bounded, allocation-free participant snapshot of a tracked
    // creature, taken on its map thread under LifecycleLock(). Every unit is
    // reduced to a token immediately; nothing is retained. Must only be
    // called from an event whose source is already rostered (see
    // OnParticipantEvidence) so a just-added participant is never missed.
    Registry::ParticipantSnapshot CollectSnapshot(Creature* creature)
    {
        Registry::ParticipantSnapshot snap;
        snap.alive = creature->IsAlive();
        snap.inCombat = creature->IsInCombat();
        snap.plainUnscriptedNonEliteHostileNpc = IsPlainUnscriptedNonEliteNpc(creature);

        // Tap: nobody, or one verified bot. Any loot group fails visible
        // (group membership is not proven here).
        ObjectGuid const tapper = creature->GetLootRecipientGUID();
        snap.tapHolderNoneOrVerifiedBot = creature->GetLootRecipientGroupGUID() == 0 &&
            (!tapper || (tapper.IsPlayer() &&
                         sIndividualProgression->GetF14VerifiedRandomBotLoginEpoch(tapper) != 0));

        bool complete = creature->GetThreatMgr().CanHaveThreatList() &&
                        creature->GetCombatManager().GetPvPCombatRefs().empty();
        bool unverifiedSeen = false;
        auto addParticipant = [&](Unit const* unit)
        {
            if (!complete)
                return;
            std::uint64_t const token = ActorTokenOf(unit);
            if (token == 0)
            {
                if (unverifiedSeen)
                    return;
                unverifiedSeen = true;
            }
            else
            {
                for (std::size_t i = 0; i < snap.participantCount; ++i)
                    if (snap.participants[i] == token)
                        return;
            }
            if (snap.participantCount == snap.participants.size())
            {
                complete = false; // more than MaxSnapshotParticipants
                return;
            }
            snap.participants[snap.participantCount++] = token;
        };

        // Every threat-list victim, offline references included.
        for (ThreatReference const* ref : creature->GetThreatMgr().GetUnsortedThreatList())
            addParticipant(ref ? ref->GetVictim() : nullptr);
        // Every PvE combat reference (threat implies combat, not vice versa).
        for (auto const& pair : creature->GetCombatManager().GetPvECombatRefs())
            addParticipant(pair.second ? pair.second->GetOther(creature) : nullptr);
        // Current attackers: participants AND the roster source.
        for (Unit* attacker : creature->getAttackers())
        {
            addParticipant(attacker);
            if (snap.attackerCount == snap.attackers.size())
            {
                complete = false;
                break;
            }
            snap.attackers[snap.attackerCount++] = ActorTokenOf(attacker);
        }

        snap.complete = complete;
        return snap;
    }

    // Damage / periodic tick / heal touching units. Pointers are used only
    // for this synchronous call. Returns quickly for untracked creatures
    // (the stamp lives in the creature's own CustomData on its map thread).
    void HandleParticipantEvent(Unit* source, Unit* target, bool mayBeAssist)
    {
        if (!LifecycleEnabled() || !target)
            return;

        if (Creature* tracked = AsWorldCreature(target))
        {
            if (!StampOf(tracked))
                return;
            std::lock_guard<std::mutex> guard(LifecycleLock());
            Registry::Registry& registry = LifecycleRegistry();
            std::uint64_t const stamp = StampOf(tracked);
            Registry::CreatureKey const key = KeyOf(tracked);
            // Creatures, pets, minions, totems, environment (null) and
            // unverified players are all unverified sources.
            std::uint64_t const sourceToken = AsWorldCreature(source) ? 0 : ActorTokenOf(source);
            if (!registry.OnParticipantEvidence(key, stamp, sourceToken) &&
                registry.NeedsSnapshot(key, stamp))
                registry.ApplyParticipantSnapshot(key, stamp, CollectSnapshot(tracked));
            LogPoisonOnce();
            return;
        }

        // A heal/HoT on a player gives the healer threat on that player's
        // creatures with no creature-side event: invalidate those rosters.
        if (mayBeAssist && target->IsPlayer() && source && source != target &&
            (source->IsPlayer() || source->IsControlledByPlayer()))
        {
            std::lock_guard<std::mutex> guard(LifecycleLock());
            std::uint64_t const receiver = ActorTokenOf(target);
            if (receiver)
                LifecycleRegistry().OnAssistOfActor(receiver, ActorTokenOf(source));
            LogPoisonOnce();
        }
    }

    // Combat end / evade of a tracked creature retires its incarnation.
    // Runs whenever a stamp exists, even with the option or IP disabled.
    void RetireOnCombatEnd(Unit* unit)
    {
        Creature* creature = AsWorldCreature(unit);
        if (!creature || !StampOf(creature))
            return;
        std::lock_guard<std::mutex> guard(LifecycleLock());
        std::uint64_t const stamp = StampOf(creature);
        if (!stamp)
            return;
        LifecycleRegistry().OnCreatureLeftCombat(KeyOf(creature), stamp);
        SetStamp(creature, 0);
        LogPoisonOnce();
    }
}

class IndividualProgressionF14LifecycleScript : public UnitScript
{
public:
    IndividualProgressionF14LifecycleScript()
        : UnitScript("IndividualProgression_F14Lifecycle", true,
                     {UNITHOOK_ON_F14_ATTACK_TARGET_TRANSITION,
                      UNITHOOK_ON_F14_CREATURE_CORPSE_READY,
                      UNITHOOK_ON_F14_CREATURE_RESPAWNED,
                      UNITHOOK_ON_DAMAGE,
                      UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
                      UNITHOOK_ON_HEAL,
                      UNITHOOK_ON_UNIT_ENTER_COMBAT,
                      UNITHOOK_ON_UNIT_EXIT_COMBAT,
                      UNITHOOK_ON_UNIT_ENTER_EVADE_MODE}) { }

    // Invalidation evidence only. Values are never modified, and no single
    // hook is ever treated as proof of a complete participant roster.
    void OnDamage(Unit* attacker, Unit* victim, uint32& /*damage*/) override
    {
        HandleParticipantEvent(attacker, victim, false);
    }

    // Called for periodic damage AND periodic heal ticks.
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* caster, uint32& /*amount*/, SpellInfo const* /*spellInfo*/) override
    {
        HandleParticipantEvent(caster, target, true);
    }

    void OnHeal(Unit* healer, Unit* receiver, uint32& /*gain*/) override
    {
        HandleParticipantEvent(healer, receiver, true);
    }

    // A tracked creature engaging a target: unverified targets are new
    // participants (same rule as its own target change).
    void OnUnitEnterCombat(Unit* unit, Unit* victim) override
    {
        if (!LifecycleEnabled())
            return;
        Creature* creature = AsWorldCreature(unit);
        if (!creature || !StampOf(creature))
            return;
        std::lock_guard<std::mutex> guard(LifecycleLock());
        if (std::uint64_t const stamp = StampOf(creature))
            LifecycleRegistry().OnCreatureTargetChange(KeyOf(creature), stamp, ActorTokenOf(victim) != 0);
        LogPoisonOnce();
    }

    void OnUnitExitCombat(Unit* unit) override
    {
        RetireOnCombatEnd(unit);
    }

    void OnUnitEnterEvadeMode(Unit* unit, uint8 /*evadeReason*/) override
    {
        RetireOnCombatEnd(unit);
    }

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
        // Runs even when the option or IP itself is OFF so a stamp can never
        // survive death or respawn into a later enabled period.
        if (!creature)
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
        // Runs even when the option or IP itself is OFF; only stamped
        // creatures matter.
        if (!creature)
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
