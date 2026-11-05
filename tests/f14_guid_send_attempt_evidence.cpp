// F14 pure evidence bridge: real Core #93/#95 callback fact *shape* only.
// Every positive below is SYNTHETIC, no Core callback or packet is wired.
// In 3.3.5a no object ACK is available; ledger MUST retain its obligation.
#include "IndividualProgressionF14GuidSendAttemptEvidence.h"
#include <cstdint>
#include <cstdio>

namespace A = IndividualProgressionF14GuidSendAttemptEvidence;
namespace P = IndividualProgressionF14ViewerRestoreProtocol;

static unsigned passed = 0, failed = 0;
#define CHECK(c) do { if (c) ++passed; else { ++failed; \
    std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

using Protocol = P::Protocol<4>;
static P::ViewerKey const viewer{100, 10};
static P::ViewerKey const otherViewer{101, 20};
static P::ViewerKey const relog{100, 11};
static P::CreatureKey const creature{1, 2, 901};
static P::CreatureKey const otherCreature{1, 2, 902};
static P::CreatureKey const wrongInstance{1, 3, 901};
static std::uint64_t const incarnation = 9001;

static P::DecisionFacts SyntheticApproved()
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
static P::TransitionFacts SyntheticTransition()
{
    P::TransitionFacts f;
    f.currentViewerEpoch = viewer.loginEpoch;
    f.currentCreatureIncarnation = incarnation;
    f.eventMapId = creature.mapId;
    f.eventInstanceId = creature.instanceId;
    return f;
}
static A::AttemptFacts SyntheticTaggedAttempt()
{
    A::AttemptFacts f;
    f.sourceAttributedByExactCoreF14Hook = true;
    f.afterActualSessionSendDirectMessageCall = true;
    f.sameOwningMapThread = true;
    f.reportedGuidExactlyMatchesCreature = true;
    f.currentViewerLoginEpoch = viewer.loginEpoch;
    f.currentCreatureIncarnation = incarnation;
    f.eventMapId = creature.mapId;
    f.eventInstanceId = creature.instanceId;
    return f;
}
static Protocol SyntheticPriorHide(bool linked = true,
                                   P::VisibilityPath path = P::VisibilityPath::Batched)
{
    Protocol p;
    CHECK(p.ObserveF14SuppressionDecision(viewer, creature, incarnation, linked,
                                          path, SyntheticApproved()) ==
          P::Outcome::SuppressionRecorded);
    return p;
}
static void CheckNoClientOrActiveHide()
{
    static_assert(!A::HasIndependentlyVerifiedClientPresentation(), "never client ACK");
    static_assert(!A::IsRuntimeF14ConcealmentAuthorized(), "never positive live hide");
    CHECK(!A::HasIndependentlyVerifiedClientPresentation());
    CHECK(!A::IsRuntimeF14ConcealmentAuthorized());
}
static void TestDefaultAndSourceAttributionFailClosed()
{
    Protocol p;
    A::AttemptFacts f;
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::Create, f) == A::Outcome::MissingOrUnverifiedCoreAttribution);
    CHECK(p.Size() == 0 && p.LedgerView().Size() == 0);

    p = SyntheticPriorHide();
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::MissingOrUnverifiedCoreAttribution);
    f = SyntheticTaggedAttempt();
    f.afterActualSessionSendDirectMessageCall = false;
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::MissingOrUnverifiedCoreAttribution);
    f = SyntheticTaggedAttempt();
    f.sourceAttributedByExactCoreF14Hook = false; // generic Core out sweep
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::MissingOrUnverifiedCoreAttribution);
    CHECK(p.Find(viewer, creature).suppressPacket == P::PacketStage::ImpliedPendingBatchFlush);
}
static void TestWrongOwnerEpochIncarnationGuidAndMap()
{
    Protocol p = SyntheticPriorHide();
    A::AttemptFacts f = SyntheticTaggedAttempt();
    f.currentViewerLoginEpoch++;
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    f = SyntheticTaggedAttempt();
    f.currentCreatureIncarnation++;
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    f = SyntheticTaggedAttempt();
    f.sameOwningMapThread = false;
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    f = SyntheticTaggedAttempt();
    f.reportedGuidExactlyMatchesCreature = false;
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    f = SyntheticTaggedAttempt();
    f.eventMapId++;
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    f = SyntheticTaggedAttempt();
    f.eventInstanceId++;
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    f = SyntheticTaggedAttempt();
    CHECK(A::ObserveExactTaggedBatchAttempt(p, relog, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, wrongInstance, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, otherCreature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::MissingMatchingPriorF14Suppression);
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation + 1,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, 0,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    CHECK(A::ObserveExactTaggedBatchAttempt(p, {}, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    CHECK(p.Find(viewer, creature).suppressPacket == P::PacketStage::ImpliedPendingBatchFlush);
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        static_cast<A::Direction>(255), SyntheticTaggedAttempt()) == A::Outcome::InvalidDirection);
}
static void TestSourceTaggedOutAndDuplicates()
{
    Protocol p = SyntheticPriorHide();
    auto f = SyntheticTaggedAttempt();
    CHECK(A::ObserveExactTaggedBatchAttempt(p, otherViewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::StaleOrCrossMapViewerCreature);
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::ExactTaggedServerSendCallOnly);
    CHECK(p.Find(viewer, creature).suppressPacket == P::PacketStage::HandedToSessionBatched);
    CHECK(!p.NeedsRestore(viewer, creature)); // still suppressed, not retired yet
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::NotPendingSameDirectionBatch);
    CHECK(p.LedgerView().Size() == 1 && !p.GlobalRestoreRequired());
    CHECK(!A::HasIndependentlyVerifiedClientPresentation());
}
static void TestRestoreOnlyAfterRealPriorHideAndRelink()
{
    auto f = SyntheticTaggedAttempt();
    Protocol none;
    CHECK(A::ObserveExactTaggedBatchAttempt(none, viewer, creature, incarnation,
        A::Direction::Create, f) == A::Outcome::MissingMatchingPriorF14Suppression);
    Protocol suppressed = SyntheticPriorHide();
    CHECK(A::ObserveExactTaggedBatchAttempt(suppressed, viewer, creature, incarnation,
        A::Direction::Create, f) == A::Outcome::WrongDirectionOrServerStage);
    suppressed.ObserveCreatureRetired(creature); // stop/death/corpse
    CHECK(suppressed.NeedsRestore(viewer, creature));
    // The OUT notification may reach this observer only after the source
    // has already been retired. It must NOT be upgraded as fresh hide proof.
    CHECK(A::ObserveExactTaggedBatchAttempt(suppressed, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::WrongDirectionOrServerStage);
    CHECK(suppressed.Find(viewer, creature).suppressPacket ==
          P::PacketStage::ImpliedPendingBatchFlush);
    CHECK(suppressed.LedgerView().PendingRestores() == 1);
    CHECK(A::ObserveExactTaggedBatchAttempt(suppressed, viewer, creature, incarnation,
        A::Direction::Create, f) == A::Outcome::WrongDirectionOrServerStage);
    CHECK(suppressed.ObserveLinkTransition(viewer, creature, incarnation, true,
        P::VisibilityPath::Batched, SyntheticTransition()) == P::Outcome::RestoreServerEvidence);
    CHECK(A::ObserveExactTaggedBatchAttempt(suppressed, viewer, creature, incarnation,
        A::Direction::Create, f) == A::Outcome::ExactTaggedServerSendCallOnly);
    CHECK(suppressed.Find(viewer, creature).restorePacket == P::PacketStage::HandedToSessionBatched);
    CHECK(suppressed.NeedsRestore(viewer, creature)); // SENT != RENDERED
    CHECK(suppressed.LedgerView().PendingRestores() == 1);
    CHECK(A::ObserveExactTaggedBatchAttempt(suppressed, viewer, creature, incarnation,
        A::Direction::Create, f) == A::Outcome::NotPendingSameDirectionBatch);
    CHECK(!suppressed.MayAnswerPositiveSuppression(viewer, creature, incarnation, SyntheticApproved()));
    CHECK(!suppressed.GlobalRestoreRequired()); // per-pair still owed, not global
}
static void TestWrongPathOrDirectionNeverUpgrade()
{
    auto f = SyntheticTaggedAttempt();
    Protocol withheld = SyntheticPriorHide(false);
    CHECK(A::ObserveExactTaggedBatchAttempt(withheld, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::WrongDirectionOrServerStage);
    CHECK(A::ObserveExactTaggedBatchAttempt(withheld, viewer, creature, incarnation,
        A::Direction::Create, f) == A::Outcome::WrongDirectionOrServerStage);
    Protocol direct = SyntheticPriorHide(true, P::VisibilityPath::Direct);
    CHECK(A::ObserveExactTaggedBatchAttempt(direct, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::NotPendingSameDirectionBatch);
    direct.ObserveCreatureRetired(creature);
    CHECK(direct.ObserveLinkTransition(viewer, creature, incarnation, true,
        P::VisibilityPath::Direct, SyntheticTransition()) == P::Outcome::RestoreServerEvidence);
    CHECK(A::ObserveExactTaggedBatchAttempt(direct, viewer, creature, incarnation,
        A::Direction::Create, f) == A::Outcome::NotPendingSameDirectionBatch);
    CHECK(direct.NeedsRestore(viewer, creature));
}
static void TestViewerRightsChangeRejectsDelayedOut()
{
    Protocol p = SyntheticPriorHide();
    // Rights changed before the matching per-GUID send attempt was delivered
    // to this observer; the original prior-hidden record now owes restore.
    p.ObserveViewerEraOrRightsChanged(viewer.guidRaw);
    CHECK(p.NeedsRestore(viewer, creature));
    CHECK(!p.GlobalRestoreRequired());
    auto f = SyntheticTaggedAttempt();
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::WrongDirectionOrServerStage);
    CHECK(p.Find(viewer, creature).suppressPacket == P::PacketStage::ImpliedPendingBatchFlush);
    CHECK(p.LedgerView().PendingRestores() == 1);
    CHECK(!p.MayAnswerPositiveSuppression(viewer, creature, incarnation, SyntheticApproved()));

    // A viewer session end is likewise a revocation, never confirmation.
    Protocol q = SyntheticPriorHide();
    q.ObserveViewerSessionEnded(viewer.guidRaw);
    CHECK(q.NeedsRestore(viewer, creature));
    CHECK(A::ObserveExactTaggedBatchAttempt(q, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::WrongDirectionOrServerStage);
    CHECK(q.Find(viewer, creature).suppressPacket == P::PacketStage::ImpliedPendingBatchFlush);
}
static void TestConfigurationAndViewerInvalidationsStillNeverClear()
{
    Protocol p = SyntheticPriorHide();
    p.ObserveViewerSessionEnded(viewer.guidRaw);
    p.ObserveMapUnloaded(creature.mapId, creature.instanceId);
    p.ObserveConfigDisabled();
    CHECK(p.GlobalRestoreRequired());
    CHECK(p.NeedsRestore(viewer, creature));
    auto f = SyntheticTaggedAttempt();
    CHECK(A::ObserveExactTaggedBatchAttempt(p, viewer, creature, incarnation,
        A::Direction::OutOfRange, f) == A::Outcome::FrozenGlobalRestore);
    CHECK(p.Find(viewer, creature).suppressPacket == P::PacketStage::ImpliedPendingBatchFlush);
    CHECK(p.NeedsRestore(viewer, creature));
    CHECK(p.GlobalRestoreRequired());
    CHECK(p.LedgerView().PendingRestores() == 1);
    CHECK(!A::HasIndependentlyVerifiedClientPresentation());
}
int main()
{
    CheckNoClientOrActiveHide();
    TestDefaultAndSourceAttributionFailClosed();
    TestWrongOwnerEpochIncarnationGuidAndMap();
    TestSourceTaggedOutAndDuplicates();
    TestRestoreOnlyAfterRealPriorHideAndRelink();
    TestWrongPathOrDirectionNeverUpgrade();
    TestViewerRightsChangeRejectsDelayedOut();
    TestConfigurationAndViewerInvalidationsStillNeverClear();
    std::printf("F14 tagged server-call evidence model: %u passed, %u failed\n", passed, failed);
    return failed ? 1 : 0;
}
