// PURE integrated #117 snapshot -> #116 one-pair recheck; no Core dispatch.
// Synthetic F14-positive inputs exist only inside this test process.
#include "IndividualProgressionF14ViewerRestoreProtocol.h"
#include "IndividualProgressionF14ExactViewerRestoreRecheckPlan.h"
#include <array>
#include <cstdint>
#include <iostream>

namespace P = IndividualProgressionF14ViewerRestoreProtocol;
namespace R = IndividualProgressionF14ExactViewerRestoreRecheckPlan;
#define CHECK(x) do { if (!(x)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #x << '\n'; \
    return 1; \
} } while (false)

static P::DecisionFacts SyntheticProof(P::ViewerKey viewer, P::CreatureKey creature, std::uint64_t inc)
{
    P::DecisionFacts d;
    d.configEnabled = true;
    d.actorPositivelyVerifiedRandomBot = true;
    d.actorEntitled = true;
    d.pairRightsProven = true;
    d.creatureAlive = true;
    d.currentViewerEpoch = viewer.loginEpoch;
    d.currentCreatureIncarnation = inc;
    d.eventMapId = creature.mapId;
    d.eventInstanceId = creature.instanceId;
    return d;
}

static R::Facts Now(P::ViewerKey viewer, P::CreatureKey creature, std::uint64_t inc)
{
    R::Facts f;
    f.normalAccountHumanNonHeadlessSession = true;
    f.viewerInWorld = true;
    f.creatureInWorld = true;
    f.currentOwningMapThreadProven = true; // SYNTHETIC test fact, not proven owner
    f.currentViewerGuidRaw = viewer.guidRaw;
    f.currentViewerLoginEpoch = viewer.loginEpoch;
    f.currentCreatureGuidRaw = creature.guidRaw;
    f.currentCreatureIncarnation = inc;
    f.viewerMapId = creature.mapId;
    f.viewerInstanceId = creature.instanceId;
    f.creatureMapId = creature.mapId;
    f.creatureInstanceId = creature.instanceId;
    f.currentEventMapId = creature.mapId;
    f.currentEventInstanceId = creature.instanceId;
    f.viewerStillVanilla = false;  // historical restore survives era change
    f.featureStillEnabled = false; // historical restore survives F14 OFF
    return f;
}

int main()
{
    static_assert(!R::HasClientAcknowledgement(), "No fabricated client ACK");
    static_assert(!R::MayClearOriginalRestorationDebt(), "No historic debt clear");
    static_assert(!R::EnablesPositiveHide(), "No live hide");
    using Proto = P::Protocol<5>;
    using Record = Proto::ExactPendingRestore;
    Proto p;
    P::ViewerKey const a{10001, 501}, b{10002, 601}, c{10003, 701};
    P::CreatureKey const npc{1, 8, 9009}, other{1, 8, 9010};
    std::uint64_t const inc = 87;

    CHECK(p.ObserveF14SuppressionDecision(a, npc, inc, true, P::VisibilityPath::Direct,
          SyntheticProof(a, npc, inc)) == P::Outcome::SuppressionRecorded);
    CHECK(p.ObserveF14SuppressionDecision(b, npc, inc, false, P::VisibilityPath::Batched,
          SyntheticProof(b, npc, inc)) == P::Outcome::SuppressionRecorded);
    CHECK(p.ObserveF14SuppressionDecision(c, other, inc, true, P::VisibilityPath::Direct,
          SyntheticProof(c, other, inc)) == P::Outcome::SuppressionRecorded);

    p.ObserveCreatureRetired(npc); // ONLY two exact owed, third not yet revoked
    std::array<Record, 1> tooSmall{};
    tooSmall[0].viewer = {999, 999};
    std::size_t count = 999, required = 0;
    CHECK(p.CopyExactPendingRestores(tooSmall, count, required) ==
          Proto::SnapshotResult::OutputTooSmallNoCopy);
    CHECK(count == 0 && required == 2);
    CHECK(tooSmall[0].viewer.guidRaw == 999);
    // Failure MUST prohibit using a truncated first pair as all restored.
    CHECK(p.LedgerView().PendingRestores() == 2);

    std::array<Record, 5> entries{};
    CHECK(p.CopyExactPendingRestores(entries, count, required) ==
          Proto::SnapshotResult::KnownPairsCopied);
    CHECK(count == 2 && required == 2);
    CHECK(entries[0].viewer == a && entries[0].creature == npc);
    CHECK(entries[1].viewer == b && entries[1].creature == npc);
    CHECK(R::SelectNormalRecheck(p, entries[0].viewer, entries[0].creature,
          entries[0].incarnation, Now(a, npc, inc)) ==
          R::Result::RequestNormalCoreVisibilityPass);
    CHECK(R::SelectNormalRecheck(p, entries[1].viewer, entries[1].creature,
          entries[1].incarnation, Now(a, npc, inc)) ==
          R::Result::NoCurrentRealHumanSession);
    R::Facts headless = Now(b, npc, inc);
    headless.normalAccountHumanNonHeadlessSession = false;
    CHECK(R::SelectNormalRecheck(p, entries[1].viewer, entries[1].creature,
          entries[1].incarnation, headless) ==
          R::Result::NoCurrentRealHumanSession);
    R::Facts unknownMapOwner = Now(b, npc, inc);
    unknownMapOwner.currentOwningMapThreadProven = false;
    CHECK(R::SelectNormalRecheck(p, entries[1].viewer, entries[1].creature,
          entries[1].incarnation, unknownMapOwner) ==
          R::Result::MissingTrustedOwningMapContext);
    R::Facts oldEpoch = Now(b, npc, inc);
    oldEpoch.currentViewerLoginEpoch += 1;
    CHECK(R::SelectNormalRecheck(p, entries[1].viewer, entries[1].creature,
          entries[1].incarnation, oldEpoch) ==
          R::Result::StaleViewerLoginEpoch);
    R::Facts wrongInstance = Now(b, npc, inc);
    wrongInstance.currentEventInstanceId += 1;
    CHECK(R::SelectNormalRecheck(p, entries[1].viewer, entries[1].creature,
          entries[1].incarnation, wrongInstance) ==
          R::Result::DifferentMapOrInstance);

    P::TransitionFacts t;
    t.currentViewerEpoch = a.loginEpoch;
    t.currentCreatureIncarnation = inc;
    t.eventMapId = npc.mapId;
    t.eventInstanceId = npc.instanceId;
    CHECK(p.ObserveLinkTransition(a, npc, inc, true, P::VisibilityPath::Direct, t) ==
          P::Outcome::RestoreServerEvidence);
    CHECK(p.CopyExactPendingRestores(entries, count, required) ==
          Proto::SnapshotResult::KnownPairsCopied);
    CHECK(entries[0].relinkAtServer == P::ServerStage::RelinkedAfterF14);
    CHECK(R::SelectNormalRecheck(p, entries[0].viewer, entries[0].creature,
          entries[0].incarnation, Now(a, npc, inc)) ==
          R::Result::AlreadyServerRelinkedClientUnknown);
    CHECK(p.LedgerView().PendingRestores() == 2); // Not ACK

    // Global config OFF: third previously-hidden pair also must reappear.
    p.ObserveConfigDisabled();
    CHECK(p.CopyExactPendingRestores(entries, count, required) ==
          Proto::SnapshotResult::KnownPairsCopied);
    CHECK(count == 3 && required == 3);
    CHECK(entries[2].viewer == c && entries[2].creature == other);
    CHECK(R::SelectNormalRecheck(p, entries[2].viewer, entries[2].creature,
          entries[2].incarnation, Now(c, other, inc)) ==
          R::Result::RequestNormalCoreVisibilityPass);
    CHECK(p.LedgerView().PendingRestores() == 3);

    // Unattributed new hide risk means the known 3 are NOT an all-clear.
    auto bad = SyntheticProof(a, npc, inc);
    bad.eventInstanceId += 1;
    CHECK(p.ObserveF14SuppressionDecision(a, npc, inc, true,
          P::VisibilityPath::Direct, bad) == P::Outcome::FailVisibleGlobalRestore);
    CHECK(p.CopyExactPendingRestores(entries, count, required) ==
          Proto::SnapshotResult::KnownPairsWithUnattributedRisk);
    CHECK(count == 3 && required == 3);
    CHECK(p.LedgerView().PendingRestores() == 3);
    CHECK(p.GlobalRestoreRequired());

    std::cout << "PASS F14 snapshot -> exact current viewer recheck: 3 records, "
              << "undersized fail atomic, stale/headless/wrong-owner/wrong-map veto, "
              << "server-only relink never ACK, F14 OFF/era advancement preserves debt, "
              << "unattributed risk never declares global all-clear\n";
    return 0;
}
