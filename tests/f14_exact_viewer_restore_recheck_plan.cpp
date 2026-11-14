// Pure F14 restoration recheck selector; uses ORIGINAL #94 protocol unchanged.
// Full exhaustiveness over live-fact mismatch and era/feature changes.
// The synthetic positive-hide record is test input ONLY, never production.
#include "IndividualProgressionF14ExactViewerRestoreRecheckPlan.h"
#include <cstdint>
#include <iostream>

namespace P = IndividualProgressionF14ViewerRestoreProtocol;
namespace S = IndividualProgressionF14ExactViewerRestoreRecheckPlan;

#define CHECK(c) do { if (!(c)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #c << '\n'; \
    return 1; \
} } while (false)

static P::ViewerKey const viewer{1001, 71};
static P::CreatureKey const creature{1, 2, 929191};
static std::uint64_t const incarnation = 5007;

static P::DecisionFacts SyntheticOnlyOriginalProof()
{
    P::DecisionFacts f;
    f.configEnabled = true;
    f.actorPositivelyVerifiedRandomBot = true;
    f.actorEntitled = true;
    f.pairRightsProven = true;
    f.creatureAlive = true;
    f.currentViewerEpoch = viewer.loginEpoch;
    f.currentCreatureIncarnation = incarnation;
    f.eventMapId = creature.mapId;
    f.eventInstanceId = creature.instanceId;
    return f;
}

static S::Facts AllCurrentFacts()
{
    S::Facts f;
    f.normalAccountHumanNonHeadlessSession = true;
    f.viewerInWorld = true;
    f.creatureInWorld = true;
    f.currentOwningMapThreadProven = true;
    f.currentViewerGuidRaw = viewer.guidRaw;
    f.currentViewerLoginEpoch = viewer.loginEpoch;
    f.currentCreatureGuidRaw = creature.guidRaw;
    f.currentCreatureIncarnation = incarnation;
    f.viewerMapId = creature.mapId;
    f.viewerInstanceId = creature.instanceId;
    f.creatureMapId = creature.mapId;
    f.creatureInstanceId = creature.instanceId;
    f.currentEventMapId = creature.mapId;
    f.currentEventInstanceId = creature.instanceId;
    // Both default FALSE: restoring existing debt must work after F14
    // has been turned OFF or the player advanced beyond Vanilla.
    f.viewerStillVanilla = false;
    f.featureStillEnabled = false;
    return f;
}

static S::Facts FromBits(std::uint32_t bits)
{
    S::Facts f = AllCurrentFacts();
    auto bit = [bits](unsigned n) { return (bits & (1u << n)) != 0; };
    f.normalAccountHumanNonHeadlessSession = bit(0);
    f.viewerInWorld = bit(1);
    f.creatureInWorld = bit(2);
    f.currentOwningMapThreadProven = bit(3);
    f.currentViewerGuidRaw = bit(4) ? viewer.guidRaw : viewer.guidRaw + 1;
    f.currentViewerLoginEpoch = bit(5) ? viewer.loginEpoch : viewer.loginEpoch + 1;
    f.currentCreatureGuidRaw = bit(6) ? creature.guidRaw : creature.guidRaw + 1;
    f.currentCreatureIncarnation = bit(7) ? incarnation : incarnation + 1;
    f.viewerMapId = bit(8) ? creature.mapId : creature.mapId + 1;
    f.viewerInstanceId = bit(9) ? creature.instanceId : creature.instanceId + 1;
    f.creatureMapId = bit(10) ? creature.mapId : creature.mapId + 1;
    f.creatureInstanceId = bit(11) ? creature.instanceId : creature.instanceId + 1;
    f.currentEventMapId = bit(12) ? creature.mapId : creature.mapId + 1;
    f.currentEventInstanceId = bit(13) ? creature.instanceId : creature.instanceId + 1;
    f.viewerStillVanilla = bit(14);
    f.featureStillEnabled = bit(15);
    return f;
}

static int TestOriginalProtocolStages(bool originallyLinked)
{
    P::Protocol<4> p;
    auto const proof = SyntheticOnlyOriginalProof();
    CHECK(p.MayAnswerPositiveSuppression(viewer, creature, incarnation, proof));
    CHECK(p.ObserveF14SuppressionDecision(viewer, creature, incarnation,
        originallyLinked, P::VisibilityPath::Direct, proof) ==
        P::Outcome::SuppressionRecorded);

    S::Facts const live = AllCurrentFacts();
    CHECK(S::SelectNormalRecheck(p, viewer, creature, incarnation, live) ==
          S::Result::FirstRecordRevocationRequired);
    p.ObserveCreatureRetired(creature);
    CHECK(p.NeedsRestore(viewer, creature));

    // The old F14 flag can be switched OFF. Preserve a safe *recheck*,
    // not a bypass around ordinary Core CanSeeOrDetect or a new hide.
    p.ObserveConfigDisabled();
    CHECK(p.GlobalRestoreRequired());
    CHECK(S::SelectNormalRecheck(p, viewer, creature, incarnation, live) ==
          S::Result::RequestNormalCoreVisibilityPass);

    // Exact logged-in session and creature now absent: NO forced CREATE.
    S::Facts gone = live;
    gone.creatureInWorld = false;
    CHECK(S::SelectNormalRecheck(p, viewer, creature, incarnation, gone) ==
          S::Result::CreatureNoLongerInWorld);

    // Old epoch cannot be discharged by a fresh login; debt persists.
    S::Facts stale = live;
    ++stale.currentViewerLoginEpoch;
    CHECK(S::SelectNormalRecheck(p, viewer, creature, incarnation, stale) ==
          S::Result::StaleViewerLoginEpoch);

    // Only a real Core link transition upgrades SERVER evidence.
    P::TransitionFacts transition;
    transition.currentViewerEpoch = viewer.loginEpoch;
    transition.currentCreatureIncarnation = incarnation;
    transition.eventMapId = creature.mapId;
    transition.eventInstanceId = creature.instanceId;
    CHECK(p.ObserveLinkTransition(viewer, creature, incarnation, true,
        P::VisibilityPath::Direct, transition) == P::Outcome::RestoreServerEvidence);
    CHECK(S::SelectNormalRecheck(p, viewer, creature, incarnation, live) ==
          S::Result::AlreadyServerRelinkedClientUnknown);
    CHECK(p.NeedsRestore(viewer, creature));
    CHECK(p.LedgerView().PendingRestores() == 1);
    return 0;
}

int main()
{
    static_assert(!S::HasClientAcknowledgement(), "No Core client-object ACK");
    static_assert(!S::MayClearOriginalRestorationDebt(), "Never clear Claude's ledger");
    static_assert(!S::EnablesPositiveHide(), "Planner cannot grant F14 hiding");
    CHECK(TestOriginalProtocolStages(true) == 0);
    CHECK(TestOriginalProtocolStages(false) == 0);

    P::Protocol<4> p;
    auto const proof = SyntheticOnlyOriginalProof();
    CHECK(p.ObserveF14SuppressionDecision(viewer, creature, incarnation, true,
          P::VisibilityPath::Direct, proof) == P::Outcome::SuppressionRecorded);
    p.ObserveCreatureRetired(creature);

    CHECK(S::SelectNormalRecheck(p, viewer, creature,
          incarnation + 1, AllCurrentFacts()) == S::Result::WrongCreatureIncarnation);
    CHECK(S::SelectNormalRecheck(p, {viewer.guidRaw, viewer.loginEpoch + 1},
          creature, incarnation, AllCurrentFacts()) == S::Result::NoExactF14Hide);
    CHECK(S::SelectNormalRecheck(p, viewer, {creature.mapId, creature.instanceId,
          creature.guidRaw + 1}, incarnation, AllCurrentFacts()) ==
          S::Result::NoExactF14Hide);

    std::uint32_t passed = 0;
    for (std::uint32_t bits = 0; bits < 65536u; ++bits)
    {
        S::Facts const now = FromBits(bits);
        S::Result const outcome =
            S::SelectNormalRecheck(p, viewer, creature, incarnation, now);
        // Only 14 LIVE identity/map/session/ownership checks matter for
        // re-evaluating a previous hide. Current feature flag and Vanilla
        // progression MUST NOT veto an already-owed restoration.
        bool const allRequired = (bits & 0x3fffu) == 0x3fffu;
        CHECK((outcome == S::Result::RequestNormalCoreVisibilityPass) == allRequired);
        CHECK(p.NeedsRestore(viewer, creature));
        CHECK(p.LedgerView().PendingRestores() == 1);
        ++passed;
    }

    std::cout << "F14 exact viewer old-hide normal Core recheck: " << passed
              << " live-fact combinations PASS; previously-hidden debt remains, "
                 "no client ACK, no hiding, no packet or Core call\n";
    return 0;
}
