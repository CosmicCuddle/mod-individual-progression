// Pure F14 bounded snapshot snapshot->freshness->viewer recheck model.
// No runtime hooks, Core Player pointers, packets or client object ACKs.
#include "IndividualProgressionF14StaleSnapshotRecheckGuard.h"
#include <array>
#include <cstdint>
#include <iostream>

namespace P = IndividualProgressionF14ViewerRestoreProtocol;
namespace R = IndividualProgressionF14ExactViewerRestoreRecheckPlan;
namespace H = IndividualProgressionF14StaleSnapshotRecheckGuard;
#define CHECK(x) do { if (!(x)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #x << '\n'; \
    return 1; \
} } while (false)

static P::ViewerKey const a{101, 1001}, b{102, 1002}, c{103, 1003};
static P::CreatureKey const npc{1, 9, 9001}, other{1, 9, 9002};
static std::uint64_t const inc = 2026;

static P::DecisionFacts Proof(P::ViewerKey v, P::CreatureKey creature)
{
    P::DecisionFacts d;
    d.configEnabled = true;
    d.actorPositivelyVerifiedRandomBot = true;
    d.actorEntitled = true;
    d.pairRightsProven = true;
    d.creatureAlive = true;
    d.currentViewerEpoch = v.loginEpoch;
    d.currentCreatureIncarnation = inc;
    d.eventMapId = creature.mapId;
    d.eventInstanceId = creature.instanceId;
    return d;
}

static R::Facts AllFresh(P::ViewerKey viewer, P::CreatureKey creature)
{
    R::Facts f;
    f.normalAccountHumanNonHeadlessSession = true;
    f.viewerInWorld = true;
    f.creatureInWorld = true;
    f.currentOwningMapThreadProven = true; // SYNTHETIC value; no Core map proof
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
    f.viewerStillVanilla = false;
    f.featureStillEnabled = false;
    return f;
}

static R::Facts VaryLiveFacts(std::uint32_t bits)
{
    auto f = AllFresh(b, npc);
    auto bit = [bits](unsigned n) { return (bits & (1u << n)) != 0; };
    f.normalAccountHumanNonHeadlessSession = bit(0);
    f.viewerInWorld = bit(1);
    f.creatureInWorld = bit(2);
    f.currentOwningMapThreadProven = bit(3);
    f.currentViewerGuidRaw = bit(4) ? b.guidRaw : a.guidRaw;
    f.currentViewerLoginEpoch = bit(5) ? b.loginEpoch : b.loginEpoch + 1;
    f.currentCreatureGuidRaw = bit(6) ? npc.guidRaw : other.guidRaw;
    f.currentCreatureIncarnation = bit(7) ? inc : inc + 1;
    f.viewerMapId = bit(8) ? npc.mapId : npc.mapId + 1;
    f.viewerInstanceId = bit(9) ? npc.instanceId : npc.instanceId + 1;
    f.creatureMapId = bit(10) ? npc.mapId : npc.mapId + 1;
    f.creatureInstanceId = bit(11) ? npc.instanceId : npc.instanceId + 1;
    f.currentEventMapId = bit(12) ? npc.mapId : npc.mapId + 1;
    f.currentEventInstanceId = bit(13) ? npc.instanceId : npc.instanceId + 1;
    f.viewerStillVanilla = bit(14);
    f.featureStillEnabled = bit(15);
    return f;
}

int main()
{
    static_assert(!H::MayDispatchCoreDirectly(), "No Core dispatch");
    static_assert(!H::GuaranteesOwningMapThread(), "Caller must prove owning map");
    static_assert(!H::CanClearOriginalLedger(), "No debt clear");
    static_assert(!H::HasClientObjectAck(), "No invented client ACK");

    using Proto = P::Protocol<6>;
    using Record = Proto::ExactPendingRestore;
    Proto original;
    CHECK(original.ObserveF14SuppressionDecision(a, npc, inc, true,
        P::VisibilityPath::Direct, Proof(a, npc)) == P::Outcome::SuppressionRecorded);
    CHECK(original.ObserveF14SuppressionDecision(b, npc, inc, false,
        P::VisibilityPath::Batched, Proof(b, npc)) == P::Outcome::SuppressionRecorded);
    CHECK(original.ObserveF14SuppressionDecision(c, other, inc, true,
        P::VisibilityPath::Direct, Proof(c, other)) == P::Outcome::SuppressionRecorded);

    original.ObserveCreatureRetired(npc);
    std::array<Record, 6> captured{}, scratch{};
    std::size_t count = 0, need = 0;
    auto outcome = original.CopyExactPendingRestores(captured, count, need);
    CHECK(outcome == Proto::SnapshotResult::KnownPairsCopied);
    CHECK(count == 2 && need == 2);
    CHECK(!original.GlobalRestoreRequired());

    auto evaluate = [&](std::size_t index, R::Facts const& facts, bool owner) {
        return H::RecheckCapturedKnownPair(original, captured, count,
                outcome, false, scratch, index, facts, owner);
    };
    CHECK(evaluate(0, AllFresh(a, npc), true) == H::Result::CandidateNormalCoreRecheckOnly);
    CHECK(evaluate(1, AllFresh(b, npc), false) == H::Result::NoProvenSerializedOwner);
    CHECK(H::RecheckCapturedKnownPair(original, captured, count, outcome, false,
        captured, 0, AllFresh(a, npc), true) == H::Result::CallerOutputAliasesSnapshot);
    CHECK(evaluate(2, AllFresh(a, npc), true) == H::Result::InvalidPairIndex);
    CHECK(evaluate(1, AllFresh(a, npc), true) == H::Result::OriginalPairNowIneligible);

    // A different NPC becomes owed AFTER snapshot: old two-pair list is NOT
    // a complete global inventory. No partial dispatch based on stale list.
    original.ObserveCreatureRetired(other);
    CHECK(evaluate(0, AllFresh(a, npc), true) == H::Result::StaleSnapshotNoDispatch);
    CHECK(original.LedgerView().PendingRestores() == 3);
    outcome = original.CopyExactPendingRestores(captured, count, need);
    CHECK(count == 3 && need == 3);
    CHECK(evaluate(2, AllFresh(c, other), true) == H::Result::CandidateNormalCoreRecheckOnly);

    // Config OFF can flip global restore freeze without changing the list's
    // existing keys; the global status is compared separately.
    original.ObserveConfigDisabled();
    CHECK(evaluate(1, AllFresh(b, npc), true) == H::Result::StaleSnapshotNoDispatch);
    CHECK(original.GlobalRestoreRequired());
    bool const frozen = original.GlobalRestoreRequired();
    outcome = original.CopyExactPendingRestores(captured, count, need);
    CHECK(H::RecheckCapturedKnownPair(original, captured, count, outcome, frozen,
        scratch, 1, AllFresh(b, npc), true) == H::Result::CandidateNormalCoreRecheckOnly);

    // Server relink after snapshot makes saved stage stale even though
    // the same original ledger debt remains outstanding (no client ACK).
    P::TransitionFacts t;
    t.currentViewerEpoch = a.loginEpoch;
    t.currentCreatureIncarnation = inc;
    t.eventMapId = npc.mapId;
    t.eventInstanceId = npc.instanceId;
    CHECK(original.ObserveLinkTransition(a, npc, inc, true,
          P::VisibilityPath::Direct, t) == P::Outcome::RestoreServerEvidence);
    CHECK(H::RecheckCapturedKnownPair(original, captured, count, outcome, frozen,
          scratch, 0, AllFresh(a, npc), true) == H::Result::StaleSnapshotNoDispatch);
    outcome = original.CopyExactPendingRestores(captured, count, need);
    CHECK(H::RecheckCapturedKnownPair(original, captured, count, outcome, frozen,
          scratch, 0, AllFresh(a, npc), true) == H::Result::OriginalPairNowIneligible);
    CHECK(original.NeedsRestore(a, npc));

    // An invalid new suppression produces "known subset with risk":
    // known pairs can still be considered for safe recheck but the known
    // list can never become a claimed global all-clear.
    auto bad = Proof(c, other);
    ++bad.eventInstanceId;
    CHECK(original.ObserveF14SuppressionDecision(c, other, inc, true,
          P::VisibilityPath::Direct, bad) == P::Outcome::FailVisibleGlobalRestore);
    CHECK(H::RecheckCapturedKnownPair(original, captured, count, outcome, frozen,
          scratch, 1, AllFresh(b, npc), true) == H::Result::StaleSnapshotNoDispatch);
    outcome = original.CopyExactPendingRestores(captured, count, need);
    CHECK(outcome == Proto::SnapshotResult::KnownPairsWithUnattributedRisk);
    CHECK(H::RecheckCapturedKnownPair(original, captured, count, outcome, frozen,
          scratch, 1, AllFresh(b, npc), true) == H::Result::CandidateKnownPairWithUnattributedRisk);

    // Missing capacity must never use one available pair while others are
    // omitted. CopyExactPendingRestores fails without mutation.
    std::array<Record, 1> small{}, spare{};
    std::size_t partialCount = 0, required = 0;
    auto const partial = original.CopyExactPendingRestores(small, partialCount, required);
    CHECK(partial == Proto::SnapshotResult::OutputTooSmallNoCopy);
    CHECK(partialCount == 0 && required == 3);
    CHECK(H::RecheckCapturedKnownPair(original, small, partialCount, partial, frozen,
          spare, 0, AllFresh(a, npc), true) == H::Result::SnapshotNotComplete);

    // Exhaustive current-fact changes AFTER a correctly captured list:
    // each of 14 required independent identities/ownership flags may be
    // invalidated; current F14/Vanilla toggles must NOT block old debt.
    std::uint32_t cases = 0;
    for (std::uint32_t bits = 0; bits < 65536u; ++bits)
    {
        auto facts = VaryLiveFacts(bits);
        auto const result = H::RecheckCapturedKnownPair(original, captured, count,
            outcome, frozen, scratch, 1, facts, true);
        bool const requiredFacts = (bits & 0x3fffu) == 0x3fffu;
        CHECK((result == H::Result::CandidateKnownPairWithUnattributedRisk) == requiredFacts);
        CHECK(original.LedgerView().PendingRestores() == 3);
        ++cases;
    }
    CHECK(cases == 65536);
    std::cout << "PASS F14 stale-snapshot failclosed, exact old human/creature "
              << "revalidation: " << cases << " current fact states, "
              << "config freeze/new owed pair/relink/global risk/undersized list "
              << "never silently dispatch, client ACK absent, no packets\n";
    return 0;
}
