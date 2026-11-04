/* Portable F14 policy regression, not a game or Core test. */
#include "src/IndividualProgressionF14RefreshPolicy.h"
#include <iostream>
#include <limits>

namespace F = IndividualProgressionF14RefreshPolicy;

static_assert(!F::MayUseConcealmentProof({}, 1, 1), "unknown proof is always visible");

int main()
{
    unsigned checks = 0;
    unsigned failures = 0;
    auto test = [&](char const* name, bool actual, bool expected) {
        ++checks;
        if (actual != expected) { ++failures; std::cerr << "FAIL " << name << '\n'; }
    };
    F::State current{0x1501ULL, 1, true, false, false, false};
    test("initially visible", F::MayUseConcealmentProof(current, 0x1501, 1), false);
    auto started = F::OnEvent(current, F::Event::AttackBegan);
    test("attack begin invalidates cache", started.invalidatePairCache, true);
    test("attack begin requests refresh", started.requestImmediateVisibilityRefresh, true);
    test("attack begin pending is visible", started.forceVisibleDuringRefresh, true);
    test("attack begin no auto-hide", F::MayUseConcealmentProof(started.state, 0x1501, 2), false);
    auto facts = F::CommitVerifiedFacts(started.state, 0x1501, 2, true);
    test("verified facts alone never hide", F::MayUseConcealmentProof(facts, 0x1501, 2), false);
    test("wrong identity cannot commit", F::CommitVerifiedFacts(started.state, 0x999, 2, true).verifiedFactsCurrent, false);
    test("wrong generation cannot commit", F::CommitVerifiedFacts(started.state, 0x1501, 1, true).verifiedFactsCurrent, false);
    test("incomplete proof cannot commit", F::CommitVerifiedFacts(started.state, 0x1501, 2, false).verifiedFactsCurrent, false);
    test("stale refresh does not enable hide", F::AcknowledgeObserverRefresh(facts, 0x1501, 1).observerRefreshCompleted, false);
    auto ready = F::AcknowledgeObserverRefresh(facts, 0x1501, 2);
    test("refresh ack with matching token and generation", F::MayUseConcealmentProof(ready, 0x1501, 2), true);
    test("another creature identity never inherits visibility", F::MayUseConcealmentProof(ready, 0x1502, 2), false);
    test("other revision never inherits visibility", F::MayUseConcealmentProof(ready, 0x1501, 3), false);
    test("later incomplete proof revokes ready result",
         F::MayUseConcealmentProof(F::CommitVerifiedFacts(ready, 0x1501, 2, false), 0x1501, 2), false);
    test("late wrong-generation ack revokes ready result",
         F::MayUseConcealmentProof(F::AcknowledgeObserverRefresh(ready, 0x1501, 1), 0x1501, 2), false);
    for (auto e : {F::Event::AttackStopped, F::Event::AttackTargetChanged,
                   F::Event::CreatureDied, F::Event::CreatureDespawned,
                   F::Event::ThreatOrRightsChanged,
                   F::Event::ObserverEraOrIdentityChanged,
                   F::Event::BotLoginOrLogout,
                   F::Event::MapOrTeleportChanged, F::Event::ConfigReloaded})
    {
        auto next = F::OnEvent(ready, e);
        test("every event invalidates formerly hidden state", F::MayUseConcealmentProof(next.state, 0x1501, 3), false);
        test("every event requires immediate refresh", next.requestImmediateVisibilityRefresh, true);
        test("every event fails visible pending refresh", next.forceVisibleDuringRefresh, true);
    }
    auto dead = F::OnEvent(ready, F::Event::CreatureDied);
    test("death explicitly marks corpse", dead.state.creatureAlive, false);
    test("death drops active attacker", dead.state.eligibleHiddenAttackerActive, false);
    test("old positive proof cannot resurrect corpse", F::CommitVerifiedFacts(dead.state, 0x1501, 3, true).verifiedFactsCurrent, false);
    test("old refresh acknowledgement cannot re-hide corpse", F::AcknowledgeObserverRefresh(dead.state, 0x1501, 3).observerRefreshCompleted, false);
    auto switched = F::OnEvent(ready, F::Event::AttackTargetChanged);
    test("target switch clears active attacker even if creature in combat", switched.state.eligibleHiddenAttackerActive, false);
    auto fresh = F::OnEvent(dead.state, F::Event::CreatureRespawned, 0x1502);
    test("respawn gets new identity", fresh.state.creatureIdentityToken == 0x1502, true);
    test("respawn starts new generation", fresh.state.combatGeneration == 1, true);
    test("respawn does not inherit attacker", fresh.state.eligibleHiddenAttackerActive, false);
    test("respawn initially visible", F::MayUseConcealmentProof(fresh.state, 0x1502, 1), false);
    auto invalid = F::OnEvent(dead.state, F::Event::CreatureRespawned, 0x1501);
    test("reused spawn identity rejected", invalid.state.creatureIdentityToken, 0);
    test("reused spawn generation rejected", invalid.state.combatGeneration, 0);
    auto overflow = ready;
    overflow.combatGeneration = std::numeric_limits<std::uint32_t>::max();
    auto overflowed = F::OnEvent(overflow, F::Event::ThreatOrRightsChanged);
    test("generation overflow invalidates identity", overflowed.state.creatureIdentityToken, 0);
    test("generation overflow invalidates revision", overflowed.state.combatGeneration, 0);
    test("generation overflow is visible", F::MayUseConcealmentProof(overflowed.state, 0x1501, 1), false);
    std::cout << "F14 REFRESH pure policy " << checks - failures << "/" << checks << " PASSED; no runtime hookup.\n";
    return failures ? 1 : 0;
}
