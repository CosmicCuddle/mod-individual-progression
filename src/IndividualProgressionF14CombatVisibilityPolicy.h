/*
 * CosmicCuddle Individual Progression — F14 creature-combat visibility policy.
 *
 * PURE, UNWIRED DEVELOPMENT MODEL. There is no runtime hook or configuration
 * reader for this policy. All facts must eventually come from safe, verified
 * observer/target snapshots; this file must not query Playerbots on map threads.
 * Without all required evidence, the policy preserves stock visibility.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_COMBAT_VISIBILITY_POLICY_H
#define INDIVIDUAL_PROGRESSION_F14_COMBAT_VISIBILITY_POLICY_H

namespace IndividualProgressionF14CombatVisibilityPolicy
{
    struct Facts
    {
        // The implementation must leave this OFF until independently approved.
        bool explicitlyEnabled = false;
        bool observerIsVanilla = false;
        bool observerIsGameMaster = false;
        bool targetIsHostileCreature = false;
        bool targetIsAlive = false;
        bool targetInCombat = false;

        // These require the SAME positive, observer-specific RandomBot identity
        // and actual player-visibility decision as the eventual F14 bot filter.
        bool tbcRaceAttackerVerifiedRandomBot = false;
        bool tbcRaceAttackerActuallyHiddenFromObserver = false;
        bool everyCombatParticipantHiddenFromObserver = false;

        // Fail open unless complete, current, concurrency-safe facts prove that
        // concealing the NPC cannot strand another player's target/combat/loot.
        bool interactionSnapshotComplete = false;
        bool observerAlreadyTargetingOrInteracting = false;
        bool anyRealPlayerInvolvedWithTarget = false;
        bool questLootTapOrSkinRightsAtRisk = false;
        bool bossEscortSummonOrOtherSpecialTarget = false;
        bool visibilityOrCombatTransitionPending = false;
    };

    constexpr bool ShouldConcealCreatureFromObserver(Facts const& f)
    {
        return f.explicitlyEnabled &&
               f.observerIsVanilla &&
               !f.observerIsGameMaster &&
               f.targetIsHostileCreature &&
               f.targetIsAlive &&
               f.targetInCombat &&
               f.tbcRaceAttackerVerifiedRandomBot &&
               f.tbcRaceAttackerActuallyHiddenFromObserver &&
               f.everyCombatParticipantHiddenFromObserver &&
               f.interactionSnapshotComplete &&
               !f.observerAlreadyTargetingOrInteracting &&
               !f.anyRealPlayerInvolvedWithTarget &&
               !f.questLootTapOrSkinRightsAtRisk &&
               !f.bossEscortSummonOrOtherSpecialTarget &&
               !f.visibilityOrCombatTransitionPending;
    }
}

#endif
