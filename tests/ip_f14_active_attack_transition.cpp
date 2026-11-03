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
    c.generation = 1;
    c.creatureIdentityToken = 0x1001ULL;
    o.explicitlyEnabled = o.positivelyVerifiedNormalPlayer = o.hasVanillaEffectiveProgression = true;
    o.allParticipantsAlreadyHiddenFromThisObserver = true;
    o.noOwnTargetAuraPetTotemGuardianOrSpellInFlight = true;
    o.noObserverOrOwnedUnitThreatCombatOrPendingAssist = true;
    o.provenOutsideCreatureAggroAssistCleaveAndAoeReach = true;
    o.noObserverOrGroupQuestLootTapSkinOrKillCredit = true;
    o.observerStageAndIdentityStable = o.coreVisibilityAndThreatRefreshVerified = true;
    o.checkedAgainstCreatureGeneration = 1;
    o.checkedAgainstCreatureIdentityToken = 0x1001ULL;
    o.targetIsAliveAtThisEvaluation = true;
    o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = true;
    const auto before = c;
    unsigned passed = 0;
    const auto check = [&](bool actual, bool expected) { if (actual != expected) return false; ++passed; return true; };
    if (!check(F14::ShouldConcealCreatureFromObserver(c, o), true)) return 1;
    // Stale cached facts still claim the bot is attacking: live check wins.
    o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = false;
    if (!check(F14::ShouldConcealCreatureFromObserver(c, o), false)) return 1;
    // Cross-target reuse of viewer facts must fail even if both creatures
    // happen to report generation 1 and a qualifying bot is attacking.
    auto secondCreature = before;
    secondCreature.creatureIdentityToken = 0x2002ULL;
    o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = true;
    if (!check(F14::ShouldConcealCreatureFromObserver(secondCreature, o), false)) return 1;
    o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = false;
    o.eligibleHiddenAttackerStillAttackingThisTargetAtThisEvaluation = true;
    c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = false;
    if (!check(F14::ShouldConcealCreatureFromObserver(c, o), false)) return 1;
    if (!check(c.inCombat, true)) return 1; // still fighting; immediately visible
    c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = true;
    // Death must override even a cached object still reporting alive=true.
    o.targetIsAliveAtThisEvaluation = false;
    if (!check(F14::ShouldConcealCreatureFromObserver(before, o), false)) return 1;
    o.targetIsAliveAtThisEvaluation = true;
    c.alive = false;
    if (!check(F14::ShouldConcealCreatureFromObserver(c, o), false)) return 1;
    if (!check(F14::ConcealmentDecisionChanged(before, o, c, o), true)) return 1;
    std::cout << passed << " active-attack/corpse policy assertions passed\n";
}
