/*
 * F14 actor-to-creature expiry fan-out, PURE/OFFLINE regression.
 * NO runtime map-thread, client visibility, Core build or game proof.
 */
#include "src/IndividualProgressionF14ActorCreatureIndex.h"
#include <iostream>

namespace F = IndividualProgressionF14MultiAttackerPolicy;
namespace I = IndividualProgressionF14ActorCreatureIndex;

int main()
{
    unsigned checks = 0, failures = 0;
    auto check = [&](char const* name, bool condition) {
        ++checks;
        if (!condition) { ++failures; std::cerr << "FAIL " << name << '\n'; }
    };
    auto make = [](std::uint64_t creature, std::uint64_t first,
                   std::uint64_t second = 0) {
        F::State s{};
        s.creatureInstanceToken = creature;
        s.generation = 1;
        s.alive = true;
        s.completeCurrentParticipantProof = true; // Model input, NOT real rights evidence.
        s.verifiedAttackers[0] = first;
        s.verifiedAttackers[1] = second;
        return s;
    };

    I::Index index;
    std::array<F::State, I::MaxKnownCreatures> snap{};
    snap[0] = make(1001, 101);
    snap[1] = make(1002, 101, 202);
    snap[2] = make(1003, 303);
    check("unknown index cannot conceal", !index.CanConsiderCreature(1001));
    check("unproved full snapshot is rejected",
          !index.ReplaceFromCompleteSnapshot(snap, 3, false));
    check("failed snapshot leaves index incomplete", !index.IsComplete());
    check("verified complete map snapshot accepted",
          index.ReplaceFromCompleteSnapshot(snap, 3, true));
    check("first creature has candidate", index.CanConsiderCreature(1001));
    check("second creature has candidate", index.CanConsiderCreature(1002));
    check("third independent creature has candidate", index.CanConsiderCreature(1003));
    check("unknown creature is never candidate", !index.CanConsiderCreature(9999));

    auto expired = index.ExpireLogin(101);
    check("expiry always invalidates entire map candidate cache", !index.IsComplete());
    check("expiry asks for global observer invalidation",
          expired.requireGlobalObserverInvalidation);
    check("expiry asks for full map thread rebuild",
          expired.requireCompleteMapThreadRebuild);
    check("shared actor was indexed", expired.actorWasIndexed);
    check("shared actor fans out to both creatures", expired.affectedCount == 2);
    check("fanout records first independent creature",
          expired.affectedCreatureTokens[0] == 1001);
    check("fanout records second independent creature",
          expired.affectedCreatureTokens[1] == 1002);
    check("even unrelated creature loses cached candidate until audit",
          !index.CanConsiderCreature(1003));
    check("expiry cannot itself authorise new hiding",
          !index.CanConsiderCreature(1002));

    snap[0] = make(1001, 0);
    snap[1] = make(1002, 202);
    snap[2] = make(1003, 303);
    check("fresh map snapshot after expiry succeeds",
          index.ReplaceFromCompleteSnapshot(snap, 3, true));
    check("expired-only creature remains visible", !index.CanConsiderCreature(1001));
    check("other bot can qualify after complete fresh proof",
          index.CanConsiderCreature(1002));
    check("unrelated creature restored after full audit", index.CanConsiderCreature(1003));
    snap[1] = make(1002, 101, 202);
    check("expired login epoch cannot be reintroduced",
          !index.ReplaceFromCompleteSnapshot(snap, 3, true));
    check("rejected stale snapshot fails visible for all",
          !index.CanConsiderCreature(1003));

    snap[1] = make(1002, 404, 202); // Different login epoch, never the old token.
    check("new login epoch may be indexed after verified rebuild",
          index.ReplaceFromCompleteSnapshot(snap, 3, true));
    check("new login can qualify after fresh proof", index.CanConsiderCreature(1002));
    auto repeated = index.ExpireLogin(101);
    check("duplicate expiry does not re-add stale actor", !repeated.actorWasIndexed);
    check("duplicate expiry still invalidates the global proof", !index.IsComplete());
    check("duplicate expiry is not a poisonous overflow", !index.IsPoisoned());
    check("rebuild excluding old login remains possible",
          index.ReplaceFromCompleteSnapshot(snap, 3, true));

    auto unknown = index.ExpireLogin(999);
    check("unindexed login expiry causes full invalidation",
          !unknown.actorWasIndexed && unknown.affectedCount == 0 && !index.IsComplete());
    check("unknown identity cannot conceal an unrelated creature",
          !index.CanConsiderCreature(1003));
    check("rebuild with fresh complete map facts can recover",
          index.ReplaceFromCompleteSnapshot(snap, 3, true));

    check("creature removal invalidates map candidates",
          index.RemoveCreature(1001) && !index.IsComplete());
    check("removed creature stays ineligible", !index.CanConsiderCreature(1001));
    snap[0] = make(2001, 0); // New creature incarnation, not a reused spawn identity.
    check("verified new incarnation restores safe unrelated candidates",
          index.ReplaceFromCompleteSnapshot(snap, 3, true));
    check("new incarnation does not inherit an old attacker",
          !index.CanConsiderCreature(2001));
    check("old incarnation is absent", !index.CanConsiderCreature(1001));

    snap[1] = snap[0];
    check("duplicate creature incarnations cannot create a complete index",
          !index.ReplaceFromCompleteSnapshot(snap, 3, true));
    snap[1] = make(1002, 404, 202);
    snap[1].verifiedAttackers[2] = 404;
    check("duplicate attacker in one creature is rejected",
          !index.ReplaceFromCompleteSnapshot(snap, 3, true));
    snap[1].verifiedAttackers[2] = 0;
    check("over-capacity snapshot cannot be silently truncated",
          !index.ReplaceFromCompleteSnapshot(snap, I::MaxKnownCreatures + 1, true));

    std::array<F::State, I::MaxKnownCreatures> full{};
    for (std::size_t i = 0; i < I::MaxKnownCreatures; ++i)
        full[i] = make(5000 + i, 777);
    I::Index bounded;
    check("exactly bounded creature count is accepted",
          bounded.ReplaceFromCompleteSnapshot(full, I::MaxKnownCreatures, true));
    auto fanout = bounded.ExpireLogin(777);
    check("expiry reaches all maximum bounded creatures",
          fanout.affectedCount == I::MaxKnownCreatures);
    check("bounded expiry never retains hide eligibility",
          !bounded.CanConsiderCreature(5000) && !bounded.CanConsiderCreature(5031));

    I::Index exhausted;
    check("empty audited map can be complete",
          exhausted.ReplaceFromCompleteSnapshot({}, 0, true));
    for (std::uint64_t token = 1; token <= I::MaxExpiredLogins; ++token)
        exhausted.ExpireLogin(token);
    check("64 expired login epochs fit bounded tombstones", !exhausted.IsPoisoned());
    auto over = exhausted.ExpireLogin(10000);
    check("65th expiry permanently disables model",
          over.registryPermanentlyDisabled && exhausted.IsPoisoned());
    check("exhausted registry cannot be reopened by a claimed audit",
          !exhausted.ReplaceFromCompleteSnapshot(snap, 3, true));

    I::Index invalid;
    auto zero = invalid.ExpireLogin(0);
    check("missing epoch poisons index rather than hiding anything",
          zero.registryPermanentlyDisabled && invalid.IsPoisoned());
    check("missing epoch cannot be repaired by a stale snapshot",
          !invalid.ReplaceFromCompleteSnapshot(snap, 3, true));

    std::cout << "F14 ACTOR INDEX " << checks - failures << "/" << checks
              << (failures ? " FAILED" : " PASSED")
              << "; offline/fail-visible model only\n";
    return failures ? 1 : 0;
}
