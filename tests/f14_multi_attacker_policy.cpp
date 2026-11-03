/*
 * Standalone F14 multi-attacker candidate policy regression.
 * Not a Worldserver, GameTest, true Playerbots identity or Core packet test.
 */
#include "src/IndividualProgressionF14MultiAttackerPolicy.h"
#include <iostream>
#include <limits>

namespace F = IndividualProgressionF14MultiAttackerPolicy;
static_assert(!F::HasVerifiedAttackerCandidates({}), "empty state always visible");
static_assert(F::MaxTrackedAttackers == 16, "bound must be intentionally reviewed");

int main()
{
    unsigned checks = 0;
    unsigned failures = 0;
    auto check = [&](char const* name, bool actual, bool expected) {
        ++checks;
        if (actual != expected)
        {
            ++failures;
            std::cerr << "FAIL " << name << ": actual=" << actual << " expected=" << expected << '\n';
        }
    };

    F::State state{0xB001ULL, 1, true, false, false, {}};
    check("initial roster empty", F::ActiveAttackers(state) == 0, true);
    check("initial unknown facts fail visible", F::HasVerifiedAttackerCandidates(state), false);

    auto a = F::OnEvent(state, F::Event::VerifiedAttackBegin, 1001);
    check("first begin invalidates", a.invalidateObserverCreatureProof, true);
    check("first begin immediate refresh request", a.requestImmediateVisibilityRefresh, true);
    check("first begin fail visible", a.failVisibleUntilReverified, true);
    check("first begin generation advances", a.state.generation == 2, true);
    check("first begin one attacker", F::ActiveAttackers(a.state) == 1, true);
    check("unverified begin not enough", F::HasVerifiedAttackerCandidates(a.state), false);
    auto aVerified = F::ReverifyCompleteParticipantFacts(a.state, 0xB001, 2, true);
    check("after full facts first attacker candidate ready", F::HasVerifiedAttackerCandidates(aVerified), true);
    check("original transition still unverified", F::HasVerifiedAttackerCandidates(a.state), false);
    check("stale target identity cannot verify", F::ReverifyCompleteParticipantFacts(a.state, 0xB002, 2, true).completeCurrentParticipantProof, false);
    check("stale generation cannot verify", F::ReverifyCompleteParticipantFacts(a.state, 0xB001, 1, true).completeCurrentParticipantProof, false);
    check("incomplete participant rights cannot verify", F::ReverifyCompleteParticipantFacts(a.state, 0xB001, 2, false).completeCurrentParticipantProof, false);

    auto b = F::OnEvent(aVerified, F::Event::VerifiedAttackBegin, 1002);
    check("second verified attacker counted", F::ActiveAttackers(b.state) == 2, true);
    check("second begin invalidates earlier proof", F::HasVerifiedAttackerCandidates(b.state), false);
    check("second begin new revision", b.state.generation == 3, true);
    auto both = F::ReverifyCompleteParticipantFacts(b.state, 0xB001, 3, true);
    check("complete both-bot snapshot can be candidate", F::HasVerifiedAttackerCandidates(both), true);

    auto duplicate = F::OnEvent(both, F::Event::VerifiedAttackBegin, 1002);
    check("duplicate does not double count", F::ActiveAttackers(duplicate.state) == 2, true);
    check("duplicate still invalidates old proof", F::HasVerifiedAttackerCandidates(duplicate.state), false);
    check("duplicate advances revision", duplicate.state.generation == 4, true);

    auto oneLeaves = F::OnEvent(both, F::Event::AttackEnd, 1001);
    check("one bot leaves, another still attacking", F::ActiveAttackers(oneLeaves.state) == 1, true);
    check("one bot leaves, old proof invalidated", F::HasVerifiedAttackerCandidates(oneLeaves.state), false);
    auto stillOne = F::ReverifyCompleteParticipantFacts(oneLeaves.state, 0xB001, 4, true);
    check("remaining bot can qualify AFTER full reverify", F::HasVerifiedAttackerCandidates(stillOne), true);
    auto noBots = F::OnEvent(stillOne, F::Event::AttackEnd, 1002);
    check("last bot stops, no attackers", F::ActiveAttackers(noBots.state) == 0, true);
    check("last bot stops, visible", F::HasVerifiedAttackerCandidates(noBots.state), false);
    check("empty attacker set cannot grant candidate", F::ReverifyCompleteParticipantFacts(noBots.state, 0xB001, 5, true).completeCurrentParticipantProof, false);

    auto unknownEnd = F::OnEvent(stillOne, F::Event::AttackEnd, 9999);
    check("unknown stop rejects incomplete event stream", unknownEnd.state.uncertain, true);
    check("unknown stop fails visible", F::HasVerifiedAttackerCandidates(unknownEnd.state), false);
    auto zeroBegin = F::OnEvent(stillOne, F::Event::VerifiedAttackBegin, 0);
    check("missing actor incarnation token rejected", zeroBegin.state.uncertain, true);
    check("missing actor token fails visible", F::HasVerifiedAttackerCandidates(zeroBegin.state), false);
    auto realHuman = F::OnEvent(stillOne, F::Event::UnverifiedOrHumanParticipant, 7001);
    check("human/unverified participant poisons snapshot", realHuman.state.uncertain, true);
    check("human participant fails visible", F::HasVerifiedAttackerCandidates(realHuman.state), false);
    check("old actor proof cannot clear uncertainty",
          F::ReverifyCompleteParticipantFacts(realHuman.state, 0xB001, 5, true).completeCurrentParticipantProof, false);
    auto changedRights = F::OnEvent(stillOne, F::Event::ThreatRightsOrIdentityChanged);
    check("loot rights change marks uncertainty", changedRights.state.uncertain, true);
    check("loot rights change revokes permission", F::HasVerifiedAttackerCandidates(changedRights.state), false);

    std::array<std::uint64_t, F::MaxTrackedAttackers> freshRoster{};
    freshRoster[0] = 1002;
    freshRoster[1] = 1003;
    auto staleRebuild = F::ReplaceWithFullyVerifiedRoster(realHuman.state, 0xB001, 4, freshRoster, 2, true);
    check("stale roster generation rejected", staleRebuild.state.uncertain, true);
    check("stale roster cannot conceal", F::HasVerifiedAttackerCandidates(staleRebuild.state), false);
    auto wrongCreature = F::ReplaceWithFullyVerifiedRoster(realHuman.state, 0xB002, 5, freshRoster, 2, true);
    check("other creature roster rejected", wrongCreature.state.uncertain, true);
    auto missingProof = F::ReplaceWithFullyVerifiedRoster(realHuman.state, 0xB001, 5, freshRoster, 2, false);
    check("partial rights proof cannot re-admit roster", missingProof.state.uncertain, true);
    freshRoster[1] = 1002;
    auto duplicateRoster = F::ReplaceWithFullyVerifiedRoster(realHuman.state, 0xB001, 5, freshRoster, 2, true);
    check("duplicate bot identities in full roster rejected", duplicateRoster.state.uncertain, true);
    freshRoster[1] = 0;
    auto zeroRoster = F::ReplaceWithFullyVerifiedRoster(realHuman.state, 0xB001, 5, freshRoster, 2, true);
    check("zero actor in claimed full roster rejected", zeroRoster.state.uncertain, true);
    freshRoster[1] = 1003;
    auto rebuilt = F::ReplaceWithFullyVerifiedRoster(realHuman.state, 0xB001, 5, freshRoster, 2, true);
    check("fresh complete roster clears uncertainty", rebuilt.state.uncertain, false);
    check("fresh roster now has two attackers", F::ActiveAttackers(rebuilt.state) == 2, true);
    check("fresh roster still needs separate rights/refresh proof", F::HasVerifiedAttackerCandidates(rebuilt.state), false);
    check("fresh roster advances identity revision", rebuilt.state.generation == 6, true);
    auto rechecked = F::ReverifyCompleteParticipantFacts(rebuilt.state, 0xB001, 6, true);
    check("full rebuilt facts allow active candidates", F::HasVerifiedAttackerCandidates(rechecked), true);
    check("bad count above bounded max fails visible",
          F::ReplaceWithFullyVerifiedRoster(rebuilt.state, 0xB001, 6, freshRoster, F::MaxTrackedAttackers + 1, true).state.uncertain, true);

    auto died = F::OnEvent(rechecked, F::Event::CreatureDied);
    check("death marks corpse not alive", died.state.alive, false);
    check("death clears all tracked active actors", F::ActiveAttackers(died.state) == 0, true);
    check("death removes candidate", F::HasVerifiedAttackerCandidates(died.state), false);
    check("old complete proof cannot re-hide corpse",
          F::ReverifyCompleteParticipantFacts(died.state, 0xB001, 7, true).completeCurrentParticipantProof, false);
    check("begin after death fails visible",
          F::HasVerifiedAttackerCandidates(F::OnEvent(died.state, F::Event::VerifiedAttackBegin, 1002).state), false);
    check("full roster cannot make corpse active",
          F::ReplaceWithFullyVerifiedRoster(died.state, 0xB001, 7, freshRoster, 2, true).state.uncertain, true);

    auto respawn = F::OnEvent(died.state, F::Event::CreatureRespawned, 0xB002);
    check("respawn accepts new living incarnation", respawn.state.alive, true);
    check("respawn fresh revision", respawn.state.generation == 1, true);
    check("respawn drops old attackers", F::ActiveAttackers(respawn.state) == 0, true);
    check("respawn does not inherit hidden state", F::HasVerifiedAttackerCandidates(respawn.state), false);
    auto sameIdentity = F::OnEvent(died.state, F::Event::CreatureRespawned, 0xB001);
    check("reused dead creature identity refused", sameIdentity.state.creatureInstanceToken == 0, true);
    check("reused dead creature identity stays visible", F::HasVerifiedAttackerCandidates(sameIdentity.state), false);
    check("missing respawn token refused",
          F::OnEvent(died.state, F::Event::CreatureRespawned).state.creatureInstanceToken == 0, true);

    auto overflow = rechecked;
    overflow.generation = std::numeric_limits<std::uint32_t>::max();
    auto invalidated = F::OnEvent(overflow, F::Event::AttackEnd, 1002);
    check("revision wrap invalidates old creature identity", invalidated.state.creatureInstanceToken == 0, true);
    check("revision wrap fails visible", F::HasVerifiedAttackerCandidates(invalidated.state), false);
    check("stale zero epoch cannot be repaired by actor event",
          F::OnEvent(invalidated.state, F::Event::VerifiedAttackBegin, 1002).state.creatureInstanceToken == 0, true);

    // One creature can be the OLD target of a switching attacker, while a
    // different creature independently gains the NEW target. Never transfer
    // creature identity or positive proof between these independent states.
    auto sourceCreature = F::OnEvent(rechecked, F::Event::AttackEnd, 1002);
    auto nextCreature = F::OnEvent(F::State{0xB003ULL, 1, true, false, false, {}},
                                   F::Event::VerifiedAttackBegin, 1002);
    check("target switch old creature has another attacker", F::ActiveAttackers(sourceCreature.state) == 1, true);
    check("target switch new creature has switched attacker", F::ActiveAttackers(nextCreature.state) == 1, true);
    check("target switch old creature invalid until proof", F::HasVerifiedAttackerCandidates(sourceCreature.state), false);
    check("target switch new creature invalid until proof", F::HasVerifiedAttackerCandidates(nextCreature.state), false);
    check("different creature identities are independent",
          sourceCreature.state.creatureInstanceToken != nextCreature.state.creatureInstanceToken, true);

    F::State full{0xB004ULL, 1, true, false, false, {}};
    for (unsigned i = 0; i < F::MaxTrackedAttackers; ++i)
        full = F::OnEvent(full, F::Event::VerifiedAttackBegin, 2000 + i).state;
    check("bounded roster holds exactly 16 verified actors", F::ActiveAttackers(full) == 16, true);
    check("bounded roster without proof remains visible", F::HasVerifiedAttackerCandidates(full), false);
    auto seventeenth = F::OnEvent(full, F::Event::VerifiedAttackBegin, 3000);
    check("17th attacker cannot silently be dropped", seventeenth.state.uncertain, true);
    check("17th attacker cannot grant concealment",
          F::HasVerifiedAttackerCandidates(F::ReverifyCompleteParticipantFacts(seventeenth.state, 0xB004, 18, true)), false);

    std::cout << "F14 MULTI-ATTACKER " << checks - failures << "/" << checks
              << (failures ? " FAILED" : " PASSED") << "; pure/offline model only\n";
    return failures ? 1 : 0;
}
