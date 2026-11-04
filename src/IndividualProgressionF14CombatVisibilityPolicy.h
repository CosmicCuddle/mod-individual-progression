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

#include <cstdint>

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
        // Current active attack against THIS creature; old combat/threat is insufficient.
        bool eligibleHiddenRandomBotCurrentlyAttackingThisCreature = false;

        // ALL participants, not merely one positive attacker, must be
        // positively identified BE/Draenei rndbots or positively owned
        // minions of those exact bots. "Hidden by some other rule" is NOT
        // enough. A missing owner, unknown or human participant fails visible.
        bool everyParticipantVerifiedTbcRaceRandomBotOrItsMinion = false;
        bool noRealPlayerCombatThreatTapOrRights = false;
        bool noPendingThreatOrCombatantChanges = false;

        // Stable nonzero identity for THIS living creature instance, not
        // merely its template or spawn ID. Regeneration/respawn must issue a
        // fresh identity so stale observer facts cannot match a new entity.
        // A creature revision alone is unsafe: different creatures may both
        // start at generation 1.
        std::uint64_t creatureIdentityToken = 0;

        // Nonzero revision of this target's combat/attack/rights facts.
        // A participant, threat, active-attack or ownership change must
        // invalidate all previously checked viewer-target pairs.
        std::uint32_t generation = 0;
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

        // This is a viewer/creature PAIR fact set, not just a viewer cache.
        // Both the target's unique instance identity and current revision
        // must match. A cached result for creature A must never apply to B
        // even if A and B have the same revision value.
        std::uint64_t checkedAgainstCreatureIdentityToken = 0;
        std::uint32_t checkedAgainstCreatureGeneration = 0;

        // These two proofs must be evaluated from live Unit/attacker state
        // on the owning map thread for THIS visibility decision, not copied
        // from the creature or viewer cached snapshot. A dead Unit ALWAYS
        // overrides stale cached alive/attack=true; attack-stop / target
        // switch must likewise fail visible despite old combat snapshots.
        // No runtime reader exists in this source-only model yet.
        bool targetIsAliveAtThisEvaluation = false;
        bool eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = false;
    };

    constexpr bool ShouldConcealCreatureFromObserver(CreatureSnapshot const& c,
                                                     ObserverSnapshot const& o)
    {
        return c.creatureIdentityToken != 0 &&
               c.creatureIdentityToken == o.checkedAgainstCreatureIdentityToken &&
               c.generation != 0 &&
               c.generation == o.checkedAgainstCreatureGeneration &&
               o.explicitlyEnabled &&
               o.positivelyVerifiedNormalPlayer &&
               o.hasVanillaEffectiveProgression &&
               o.allParticipantsAlreadyHiddenFromThisObserver &&
               o.noOwnTargetAuraPetTotemGuardianOrSpellInFlight &&
               o.noObserverOrOwnedUnitThreatCombatOrPendingAssist &&
               o.provenOutsideCreatureAggroAssistCleaveAndAoeReach &&
               o.noObserverOrGroupQuestLootTapSkinOrKillCredit &&
               o.observerStageAndIdentityStable &&
               o.coreVisibilityAndThreatRefreshVerified &&
               o.targetIsAliveAtThisEvaluation &&
               o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation &&
               c.complete && c.current &&
               c.plainUnscriptedNonEliteHostileNpc &&
               c.alive && c.inCombat &&
               c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature &&
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
