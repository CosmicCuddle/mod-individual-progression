// F14 #114: pure, source-level lifecycle evidence across BOTH actual pinned
// Core corpse-removal modes. No Core execution, client, DB or live hide.
// Reuses original Claude #94 per-viewer protocol and #103 send-attempt bridge
// UNCHANGED. Every apparent success is SERVER evidence, not client rendering.
#include "IndividualProgressionF14ViewerRestoreProtocol.h"
#include "IndividualProgressionF14GuidSendAttemptEvidence.h"
#include <cstdint>
#include <cstdio>

namespace P = IndividualProgressionF14ViewerRestoreProtocol;
namespace A = IndividualProgressionF14GuidSendAttemptEvidence;

static int g_pass = 0;
static int g_fail = 0;
#define CHECK(c) do { if (c) ++g_pass; else { ++g_fail; \
    std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static P::ViewerKey const kViewer{100, 10};
static P::ViewerKey const kRelogged{100, 11};
static P::CreatureKey const kCreature{1, 2, 901};
static std::uint64_t const kIncarnation = 1001;

static P::DecisionFacts SyntheticF14PositivePrehideFacts()
{
    P::DecisionFacts f;
    f.configEnabled = true;
    f.actorPositivelyVerifiedRandomBot = true;
    f.actorEntitled = true;
    f.pairRightsProven = true;
    f.creatureAlive = true;
    f.currentViewerEpoch = kViewer.loginEpoch;
    f.currentCreatureIncarnation = kIncarnation;
    f.eventMapId = kCreature.mapId;
    f.eventInstanceId = kCreature.instanceId;
    return f;
}

static P::TransitionFacts SyntheticTransition(bool oldEpoch = true)
{
    P::TransitionFacts t;
    t.currentViewerEpoch = oldEpoch ? kViewer.loginEpoch : kRelogged.loginEpoch;
    t.currentCreatureIncarnation = kIncarnation;
    t.eventMapId = kCreature.mapId;
    t.eventInstanceId = kCreature.instanceId;
    return t;
}

static A::AttemptFacts SyntheticTaggedAfterSend()
{
    A::AttemptFacts a;
    a.sourceAttributedByExactCoreF14Hook = true;
    a.afterActualSessionSendDirectMessageCall = true;
    a.sameOwningMapThread = true; // input ASSUMPTION, not established by Core
    a.reportedGuidExactlyMatchesCreature = true;
    a.currentViewerLoginEpoch = kViewer.loginEpoch;
    a.currentCreatureIncarnation = kIncarnation;
    a.eventMapId = kCreature.mapId;
    a.eventInstanceId = kCreature.instanceId;
    return a;
}

static void Evaluate(bool compatibilityMode, bool linkedBeforeHide,
                     P::VisibilityPath path, bool serverRelinkObserved)
{
    using Proto = P::Protocol<4>;
    Proto p;
    P::DecisionFacts const proof = SyntheticF14PositivePrehideFacts();

    // This is a SYNTHETIC positive decision: production F14 policy remains OFF.
    CHECK(p.MayAnswerPositiveSuppression(kViewer, kCreature, kIncarnation, proof));
    CHECK(p.ObserveF14SuppressionDecision(kViewer, kCreature, kIncarnation,
        linkedBeforeHide, path, proof) == P::Outcome::SuppressionRecorded);
    CHECK(p.Size() == 1);
    CHECK(!p.GlobalRestoreRequired());

    // ATTACK STOP / JUST DIED revokes every previously F14-suppressed pair.
    p.ObserveCreatureRetired(kCreature);
    CHECK(p.NeedsRestore(kViewer, kCreature));
    CHECK(p.LedgerView().PendingRestores() == 1);
    CHECK(!p.MayAnswerPositiveSuppression(kViewer, kCreature, kIncarnation, proof));

    // An OUT attempt arriving after retire must not be newly accepted as
    // proof of a valid hide; batched direction ordering matters.
    CHECK(A::ObserveExactTaggedBatchAttempt(p, kViewer, kCreature, kIncarnation,
        A::Direction::OutOfRange, SyntheticTaggedAfterSend()) ==
        (linkedBeforeHide && path == P::VisibilityPath::Batched
         ? A::Outcome::WrongDirectionOrServerStage
         : A::Outcome::WrongDirectionOrServerStage));

    // Old epoch is not the newly connected client and NEVER acknowledges
    // restoration of this original hidden pair.
    CHECK(p.ObserveLinkTransition(kRelogged, kCreature, kIncarnation, true,
        path, SyntheticTransition(false)) == P::Outcome::IgnoredStale);
    CHECK(p.NeedsRestore(kViewer, kCreature));

    if (serverRelinkObserved)
    {
        CHECK(p.ObserveLinkTransition(kViewer, kCreature, kIncarnation, true,
            path, SyntheticTransition()) == P::Outcome::RestoreServerEvidence);
        P::Evidence const e = p.Find(kViewer, kCreature);
        CHECK(e.restoreServer == P::ServerStage::RelinkedAfterF14);
        if (path == P::VisibilityPath::Batched)
        {
            CHECK(e.restorePacket == P::PacketStage::ImpliedPendingBatchFlush);
            CHECK(A::ObserveExactTaggedBatchAttempt(p, kViewer, kCreature, kIncarnation,
                A::Direction::Create, SyntheticTaggedAfterSend()) ==
                A::Outcome::ExactTaggedServerSendCallOnly);
            CHECK(p.Find(kViewer, kCreature).restorePacket ==
                P::PacketStage::HandedToSessionBatched);
        }
        else
        {
            CHECK(e.restorePacket == P::PacketStage::HandedToSessionDirect);
            CHECK(A::ObserveExactTaggedBatchAttempt(p, kViewer, kCreature, kIncarnation,
                A::Direction::Create, SyntheticTaggedAfterSend()) ==
                A::Outcome::NotPendingSameDirectionBatch);
        }

        // A matching SendDirectMessage call is still not an object ACK.
        CHECK(p.NeedsRestore(kViewer, kCreature));
        CHECK(p.LedgerView().PendingRestores() == 1);
    }
    else
    {
        CHECK(p.Find(kViewer, kCreature).restoreServer == P::ServerStage::None);
        CHECK(A::ObserveExactTaggedBatchAttempt(p, kViewer, kCreature, kIncarnation,
            A::Direction::Create, SyntheticTaggedAfterSend()) ==
            A::Outcome::WrongDirectionOrServerStage);
    }

    // SAME actual pinned Core lifecycle choices:
    //  - compatibility: setDeathState(Dead), destroy for visible players;
    //  - dynamic: loot.clear(), SaveRespawnTime(), AddObjectToRemoveList().
    // Both lack any client object ACK and there may be no later Unit::Update.
    if (compatibilityMode)
        p.ObserveCreatureRetired(kCreature); // Corpse -> Dead (repeated)
    else
        p.ObserveMapUnloaded(kCreature.mapId, kCreature.instanceId); // removal/instance exit
    CHECK(p.NeedsRestore(kViewer, kCreature));
    CHECK(p.LedgerView().PendingRestores() == 1);

    // Corpse is gone: a recycled full-GUID/instance incarnation is not an
    // acknowledgement of the old viewer/creature hidden pair.
    CHECK(p.ObserveLinkTransition(kViewer, kCreature, kIncarnation + 1, true,
        path, SyntheticTransition()) == P::Outcome::IgnoredRecycled);
    CHECK(p.NeedsRestore(kViewer, kCreature));

    // Log out and config OFF are revocation requests, NOT positive proof.
    p.ObserveViewerSessionEnded(kViewer.guidRaw);
    p.ObserveConfigDisabled();
    CHECK(p.GlobalRestoreRequired());
    CHECK(p.NeedsRestore(kViewer, kCreature));
    CHECK(p.LedgerView().PendingRestores() == 1);
    CHECK(p.Size() == 1);
    CHECK(!p.MayAnswerPositiveSuppression(kViewer, kCreature, kIncarnation, proof));
    CHECK(!A::HasIndependentlyVerifiedClientPresentation());
    CHECK(!A::IsRuntimeF14ConcealmentAuthorized());
}

int main()
{
    static_assert(!A::HasIndependentlyVerifiedClientPresentation(),
                  "3.3.5a Core send-attempt is NEVER client presentation evidence");
    static_assert(!A::IsRuntimeF14ConcealmentAuthorized(),
                  "F14 protocol NEVER authorises runtime hide");
    static_assert(P::ClientStageOf() == P::ClientStage::NotServerObservable,
                  "the 3.3.5a client object does not ACK its presentation");

    unsigned combinations = 0;
    for (int compatible = 0; compatible < 2; ++compatible)
        for (int linked = 0; linked < 2; ++linked)
            for (int batched = 0; batched < 2; ++batched)
                for (int relink = 0; relink < 2; ++relink)
                {
                    Evaluate(compatible != 0, linked != 0,
                             batched ? P::VisibilityPath::Batched : P::VisibilityPath::Direct,
                             relink != 0);
                    ++combinations;
                }
    CHECK(combinations == 16);
    std::printf("F14 both Core corpse removal modes / original #94+#103 protocol: "
                "%u scenarios, %d checks passed, %d failed; all prior-hidden "
                "obligations retained\n", combinations, g_pass, g_fail);
    return g_fail ? 1 : 0;
}
