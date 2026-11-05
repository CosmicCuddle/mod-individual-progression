/*
 * CosmicCuddle IP F14 — attack/corpse lifecycle event consumer (DEFAULT OFF).
 *
 * Observes the optional Core prototype hooks (COSMIC_F14_ATTACK_EVENT_HOOK_VERSION)
 * plus the existing AllCreatureScript::OnCreatureRemoveWorld hook, and keeps
 * the pure IndividualProgressionF14LifecycleRegistry up to date.
 *
 * Stage 2: existing UnitScript damage / periodic / heal / combat / evade
 * hooks are INVALIDATION evidence only, and a bounded threat + combat +
 * attacker + tap snapshot is recorded as NON-AUTHORITATIVE conservative
 * evidence. It can only add uncertainty; it never sets any participant or
 * rights proof, and nothing consumes it for visibility.
 *
 * Bookkeeping ONLY by default. A separately compiled, experimental Core
 * post-event request bridge can request a bounded visibility PASS when a
 * previously tracked creature changes state. It NEVER denies visibility,
 * sends packets directly, writes stage/SQL/settings or calls Playerbots. Actor
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
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
#include "IndividualProgressionF14LogoutRefreshQueue.h"
#include <atomic>
#endif
#include "AllCreatureScript.h"
#include "Creature.h"
#include "DataMap.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
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

#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
    using LogoutQueue = IndividualProgressionF14LogoutRefreshQueue::Queue<>;

    // Both queue operations and registry writes use LifecycleLock().
    // Atomic flag makes extra Unit::Update work negligible unless a
    // verified owner logs out OR a previously tracked NPC's rights change.
    LogoutQueue& PendingLogoutRefresh()
    {
        static LogoutQueue pending;
        return pending;
    }

    std::atomic<bool>& HasPendingLogoutRefresh()
    {
        static std::atomic<bool> pending{false};
        return pending;
    }

    bool& LogoutRefreshOverflowLogged()
    {
        static bool logged = false;
        return logged;
    }
#endif

    // Must be called with LifecycleLock() held. Queue failure is independent
    // of registry poison and can originate from human damage/HoT/assist fanout.
    void LogPoisonOnce()
    {
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
        if (PendingLogoutRefresh().IsPoisoned() && !LogoutRefreshOverflowLogged())
        {
            LogoutRefreshOverflowLogged() = true;
            LOG_ERROR("module", "IndividualProgression F14 experimental map-thread refresh queue exhausted; no further requests accepted, concealment must remain disabled");
        }
#endif
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
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
    PendingLogoutRefresh().Reset();
    HasPendingLogoutRefresh().store(false, std::memory_order_release);
    LogoutRefreshOverflowLogged() = false;
#endif
}

void IndividualProgression::ExpireF14LifecycleActor(std::uint64_t loginEpoch)
{
    // Runs even when the option is OFF: harmless on an empty registry and
    // guarantees an epoch recorded before a toggle can never outlive logout.
    if (!loginEpoch)
        return;
    std::lock_guard<std::mutex> guard(LifecycleLock());
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
    // Logout can run independently of Core AttackStop. Queue ONLY copied
    // creature keys and stamp values, never Creature*/Map*/Player*. The
    // target itself consumes the request on its next owning-map update.
    auto& pending = PendingLogoutRefresh();
    std::size_t const affected = LifecycleRegistry().ExpireActor(
        loginEpoch, [&pending](Registry::CreatureKey const& key, std::uint64_t stamp)
        {
            pending.Remember(key, stamp);
        });
    HasPendingLogoutRefresh().store(pending.HasPending(), std::memory_order_release);
    LogPoisonOnce();
#else
    std::size_t const affected = LifecycleRegistry().ExpireActor(loginEpoch);
#endif
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
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
    // A poisoned refresh queue cannot satisfy future viewer restores even if
    // the combat registry itself is healthy. Surface this as unhealthy.
    status.poisoned = status.poisoned || PendingLogoutRefresh().IsPoisoned();
#endif
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
    status.snapshotsOnlyVerifiedBots = c.snapshotsOnlyVerifiedBots;
    return status;
}

#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION) && COSMIC_F14_ATTACK_EVENT_HOOK_VERSION >= 1
namespace
{
    constexpr char const* StampKey = "IndividualProgression.F14.LifecycleIncarnation";

    struct F14CreatureIncarnation : public DataMap::Base
    {
        std::uint64_t token = 0;
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
        // The one-shot request survives token=0 after corpse/last attack.
        bool everTracked = false;
        bool refreshPending = false;
#endif
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
        F14CreatureIncarnation* state = creature->CustomData.GetDefault<F14CreatureIncarnation>(StampKey);
        state->token = token;
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
        if (token)
        {
            state->everTracked = true;
            state->refreshPending = true;
        }
#endif
    }

#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
    // Caller holds LifecycleLock(); no allocation for an unknown creature.
    void MarkPendingRefresh(Creature* creature)
    {
        if (!creature)
            return;
        if (F14CreatureIncarnation* state = creature->CustomData.Get<F14CreatureIncarnation>(StampKey))
            if (state->everTracked)
                state->refreshPending = true;
    }
#endif

    // Verified current RandomBot login epoch, or 0. Players only: a pet,
    // minion or charmed unit is NEVER a first-party bot token (see
    // MinionOwnerEpochOf for separate, weaker minion evidence). Must be
    // called with LifecycleLock() held.
    std::uint64_t ActorTokenOf(Unit const* unit)
    {
        if (!unit || !unit->IsPlayer())
            return 0;
        return sIndividualProgression->GetF14VerifiedRandomBotLoginEpoch(unit->GetGUID());
    }

    // Stage 3: value-only minion lineage. Returns the OWNER's current
    // verified login epoch for a pet/guardian/totem/summon only when every
    // GUID in its lineage agrees with one player owner; otherwise 0
    // (unknown => visible). No owner/charmer/summoner pointer is ever
    // dereferenced and Playerbots is never consulted. LifecycleLock() held.
    std::uint64_t MinionOwnerEpochOf(Unit const* unit)
    {
        if (!unit || unit->IsPlayer())
            return 0;
        Creature const* minion = unit->ToCreature();
        if (!minion)
            return 0;
        Registry::MinionLineage lineage;
        lineage.minionType = minion->IsPet() || minion->IsGuardian() || minion->IsTotem() || minion->IsSummon();
        lineage.vehicleOrPossessed = minion->IsVehicle() || minion->isPossessed();
        ObjectGuid const owner = minion->GetOwnerGUID();
        lineage.owner = owner.GetRawValue();
        lineage.ownerIsPlayer = owner.IsPlayer();
        lineage.charmer = minion->GetCharmerGUID().GetRawValue();
        lineage.creator = minion->GetCreatorGUID().GetRawValue();
        lineage.summon = minion->IsSummon();
        if (TempSummon const* summon = minion->ToTempSummon())
        {
            lineage.summonDataAvailable = true;
            lineage.summoner = summon->GetSummonerGUID().GetRawValue();
        }
        if (!Registry::LineageMatchesSingleOwner(lineage))
            return 0;
        return Registry::MinionOwnerEvidence(lineage,
            sIndividualProgression->GetF14VerifiedRandomBotLoginEpoch(owner));
    }

    // One visited entry: a first-party verified bot, else a verified-owner
    // minion, else unknown. Returns false when the caller must break.
    bool VisitUnit(Registry::SnapshotCollector& collector, Unit const* unit, bool attacking)
    {
        if (std::uint64_t const bot = ActorTokenOf(unit))
            return attacking ? collector.AddAttacker(bot) : collector.AddParticipant(bot);
        if (std::uint64_t const owner = MinionOwnerEpochOf(unit))
            return collector.AddMinion(owner);
        return collector.AddParticipant(0);
    }

    bool LifecycleEnabled()
    {
        return sIndividualProgression->enabled && sIndividualProgression->f14ExperimentalAttackLifecycleRegistry;
    }

    // Conservative first-pilot creature TYPE scope (hostility is NOT checked
    // here and never claimed). Anything rare, elite, boss,
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

    // Bounded, allocation-free, NON-AUTHORITATIVE participant snapshot of a
    // tracked creature, taken on its map thread under LifecycleLock(). Every
    // unit is reduced to a token immediately; nothing is retained. Only
    // called from an event whose source is already rostered (see
    // OnParticipantEvidence). It is not a census of every participant or
    // right; it can only add uncertainty in the registry.
    Registry::ParticipantSnapshot CollectSnapshot(Creature* creature)
    {
        bool const alive = creature->IsAlive();
        bool const inCombat = creature->IsInCombat();
        // Type-scope exclusion only; hostility is NOT verified here.
        bool const plainScope = IsPlainUnscriptedNonEliteNpc(creature);

        // Tap: nobody, or one verified bot. Any loot group fails visible
        // (group membership is not proven here).
        ObjectGuid const tapper = creature->GetLootRecipientGUID();
        bool const tapOk = creature->GetLootRecipientGroupGUID() == 0 &&
            (!tapper || (tapper.IsPlayer() &&
                         sIndividualProgression->GetF14VerifiedRandomBotLoginEpoch(tapper) != 0));

        // Enumerate with a fixed TOTAL visit budget (duplicates included)
        // and break each loop immediately when the collector says stop, so
        // the global LifecycleLock is never held for an unbounded scan.
        Registry::SnapshotCollector collector;
        // API unavailability or a creature not in its map => unknown => visible.
        if (!creature->IsInWorld() || !creature->FindMap() ||
            !creature->GetThreatMgr().CanHaveThreatList() ||
            !creature->GetCombatManager().GetPvPCombatRefs().empty())
            collector.Fail();

        // Every threat-list victim, offline references included.
        if (!collector.Stopped())
            for (ThreatReference const* ref : creature->GetThreatMgr().GetUnsortedThreatList())
                if (!VisitUnit(collector, ref ? ref->GetVictim() : nullptr, false))
                    break;
        // Every PvE combat reference (threat implies combat, not vice versa).
        if (!collector.Stopped())
            for (auto const& pair : creature->GetCombatManager().GetPvECombatRefs())
                if (!VisitUnit(collector, pair.second ? pair.second->GetOther(creature) : nullptr, false))
                    break;
        // Current attackers: participants AND attacker set (minions are
        // recorded only as minion-owner evidence, never as attackers).
        if (!collector.Stopped())
            for (Unit* attacker : creature->getAttackers())
                if (!VisitUnit(collector, attacker, true))
                    break;

        Registry::ParticipantSnapshot snap = collector.Take();
        snap.alive = alive;
        snap.inCombat = inCombat;
        snap.plainUnscriptedNonEliteNpcScope = plainScope;
        snap.tapHolderNoneOrVerifiedBot = tapOk;
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
            bool evidenceChanged = registry.OnParticipantEvidence(key, stamp, sourceToken);
            if (!evidenceChanged && registry.NeedsSnapshot(key, stamp))
            {
                registry.ApplyParticipantSnapshot(key, stamp, CollectSnapshot(tracked));
                evidenceChanged = true; // a new conservative snapshot can revoke old rights.
            }
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
            // Real-player damage/DoT/heal and unexpected owners invalidate
            // previous observer facts. Request one future owning-map pass,
            // NEVER a visibility update inside this damage/heal callback.
            if (evidenceChanged)
            {
                LogoutQueue& pending = PendingLogoutRefresh();
                pending.Remember(key, stamp);
                HasPendingLogoutRefresh().store(pending.HasPending(), std::memory_order_release);
            }
#endif
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
            {
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
                // Healing a verified bot may create new threat/assist rights
                // for an unseen human without touching the creature itself.
                // Fan out value-only affected creature identities under lock.
                LogoutQueue& pending = PendingLogoutRefresh();
                LifecycleRegistry().OnAssistOfActor(receiver, ActorTokenOf(source),
                    [&pending](Registry::CreatureKey const& key, std::uint64_t stamp)
                    {
                        pending.Remember(key, stamp);
                    });
                HasPendingLogoutRefresh().store(pending.HasPending(), std::memory_order_release);
#else
                LifecycleRegistry().OnAssistOfActor(receiver, ActorTokenOf(source));
#endif
            }
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
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
        MarkPendingRefresh(creature);
#endif
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
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
                      UNITHOOK_ON_F14_CREATURE_VISIBILITY_REFRESH_REQUEST,
                      UNITHOOK_ON_UNIT_UPDATE,
#endif
                      UNITHOOK_ON_DAMAGE,
                      UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK,
                      UNITHOOK_ON_HEAL,
                      UNITHOOK_ON_UNIT_ENTER_COMBAT,
                      UNITHOOK_ON_UNIT_EXIT_COMBAT,
                      UNITHOOK_ON_UNIT_ENTER_EVADE_MODE}) { }

#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
    // Map-thread handoff of logged-out owner / changed threat-rights evidence. The queue stores no
    // pointers; a different object with a recycled creature GUID cannot
    // consume an expired incarnation stamp. Always drop the lock BEFORE
    // asking Core to refresh links or walking any nearby viewers.
    void OnUnitUpdate(Unit* unit, uint32 /*diff*/) override
    {
        if (!HasPendingLogoutRefresh().load(std::memory_order_acquire))
            return;
        Creature* creature = AsWorldCreature(unit);
        if (!creature)
            return;
        bool matched = false;
        {
            std::lock_guard<std::mutex> guard(LifecycleLock());
            LogoutQueue& pending = PendingLogoutRefresh();
            matched = pending.Take(KeyOf(creature), StampOf(creature));
            HasPendingLogoutRefresh().store(pending.HasPending(), std::memory_order_release);
            if (matched)
                MarkPendingRefresh(creature);
        }
        if (matched && creature->IsInWorld() &&
            sScriptMgr->ShouldF14RefreshCreatureVisibility(creature))
            creature->UpdateObjectVisibility(true);
    }
#endif

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
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
                MarkPendingRefresh(selfCreature);
#endif
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
            {
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
                MarkPendingRefresh(oldCreature);
#endif
                registry.OnParticipantEnd(KeyOf(oldCreature), stamp, actor);
            }
        }

        if (newCreature)
        {
            std::uint64_t const stamp = StampOf(newCreature);
            if (actor || stamp)
            {
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
                MarkPendingRefresh(newCreature);
#endif
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
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
                MarkPendingRefresh(newCreature);
#endif
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

#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
    // Called after lifecycle bookkeeping; only requests a Core visibility
    // PASS, never denial. The request stays valid after stamp retirement.
    bool OnF14ShouldRefreshCreatureVisibility(Creature const* creature) override
    {
        if (!creature || !creature->IsInWorld() || creature->IsPet())
            return false;
        // Release the mutex BEFORE Core performs UpdateObjectVisibility.
        std::lock_guard<std::mutex> guard(LifecycleLock());
        F14CreatureIncarnation* state = creature->CustomData.Get<F14CreatureIncarnation>(StampKey);
        if (!state || !state->everTracked || !state->refreshPending)
            return false;
        state->refreshPending = false;
        return true;
    }
#endif

private:
    static void RetireIncarnation(Creature* creature, bool respawn)
    {
        // Runs even when the option or IP itself is OFF so a stamp can never
        // survive death or respawn into a later enabled period.
        if (!creature)
            return;
        std::lock_guard<std::mutex> guard(LifecycleLock());
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
        // Respawn may follow a corpse callback which retired token=0.
        // Remember prior tracking independently of CURRENT hide eligibility.
        MarkPendingRefresh(creature);
#endif
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
#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT) && COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1
        // Map removal cannot deliver a WorldObject visibility pass.
        // Retire the copied value-only request without keeping any pointers.
        PendingLogoutRefresh().Discard(KeyOf(creature));
        HasPendingLogoutRefresh().store(
            PendingLogoutRefresh().HasPending(), std::memory_order_release);
#endif
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
