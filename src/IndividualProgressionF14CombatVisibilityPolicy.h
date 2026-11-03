/*
 * CosmicCuddle Individual Progression — F14 creature combat presentation.
 *
 * PURE, UNWIRED MODEL. No current Core/IP source includes this in a runtime
 * visibility hook, and no live config enables it.
 *
 * The actual per-creature state must eventually be refreshed from a safe
 * event-driven identity/threat snapshot, not computed by scanning participants
 * or consulting Playerbots inside the per-viewer visibility hot path.
 *
 * Every positive safety proof defaults false. Missing/expired facts => VISIBLE.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_COMBAT_VISIBILITY_POLICY_H
#define INDIVIDUAL_PROGRESSION_F14_COMBAT_VISIBILITY_POLICY_H

namespace IndividualProgressionF14CombatVisibilityPolicy
{
    // Cached per-creature evidence. Populate only on a safe map-thread event:
    // engage, new threat/victim, combatant join/leave, reset, death, despawn
    // and relevant RandomBot identity/ownership changes.
    struct CreatureSnapshot
    {
        bool complete = false;
        bool current = false;
        // Conservative first-pilot scope; rare/boss/escort/SmartAI/scripted,
        // vehicle/charmed/summoned, pet and other special units fail visible.
        bool plainUnscriptedNonEliteHostileNpc = false;
        bool alive = false;
        bool inCombat = false;

        // ALL participants, not merely one positive attacker, must be
        // positively identified BE/Draenei rndbots or positively owned
        // minions of those exact bots. "Hidden by some other rule" is NOT
        // enough. A missing owner, unknown or human participant fails visible.
        bool everyParticipantVerifiedTbcRaceRandomBotOrItsMinion = false;
        bool noRealPlayerCombatThreatTapOrRights = false;
        bool noPendingThreatOrCombatantChanges = false;
    };

    // Cheap per-observer evidence, with no threat-list walk or mutable PB
    // RandomPlayerbotMgr lookup during a visibility check.
    struct ObserverSnapshot
    {
        bool explicitlyEnabled = false;  // This is NOT a live config key.
        bool positivelyVerifiedNormalPlayer = false; // No GM/security bypass.
        bool hasVanillaEffectiveProgression = false;

        // Positive evidence from the SAME actual F14 player-hiding policy,
        // for every actor in the cached creature snapshot. Race is insufficient.
        bool allParticipantsAlreadyHiddenFromThisObserver = false;

        // "No interaction" must include target selection, existing auras and
        // DoTs, observer-owned pets/totems/guardians, missiles/spells in flight,
        // direct/assist threat and combat, and group-shared quest/loot/skin/
        // kill credit. Any uncertainty in these observations => visible.
        bool noOwnTargetAuraPetTotemGuardianOrSpellInFlight = false;
        bool noObserverOrOwnedUnitThreatCombatOrPendingAssist = false;
        bool provenOutsideCreatureAggroAssistCleaveAndAoeReach = false;
        bool noObserverOrGroupQuestLootTapSkinOrKillCredit = false;

        // If an era/identity transition or event-based visibility refresh is
        // pending, show the creature instead of risking invisible ghost hits.
        bool observerStageAndIdentityStable = false;

        // Cannot be supplied by today's pinned Core: requires an approved
        // observer-specific deny hook and an immediate threat/exit/death/
        // progression-change visibility invalidation implementation.
        bool coreVisibilityAndThreatRefreshVerified = false;
    };

    constexpr bool ShouldConcealCreatureFromObserver(CreatureSnapshot const& c,
                                                     ObserverSnapshot const& o)
    {
        return o.explicitlyEnabled &&
               o.positivelyVerifiedNormalPlayer &&
               o.hasVanillaEffectiveProgression &&
               o.allParticipantsAlreadyHiddenFromThisObserver &&
               o.noOwnTargetAuraPetTotemGuardianOrSpellInFlight &&
               o.noObserverOrOwnedUnitThreatCombatOrPendingAssist &&
               o.provenOutsideCreatureAggroAssistCleaveAndAoeReach &&
               o.noObserverOrGroupQuestLootTapSkinOrKillCredit &&
               o.observerStageAndIdentityStable &&
               o.coreVisibilityAndThreatRefreshVerified &&
               c.complete && c.current &&
               c.plainUnscriptedNonEliteHostileNpc &&
               c.alive && c.inCombat &&
               c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion &&
               c.noRealPlayerCombatThreatTapOrRights &&
               c.noPendingThreatOrCombatantChanges;
    }

    // A MODEL of why a runtime visibility refresh will be required.
    // It does NOT perform it. Event handlers must independently invalidate
    // cached facts and schedule refresh before hidden creatures can attack.
    constexpr bool ConcealmentDecisionChanged(CreatureSnapshot const& priorCreature,
                                              ObserverSnapshot const& priorObserver,
                                              CreatureSnapshot const& nextCreature,
                                              ObserverSnapshot const& nextObserver)
    {
        return ShouldConcealCreatureFromObserver(priorCreature, priorObserver) !=
               ShouldConcealCreatureFromObserver(nextCreature, nextObserver);
    }
}

#endif
