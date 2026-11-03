#include "src/IndividualProgressionF14CombatVisibilityPolicy.h"
#include <iostream>
namespace F14 = IndividualProgressionF14CombatVisibilityPolicy;
int main()
{
    F14::CreatureSnapshot c{};
    F14::ObserverSnapshot o{};
    c.complete = c.current = c.plainUnscriptedNonEliteHostileNpc = true;
    c.alive = c.inCombat = c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion = true;
    c.noRealPlayerCombatThreatTapOrRights = c.noPendingThreatOrCombatantChanges = true;
    c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = true;
    o.explicitlyEnabled = o.positivelyVerifiedNormalPlayer = o.hasVanillaEffectiveProgression = true;
    o.allParticipantsAlreadyHiddenFromThisObserver = true;
    o.noOwnTargetAuraPetTotemGuardianOrSpellInFlight = true;
    o.noObserverOrOwnedUnitThreatCombatOrPendingAssist = true;
    o.provenOutsideCreatureAggroAssistCleaveAndAoeReach = true;
    o.noObserverOrGroupQuestLootTapSkinOrKillCredit = true;
    o.observerStageAndIdentityStable = o.coreVisibilityAndThreatRefreshVerified = true;
    unsigned passed = 0;
    const auto check = [&](bool actual, bool expected) { if (actual != expected) return false; ++passed; return true; };
    if (!check(F14::ShouldConcealCreatureFromObserver(c, o), true)) return 1;
    c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = false;
    if (!check(F14::ShouldConcealCreatureFromObserver(c, o), false)) return 1;
    if (!check(c.inCombat, true)) return 1; // still fighting; immediately visible
    c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = true;
    c.alive = false;
    if (!check(F14::ShouldConcealCreatureFromObserver(c, o), false)) return 1;
    if (!check(F14::ConcealmentDecisionChanged(F14::CreatureSnapshot{
        true, true, true, true, true, true, true, true, true}, o, c, o), true)) return 1;
    std::cout << passed << " active-attack/corpse policy assertions passed\n";
}
