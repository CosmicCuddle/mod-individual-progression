/*
 * Portable policy-only tests. No Worldserver, Core hooks, or gameplay runs.
 * Explicit error counts continue to work even under -DNDEBUG.
 */
#include "src/IndividualProgressionF14CombatVisibilityPolicy.h"
#include <iostream>

namespace F14 = IndividualProgressionF14CombatVisibilityPolicy;

constexpr F14::CreatureSnapshot Creature()
{
    F14::CreatureSnapshot c{};
    c.complete = true;
    c.current = true;
    c.generation = 1; // Present, current creature state.
    c.plainUnscriptedNonEliteHostileNpc = true;
    c.alive = true;
    c.inCombat = true;
    c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = true;
    c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion = true;
    c.noRealPlayerCombatThreatTapOrRights = true;
    c.noPendingThreatOrCombatantChanges = true;
    return c;
}

constexpr F14::ObserverSnapshot Observer()
{
    F14::ObserverSnapshot o{};
    o.explicitlyEnabled = true;
    o.checkedAgainstCreatureGeneration = 1;
    o.positivelyVerifiedNormalPlayer = true;
    o.hasVanillaEffectiveProgression = true;
    o.allParticipantsAlreadyHiddenFromThisObserver = true;
    o.noOwnTargetAuraPetTotemGuardianOrSpellInFlight = true;
    o.noObserverOrOwnedUnitThreatCombatOrPendingAssist = true;
    o.provenOutsideCreatureAggroAssistCleaveAndAoeReach = true;
    o.noObserverOrGroupQuestLootTapSkinOrKillCredit = true;
    o.observerStageAndIdentityStable = true;
    o.coreVisibilityAndThreatRefreshVerified = true;
    return o;
}

static_assert(!F14::ShouldConcealCreatureFromObserver({}, {}),
              "unverified/default-disabled policy must not hide anything");
static_assert(F14::ShouldConcealCreatureFromObserver(Creature(), Observer()),
              "an isolated encounter could be concealed only after approved Core infrastructure");

int main()
{
    unsigned checks = 0;
    unsigned failures = 0;
    const auto check = [&](char const* name, bool actual, bool expected)
    {
        ++checks;
        if (actual != expected)
        {
            ++failures;
            std::cerr << "FAIL: " << name << '\n';
        }
    };
    const auto hide = [&](char const* name, F14::CreatureSnapshot c, F14::ObserverSnapshot o)
    {
        check(name, F14::ShouldConcealCreatureFromObserver(c, o), true);
    };
    const auto visible = [&](char const* name, F14::CreatureSnapshot c, F14::ObserverSnapshot o)
    {
        check(name, F14::ShouldConcealCreatureFromObserver(c, o), false);
    };

    const auto c0 = Creature();
    const auto o0 = Observer();
    hide("verified isolated bot-only fight", c0, o0);

    auto c = c0;
    auto o = o0;
    o.explicitlyEnabled = false; visible("default OFF", c, o);
    o = o0; o.positivelyVerifiedNormalPlayer = false; visible("GM, unknown or not player", c, o);
    o = o0; o.hasVanillaEffectiveProgression = false; visible("TBC or later viewer", c, o);
    o = o0; o.allParticipantsAlreadyHiddenFromThisObserver = false; visible("not all attackers hidden", c, o);
    o = o0; o.noOwnTargetAuraPetTotemGuardianOrSpellInFlight = false; visible("aura, pet, target, spell or missile", c, o);
    o = o0; o.noObserverOrOwnedUnitThreatCombatOrPendingAssist = false; visible("observer or pet threatened", c, o);
    o = o0; o.provenOutsideCreatureAggroAssistCleaveAndAoeReach = false; visible("could aggro, assist, cleave, AoE observer", c, o);
    o = o0; o.noObserverOrGroupQuestLootTapSkinOrKillCredit = false; visible("quest, loot, tap or group kill-credit", c, o);
    o = o0; o.observerStageAndIdentityStable = false; visible("tier or identity update pending", c, o);
    o = o0; o.coreVisibilityAndThreatRefreshVerified = false; visible("Core seam/refresh missing", c, o);

    c = c0; c.complete = false; visible("incomplete combat snapshot", c, o0);
    c = c0; c.current = false; visible("expired combat snapshot", c, o0);
    c = c0; c.plainUnscriptedNonEliteHostileNpc = false; visible("rare/boss/escort/pet/script/charm", c, o0);
    c = c0; c.alive = false; visible("corpse or dead creature", c, o0);
    c = c0; c.inCombat = false; visible("combat ended", c, o0);
    c = c0; c.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = false;
    visible("bot stops attacking even though creature remains in combat", c, o0);
    c = c0; c.alive = false;
    visible("death overrides a stale active-attacker flag", c, o0);
    c = c0; c.generation = 2;
    visible("stale viewer-target pair after combatant joins", c, o0);
    o = o0; o.checkedAgainstCreatureGeneration = 2;
    visible("stale viewer-target pair against original creature generation", c0, o);
    c = c0; c.generation = 0;
    o = o0; o.checkedAgainstCreatureGeneration = 0;
    visible("zero generation never authorizes concealment", c, o);
    c = c0; c.generation = 2;
    o = o0; o.checkedAgainstCreatureGeneration = 2;
    hide("fresh viewer-target pair matches new creature generation", c, o);
    c = c0; c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion = false; visible("human BE, Altbot or unknown attacker", c, o0);
    c = c0; c.noRealPlayerCombatThreatTapOrRights = false; visible("other real player has threat/tap rights", c, o0);
    c = c0; c.noPendingThreatOrCombatantChanges = false; visible("new victim or threat-add pending", c, o0);

    c = c0;
    o = o0;
    check("visibility refresh needed after combat end",
          F14::ConcealmentDecisionChanged(c0, o0, []{
              auto next = Creature(); next.inCombat = false; return next;
          }(), o0), true);
    c.alive = false;
    check("visibility refresh needed on death",
          F14::ConcealmentDecisionChanged(c0, o0, c, o0), true);
    c = c0; c.noPendingThreatOrCombatantChanges = false;
    check("visibility refresh needed at new threat add",
          F14::ConcealmentDecisionChanged(c0, o0, c, o0), true);
    c = c0; c.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion = false;
    check("visibility refresh needed on new unverified attacker",
          F14::ConcealmentDecisionChanged(c0, o0, c, o0), true);
    o.hasVanillaEffectiveProgression = false;
    check("visibility refresh needed after TBC unlock",
          F14::ConcealmentDecisionChanged(c0, o0, c0, o), true);
    o = o0; o.provenOutsideCreatureAggroAssistCleaveAndAoeReach = false;
    check("visibility refresh needed when observer enters danger range",
          F14::ConcealmentDecisionChanged(c0, o0, c0, o), true);
    check("no refresh suggested for unchanged facts",
          F14::ConcealmentDecisionChanged(c0, o0, c0, o0), false);

    std::cout << "F14 unwired policy model: " << (checks - failures) << "/" << checks
              << " checks passed (including NDEBUG). No gameplay effect.\n";
    return failures ? 1 : 0;
}
