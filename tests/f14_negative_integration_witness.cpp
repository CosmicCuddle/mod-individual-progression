// Synthetic negative composition only. No Core pass producer or visibility hook.
#include "IndividualProgressionF14CurrentPassIdentity.h"
#include "IndividualProgressionF14SendAttemptCorrelation.h"
#include "IndividualProgressionF14ViewerRestoreProtocol.h"
#include <cstdlib>
#include <iostream>

namespace Pass = IndividualProgressionF14CurrentPassIdentity;
namespace Corr = IndividualProgressionF14SendAttemptCorrelation;
namespace Restore = IndividualProgressionF14ViewerRestoreProtocol;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)
static_assert(!Corr::HasCorePassTokenProducer(), "synthetic tokens are not a Core producer");
static_assert(!Corr::IsSendCallClientPresentation(), "send call is not client restore");
static_assert(Restore::ClientStageOf() == Restore::ClientStage::NotServerObservable,
              "client presentation is not server observable");

using PassTable = Pass::Tracker<2, 4>;
using CorrTable = Corr::Table<3, 2>;
using Protocol = Restore::Protocol<3>;

struct Witness
{
    PassTable& pass;
    CorrTable& correlation;
    Protocol& restore;
    Pass::ViewerIdentity current{};

    // An exact old-session revoke may drop its ephemeral evidence. It always
    // routes the raw GUID to the restoration protocol, across all epochs.
    void RevokeExact(Pass::ViewerIdentity old)
    {
        pass.RevokeViewer(old);
        restore.ObserveViewerSessionEnded(old.guid);
        if (current == old)
        {
            correlation.ObserveViewerSessionEnded(old.guid);
            current = {};
        }
    }

    Corr::Correlation TrySend(Pass::ViewerIdentity identity, std::uint64_t token,
                              Corr::SendFacts facts, std::uint64_t currentIncarnation)
    {
        Corr::Correlation refused;
        if (!(current == identity) || !pass.IsActive(token, identity) ||
            !pass.BatchWasObserved(token, identity) ||
            identity.loginEpoch != facts.currentViewerEpoch ||
            identity.map.mapId != facts.eventMapId ||
            identity.map.instanceId != facts.eventInstanceId)
            return refused;
        facts.globalRestoreRequired = restore.GlobalRestoreRequired();
        Corr::Correlation match = correlation.Correlate(facts);
        if (match.Matched() && match.incarnation != currentIncarnation)
            match.result = Corr::Result::RecycledIncarnation;
        return match;
    }
};

static Restore::DecisionFacts PriorFacts(Restore::ViewerKey viewer,
                                         Restore::CreatureKey creature,
                                         std::uint64_t incarnation)
{
    Restore::DecisionFacts facts;
    facts.configEnabled = facts.actorPositivelyVerifiedRandomBot = true;
    facts.actorEntitled = facts.pairRightsProven = facts.creatureAlive = true;
    facts.currentViewerEpoch = viewer.loginEpoch;
    facts.currentCreatureIncarnation = incarnation;
    facts.eventMapId = creature.mapId;
    facts.eventInstanceId = creature.instanceId;
    return facts;
}

// Fixture seed: represents an already recorded historical F14 server hide.
// The witness itself has no positive suppression or concealment method.
static bool SeedPriorHidden(Protocol& protocol, Restore::ViewerKey viewer,
                            Restore::CreatureKey creature, std::uint64_t incarnation)
{
    return protocol.ObserveF14SuppressionDecision(
        viewer, creature, incarnation, true, Restore::VisibilityPath::Direct,
        PriorFacts(viewer, creature, incarnation)) == Restore::Outcome::SuppressionRecorded;
}

static Corr::NominationFacts Nomination(Protocol const& protocol,
                                       Restore::ViewerKey viewer,
                                       Restore::CreatureKey creature,
                                       std::uint64_t incarnation)
{
    Corr::NominationFacts facts;
    facts.currentViewerEpoch = viewer.loginEpoch;
    facts.currentCreatureIncarnation = incarnation;
    facts.eventMapId = creature.mapId;
    facts.eventInstanceId = creature.instanceId;
    Corr::ReadProtocolFacts(protocol, viewer, creature, incarnation, facts);
    return facts;
}

static Corr::SendFacts Send(Restore::ViewerKey viewer, Restore::CreatureKey creature,
                            std::uint64_t token)
{
    Corr::SendFacts facts;
    facts.sourceAttributedByExactCoreF14Hook = true;
    facts.viewerGuidRaw = viewer.guidRaw;
    facts.currentViewerEpoch = viewer.loginEpoch;
    facts.creatureGuidRaw = creature.guidRaw;
    facts.eventMapId = creature.mapId;
    facts.eventInstanceId = creature.instanceId;
    facts.direction = Corr::Direction::Create;
    facts.passToken = token;
    return facts;
}

int main()
{
    Pass::ViewerIdentity const old{100, 10, 1, {1, 2}};
    Pass::ViewerIdentity const next{100, 11, 2, {1, 2}};
    Pass::ViewerIdentity const other{200, 20, 1, {1, 2}};
    Restore::ViewerKey const oldKey{100, 10}, nextKey{100, 11}, otherKey{200, 20};
    Restore::CreatureKey const creature{1, 2, 901};
    Restore::CreatureKey const otherCreature{1, 2, 902};

    // Pre-existing synthetic obligations for two login epochs of the same
    // raw GUID and an unrelated viewer. No fixture executes client restore.
    Protocol protocol;
    CHECK(SeedPriorHidden(protocol, oldKey, creature, 9001));
    CHECK(SeedPriorHidden(protocol, nextKey, otherCreature, 9002));
    CHECK(SeedPriorHidden(protocol, otherKey, creature, 9003));
    CHECK(protocol.LedgerView().PendingRestores() == 0);
    PassTable passes;
    CorrTable correlations;
    Witness witness{passes, correlations, protocol, old};

    CHECK(passes.Begin(50, old) == Pass::Result::Recorded);
    CHECK(passes.ObserveBatch(50, old) == Pass::Result::BatchObserved);
    witness.RevokeExact(old);
    CHECK(protocol.NeedsRestore(oldKey, creature));
    CHECK(protocol.NeedsRestore(nextKey, otherCreature)); // raw GUID, all epochs
    CHECK(!protocol.NeedsRestore(otherKey, creature));
    CHECK(protocol.LedgerView().PendingRestores() == 2);

    PassTable missingIdentity;
    CHECK(missingIdentity.Begin(90, {}) == Pass::Result::Refused);
    CHECK(missingIdentity.Begin(90, old) == Pass::Result::Refused);
    CHECK(!missingIdentity.IsActive(90, old));
    CHECK(passes.Begin(60, next) == Pass::Result::Recorded);
    CHECK(passes.ObserveBatch(60, next) == Pass::Result::BatchObserved);
    witness.current = next;
    CHECK(correlations.Nominate(nextKey, otherCreature, 9002, Corr::Direction::Create,
                                60, Nomination(protocol, nextKey, otherCreature, 9002)) ==
          Corr::Result::Nominated);
    witness.RevokeExact(old); // delayed old-session callback
    CHECK(passes.IsActive(60, next) && correlations.Candidates() == 1);
    CHECK(witness.TrySend(next, 60, Send(nextKey, otherCreature, 60), 9002).Matched());
    CHECK(protocol.LedgerView().PendingRestores() == 2); // match never clears debt
    CHECK(!witness.TrySend(old, 50, Send(oldKey, creature, 50), 9001).Matched());

    // Replayed and nested passes cannot upgrade even a nominated send.
    CHECK(correlations.Nominate(nextKey, otherCreature, 9002, Corr::Direction::Create,
                                60, Nomination(protocol, nextKey, otherCreature, 9002)) ==
          Corr::Result::Nominated);
    CHECK(passes.Begin(61, next) == Pass::Result::Mismatch);
    CHECK(!witness.TrySend(next, 60, Send(nextKey, otherCreature, 60), 9002).Matched());
    CHECK(!witness.TrySend(next, 61, Send(nextKey, otherCreature, 61), 9002).Matched());
    CHECK(passes.End(60, next) == Pass::Result::Quarantined);
    CHECK(correlations.ObservePassEnded(nextKey, 60) == Corr::Result::PassAccepted);
    CHECK(passes.Begin(60, next) == Pass::Result::Refused);
    CHECK(!witness.TrySend(next, 60, Send(nextKey, otherCreature, 60), 9002).Matched());
    CHECK(protocol.LedgerView().PendingRestores() == 2);

    // A different viewer survives local pass quarantine. Incarnation is
    // checked at nomination AND at the composed send read boundary.
    CHECK(passes.Begin(70, other) == Pass::Result::Recorded);
    CHECK(passes.ObserveBatch(70, other) == Pass::Result::BatchObserved);
    witness.current = other;
    protocol.ObserveViewerSessionEnded(other.guid);
    CHECK(correlations.Nominate(otherKey, creature, 9003, Corr::Direction::Create,
                                70, Nomination(protocol, otherKey, creature, 9003)) ==
          Corr::Result::Nominated);
    CHECK(!witness.TrySend(other, 70, Send(otherKey, creature, 70), 9999).Matched());
    CHECK(protocol.NeedsRestore(otherKey, creature));
    CHECK(correlations.Nominate(otherKey, creature, 9999, Corr::Direction::Create,
                                71, Nomination(protocol, otherKey, creature, 9003)) ==
          Corr::Result::RecycledIncarnation);
    CHECK(passes.IsActive(70, other));

    // E1 candidate capacity poisons only correlation; no ledger obligation
    // becomes complete or globally cleared when ephemeral capacity fails.
    Corr::Table<1, 2> tinyCorrelation;
    CHECK(tinyCorrelation.Nominate(nextKey, otherCreature, 9002, Corr::Direction::Create,
                                   80, Nomination(protocol, nextKey, otherCreature, 9002)) ==
          Corr::Result::Nominated);
    CHECK(tinyCorrelation.Nominate(otherKey, creature, 9003, Corr::Direction::Create,
                                   81, Nomination(protocol, otherKey, creature, 9003)) ==
          Corr::Result::Capacity);
    CHECK(tinyCorrelation.IsPoisoned());
    CHECK(protocol.LedgerView().PendingRestores() == 3);

    // Pass capacity/quarantine never completes debt or unfreezes recovery.
    protocol.ObserveConfigDisabled();
    CHECK(protocol.GlobalRestoreRequired());
    CHECK(protocol.LedgerView().PendingRestores() == 3);
    CHECK(passes.End(70, other) == Pass::Result::Ended);
    CHECK(passes.Inspect().admissionsClosed);
    CHECK(passes.Begin(80, other) == Pass::Result::Refused);
    CHECK(protocol.GlobalRestoreRequired());
    CHECK(protocol.LedgerView().PendingRestores() == 3);
    std::cout << "PASS F14 negative composition: pass/correlation loss retains restore debt\n";
    return EXIT_SUCCESS;
}
