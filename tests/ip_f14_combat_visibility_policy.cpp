/* Development-only portable regression model. Does not link a Worldserver. */
#include "src/IndividualProgressionF14CombatVisibilityPolicy.h"
#include <cassert>
#include <iostream>

namespace F14 = IndividualProgressionF14CombatVisibilityPolicy;

constexpr F14::Facts Eligible()
{
    F14::Facts f{};
    f.explicitlyEnabled = true;
    f.observerIsVanilla = true;
    f.targetIsHostileCreature = true;
    f.targetIsAlive = true;
    f.targetInCombat = true;
    f.tbcRaceAttackerVerifiedRandomBot = true;
    f.tbcRaceAttackerActuallyHiddenFromObserver = true;
    f.everyCombatParticipantHiddenFromObserver = true;
    f.interactionSnapshotComplete = true;
    return f;
}

constexpr bool DefaultOFF()
{
    F14::Facts f = Eligible();
    f.explicitlyEnabled = false;
    return !F14::ShouldConcealCreatureFromObserver(f);
}
static_assert(DefaultOFF(), "F14 combat visibility must default to stock");
static_assert(F14::ShouldConcealCreatureFromObserver(Eligible()),
              "a safely isolated hidden RandomBot encounter can be concealed");

int main()
{
    unsigned cases = 0;
    auto const expectVisible = [&cases](F14::Facts f)
    {
        assert(!F14::ShouldConcealCreatureFromObserver(f));
        ++cases;
    };
    auto const expectConcealed = [&cases](F14::Facts f)
    {
        assert(F14::ShouldConcealCreatureFromObserver(f));
        ++cases;
    };

    expectConcealed(Eligible()); // Positive case, after verified F14 bot hiding.
    F14::Facts f = Eligible(); f.explicitlyEnabled = false; expectVisible(f);
    f = Eligible(); f.observerIsVanilla = false; expectVisible(f); // TBC+.
    f = Eligible(); f.observerIsGameMaster = true; expectVisible(f); // GM fallback.
    f = Eligible(); f.targetIsHostileCreature = false; expectVisible(f);
    f = Eligible(); f.targetIsAlive = false; expectVisible(f); // Corpse.
    f = Eligible(); f.targetInCombat = false; expectVisible(f); // Combat ended.
    f = Eligible(); f.tbcRaceAttackerVerifiedRandomBot = false; expectVisible(f); // Human / Altbot / unknown.
    f = Eligible(); f.tbcRaceAttackerActuallyHiddenFromObserver = false; expectVisible(f); // No race-only hiding.
    f = Eligible(); f.everyCombatParticipantHiddenFromObserver = false; expectVisible(f); // Mixed combat.
    f = Eligible(); f.interactionSnapshotComplete = false; expectVisible(f); // Unknown state.
    f = Eligible(); f.observerAlreadyTargetingOrInteracting = true; expectVisible(f);
    f = Eligible(); f.anyRealPlayerInvolvedWithTarget = true; expectVisible(f);
    f = Eligible(); f.questLootTapOrSkinRightsAtRisk = true; expectVisible(f);
    f = Eligible(); f.bossEscortSummonOrOtherSpecialTarget = true; expectVisible(f);
    f = Eligible(); f.visibilityOrCombatTransitionPending = true; expectVisible(f);

    // A creature must reappear once the hidden bot ends combat or disappears.
    f = Eligible(); expectConcealed(f);
    f.targetInCombat = false; expectVisible(f);
    f = Eligible(); f.tbcRaceAttackerVerifiedRandomBot = false; expectVisible(f);

    std::cout << "F14 combat visibility: " << cases << "/" << cases
              << " pure-model decisions passed (no runtime hooks).\n";
}
