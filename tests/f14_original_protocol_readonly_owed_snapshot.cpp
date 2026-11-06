// F14 original Claude #94 additive read-only snapshot regression.
// NO runtime Core/Playerbots, no client ACK, no positive policy or ledger clear.
#include "IndividualProgressionF14ViewerRestoreProtocol.h"
#include <array>
#include <cstdint>
#include <iostream>

namespace P = IndividualProgressionF14ViewerRestoreProtocol;
#define CHECK(x) do { if (!(x)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #x << '\n'; \
    return 1; \
} } while (false)

static P::DecisionFacts SyntheticOnly(P::ViewerKey v, P::CreatureKey c, std::uint64_t epoch)
{
    P::DecisionFacts f;
    f.configEnabled = true;
    f.actorPositivelyVerifiedRandomBot = true;
    f.actorEntitled = true;
    f.pairRightsProven = true;
    f.creatureAlive = true;
    f.currentViewerEpoch = v.loginEpoch;
    f.currentCreatureIncarnation = epoch;
    f.eventMapId = c.mapId;
    f.eventInstanceId = c.instanceId;
    return f;
}

template<std::size_t N>
static bool Contains(std::array<typename P::Protocol<N>::ExactPendingRestore, N> const&,
                     std::size_t, P::ViewerKey, P::CreatureKey)
{
    return false; // unused: tests inspect exact entries and ordering below
}

static int TestExactCopyAndGlobalRisk()
{
    using Proto = P::Protocol<6>;
    using Record = Proto::ExactPendingRestore;
    Proto p;
    P::ViewerKey const v1{101, 11}, v2{102, 21}, v3{103, 31};
    P::CreatureKey const c1{1, 10, 901}, c2{1, 10, 902}, c3{1, 10, 903};
    std::uint64_t const incarnation = 777;

    CHECK(p.ObserveF14SuppressionDecision(v1, c1, incarnation, true,
        P::VisibilityPath::Direct, SyntheticOnly(v1, c1, incarnation)) == P::Outcome::SuppressionRecorded);
    CHECK(p.ObserveF14SuppressionDecision(v2, c1, incarnation, false,
        P::VisibilityPath::Batched, SyntheticOnly(v2, c1, incarnation)) == P::Outcome::SuppressionRecorded);
    CHECK(p.ObserveF14SuppressionDecision(v1, c2, incarnation, true,
        P::VisibilityPath::Batched, SyntheticOnly(v1, c2, incarnation)) == P::Outcome::SuppressionRecorded);
    CHECK(p.ObserveF14SuppressionDecision(v3, c3, incarnation, false,
        P::VisibilityPath::Direct, SyntheticOnly(v3, c3, incarnation)) == P::Outcome::SuppressionRecorded);
    CHECK(p.Size() == 4);
    CHECK(p.LedgerView().Size() == 4);
    CHECK(p.LedgerView().PendingRestores() == 0);

    std::array<Record, 4> data{};
    std::size_t count = 42, needed = 42;
    CHECK(p.CopyExactPendingRestores(data, count, needed) == Proto::SnapshotResult::KnownPairsCopied);
    CHECK(count == 0 && needed == 0);

    p.ObserveCreatureRetired(c1);
    CHECK(p.LedgerView().PendingRestores() == 2);

    Record sentinel{};
    sentinel.viewer = {8008, 99};
    sentinel.creature = {999, 888, 777};
    sentinel.incarnation = 123456;

    std::array<Record, 1> tooSmall{{sentinel}};
    count = 99;
    needed = 99;
    CHECK(p.CopyExactPendingRestores(tooSmall, count, needed) == Proto::SnapshotResult::OutputTooSmallNoCopy);
    CHECK(count == 0 && needed == 2);
    CHECK(tooSmall[0].viewer == sentinel.viewer);
    CHECK(tooSmall[0].creature == sentinel.creature);
    CHECK(tooSmall[0].incarnation == sentinel.incarnation);

    count = needed = 99;
    CHECK(p.CopyExactPendingRestores(data, count, needed) == Proto::SnapshotResult::KnownPairsCopied);
    CHECK(count == 2 && needed == 2);
    CHECK(data[0].viewer == v1 && data[0].creature == c1);
    CHECK(data[1].viewer == v2 && data[1].creature == c1);
    CHECK(data[0].incarnation == incarnation && data[1].incarnation == incarnation);
    CHECK(data[0].suppressedAtServer == P::ServerStage::UnlinkedByF14);
    CHECK(data[1].suppressedAtServer == P::ServerStage::CreateWithheldByF14);
    CHECK(data[0].relinkAtServer == P::ServerStage::None);

    // Core stage-2 re-link evidence does not remove the record or debt.
    P::TransitionFacts t;
    t.currentViewerEpoch = v1.loginEpoch;
    t.currentCreatureIncarnation = incarnation;
    t.eventMapId = c1.mapId;
    t.eventInstanceId = c1.instanceId;
    CHECK(p.ObserveLinkTransition(v1, c1, incarnation, true,
        P::VisibilityPath::Direct, t) == P::Outcome::RestoreServerEvidence);
    CHECK(p.CopyExactPendingRestores(data, count, needed) == Proto::SnapshotResult::KnownPairsCopied);
    CHECK(count == 2);
    CHECK(data[0].relinkAtServer == P::ServerStage::RelinkedAfterF14);
    CHECK(data[0].restorePacket == P::PacketStage::HandedToSessionDirect);
    CHECK(p.LedgerView().PendingRestores() == 2);

    // Disable F14: ALL four old pairs need restore, including different
    // human viewer and target; never discard due new client progression.
    p.ObserveConfigDisabled();
    CHECK(p.GlobalRestoreRequired());
    CHECK(p.LedgerView().PendingRestores() == 4);
    CHECK(p.CopyExactPendingRestores(data, count, needed) == Proto::SnapshotResult::KnownPairsCopied);
    CHECK(count == 4 && needed == 4);
    CHECK(data[2].viewer == v1 && data[2].creature == c2);
    CHECK(data[3].viewer == v3 && data[3].creature == c3);
    CHECK(p.LedgerView().Size() == 4);

    // A 0-capacity destination is an explicit no-copy failure, NOT silent
    // truncation or a partial success.
    std::array<Record, 0> zero{};
    CHECK(p.CopyExactPendingRestores(zero, count, needed) == Proto::SnapshotResult::OutputTooSmallNoCopy);
    CHECK(count == 0 && needed == 4);

    // Wrong map F14-positive decision becomes un-attributed risk; snapshot
    // still exposes old exact known pairs but MUST announce incompleteness.
    auto malformed = SyntheticOnly(v3, c3, incarnation);
    ++malformed.eventMapId;
    CHECK(p.ObserveF14SuppressionDecision(v3, c3, incarnation, true,
        P::VisibilityPath::Direct, malformed) == P::Outcome::FailVisibleGlobalRestore);
    CHECK(p.UnattributedHideRisk());
    CHECK(p.CopyExactPendingRestores(data, count, needed) ==
          Proto::SnapshotResult::KnownPairsWithUnattributedRisk);
    CHECK(count == 4 && needed == 4);
    CHECK(p.LedgerView().PendingRestores() == 4);
    return 0;
}

static int TestCapacityOverflowPoisonNeverEvicts()
{
    using Proto = P::Protocol<2>;
    using Record = Proto::ExactPendingRestore;
    Proto p;
    std::uint64_t const inc = 300;
    P::CreatureKey const c{0, 0, 20};
    for (std::uint64_t i = 1; i <= 2; ++i)
    {
        P::ViewerKey const v{i, i};
        CHECK(p.ObserveF14SuppressionDecision(v, c, inc, i == 1,
            P::VisibilityPath::Direct, SyntheticOnly(v, c, inc)) ==
            P::Outcome::SuppressionRecorded);
    }
    P::ViewerKey const extra{3, 3};
    CHECK(p.ObserveF14SuppressionDecision(extra, c, inc, true,
        P::VisibilityPath::Direct, SyntheticOnly(extra, c, inc)) ==
        P::Outcome::FailVisibleGlobalRestore);
    CHECK(p.GlobalRestoreRequired());
    CHECK(p.UnattributedHideRisk());
    CHECK(p.LedgerView().PendingRestores() == 2);
    std::array<Record, 2> buffer{};
    std::size_t count = 0, required = 0;
    CHECK(p.CopyExactPendingRestores(buffer, count, required) ==
          Proto::SnapshotResult::KnownPairsWithUnattributedRisk);
    CHECK(count == 2 && required == 2);
    CHECK(buffer[0].viewer.guidRaw == 1);
    CHECK(buffer[1].viewer.guidRaw == 2);
    CHECK(p.LedgerView().Size() == 2);
    return 0;
}

static int TestBoundedSubsetNeverSilentlyDrops()
{
    using Proto = P::Protocol<16>;
    using Record = Proto::ExactPendingRestore;
    Proto p;
    P::CreatureKey const c{571, 7, 717};
    for (std::uint64_t i = 1; i <= 16; ++i)
    {
        P::ViewerKey const viewer{1000 + i, 2000 + i};
        CHECK(p.ObserveF14SuppressionDecision(viewer, c, 999, i % 2 == 0,
            P::VisibilityPath::Direct, SyntheticOnly(viewer, c, 999)) ==
            P::Outcome::SuppressionRecorded);
    }
    p.ObserveCreatureRetired(c);
    std::array<Record, 15> small{};
    small[0].viewer.guidRaw = 888888;
    std::size_t count = 77, needed = 0;
    CHECK(p.CopyExactPendingRestores(small, count, needed) ==
          Proto::SnapshotResult::OutputTooSmallNoCopy);
    CHECK(count == 0 && needed == 16);
    CHECK(small[0].viewer.guidRaw == 888888);
    std::array<Record, 16> exact{};
    CHECK(p.CopyExactPendingRestores(exact, count, needed) ==
          Proto::SnapshotResult::KnownPairsCopied);
    CHECK(count == 16 && needed == 16);
    for (std::size_t i = 0; i < count; ++i)
    {
        CHECK(exact[i].viewer.guidRaw == 1001 + i);
        CHECK(exact[i].viewer.loginEpoch == 2001 + i);
        CHECK(exact[i].incarnation == 999);
    }
    CHECK(p.LedgerView().PendingRestores() == 16);
    return 0;
}

int main()
{
    CHECK(TestExactCopyAndGlobalRisk() == 0);
    CHECK(TestCapacityOverflowPoisonNeverEvicts() == 0);
    CHECK(TestBoundedSubsetNeverSilentlyDrops() == 0);
    std::cout << "F14 original #94 read-only known-owed-pair snapshot PASS: "
              << "capacity/no-truncation, exact viewer epochs, map/incarnation, "
              << "direct/batched source stages, frozen/global risk, "
              << "no client ACK, no original ledger mutation, F14 OFF\n";
    return 0;
}
