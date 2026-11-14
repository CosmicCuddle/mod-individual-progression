// Native tests for the pure F14 per-viewer restore EVIDENCE protocol.
// GCC/Clang, C++17/C++20, with and without NDEBUG. Simulated booleans here
// are NOT server or 3.3.5a client proof; nothing is wired or installed.
#include "IndividualProgressionF14ViewerRestoreProtocol.h"

#include <cstdint>
#include <cstdio>

namespace P = IndividualProgressionF14ViewerRestoreProtocol;

static unsigned g_pass = 0;
static unsigned g_fail = 0;

#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        if (cond)                                                                \
            ++g_pass;                                                            \
        else                                                                     \
        {                                                                        \
            ++g_fail;                                                            \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

static_assert(P::ClientStageOf() == P::ClientStage::NotServerObservable,
              "client presentation is never server-observable");

namespace
{
    P::ViewerKey const kViewer{100, 10};
    P::ViewerKey const kViewerRelog{100, 11};
    P::ViewerKey const kOther{200, 20};
    P::CreatureKey const kA{0, 0, 901};
    P::CreatureKey const kAOtherInstance{0, 1, 901};
    P::CreatureKey const kB{1, 0, 902};
    P::CreatureKey const kC{1, 0, 903};
    std::uint64_t const kIncA = 1001, kIncB = 2002, kIncC = 3003;

    // Simulated "everything proven" facts for the event's own viewer/creature.
    P::DecisionFacts Proven(P::ViewerKey v, P::CreatureKey c, std::uint64_t inc)
    {
        P::DecisionFacts f;
        f.configEnabled = true;
        f.actorPositivelyVerifiedRandomBot = true;
        f.actorEntitled = true;
        f.pairRightsProven = true;
        f.creatureAlive = true;
        f.currentViewerEpoch = v.loginEpoch;
        f.currentCreatureIncarnation = inc;
        f.eventMapId = c.mapId;
        f.eventInstanceId = c.instanceId;
        return f;
    }

    P::TransitionFacts Live(P::ViewerKey v, P::CreatureKey c, std::uint64_t inc)
    {
        P::TransitionFacts f;
        f.currentViewerEpoch = v.loginEpoch;
        f.currentCreatureIncarnation = inc;
        f.eventMapId = c.mapId;
        f.eventInstanceId = c.instanceId;
        return f;
    }

    using Proto = P::Protocol<4>;

    void ExpectFailVisible(P::DecisionFacts const& f, P::FailReason reason,
                           P::ViewerKey v = kViewer, P::CreatureKey c = kA, std::uint64_t inc = kIncA)
    {
        Proto p;
        CHECK(!p.MayAnswerPositiveSuppression(v, c, inc, f));
        CHECK(p.ObserveF14SuppressionDecision(v, c, inc, true, P::VisibilityPath::Direct, f) ==
              P::Outcome::FailVisibleGlobalRestore);
        CHECK(p.Failures(reason) == 1);
        CHECK(p.GlobalRestoreRequired());
        CHECK(p.UnattributedHideRisk());
        CHECK(p.NeedsRestore(kOther, kB)); // unknown pair: restore everything
        CHECK(p.Size() == 0);
        CHECK(p.LedgerView().Size() == 0);
        // Sticky: even a fully proven later decision cannot clear it.
        CHECK(!p.MayAnswerPositiveSuppression(kOther, kB, kIncB, Proven(kOther, kB, kIncB)));
        CHECK(p.ObserveF14SuppressionDecision(kOther, kB, kIncB, true, P::VisibilityPath::Direct,
                                              Proven(kOther, kB, kIncB)) ==
              P::Outcome::FailVisibleGlobalRestore);
        CHECK(p.GlobalRestoreRequired());
    }

    void TestDefaultFactsFailVisible()
    {
        Proto p;
        P::DecisionFacts const defaults{};
        CHECK(!p.MayAnswerPositiveSuppression(kViewer, kA, kIncA, defaults));
        ExpectFailVisible(defaults, P::FailReason::ConfigOff);
    }

    void TestEachMissingFactFailsVisible()
    {
        P::DecisionFacts f = Proven(kViewer, kA, kIncA);
        f.configEnabled = false;
        ExpectFailVisible(f, P::FailReason::ConfigOff);

        f = Proven(kViewer, kA, kIncA);
        f.actorPositivelyVerifiedRandomBot = false; // human or Altbot actor
        ExpectFailVisible(f, P::FailReason::UnverifiedActor);

        f = Proven(kViewer, kA, kIncA);
        f.actorEntitled = false;
        ExpectFailVisible(f, P::FailReason::NoEntitlement);

        f = Proven(kViewer, kA, kIncA);
        f.pairRightsProven = false; // the real pair-rights policy is default false
        ExpectFailVisible(f, P::FailReason::RightsNotProven);

        f = Proven(kViewer, kA, kIncA);
        f.currentViewerEpoch = kViewerRelog.loginEpoch; // stale session
        ExpectFailVisible(f, P::FailReason::StaleViewerSession);

        f = Proven(kViewer, kA, kIncA);
        f.currentCreatureIncarnation = kIncA + 1; // creature GUID reused
        ExpectFailVisible(f, P::FailReason::RecycledIncarnation);

        f = Proven(kViewer, kA, kIncA);
        f.creatureAlive = false; // death
        ExpectFailVisible(f, P::FailReason::CreatureNotAlive);

        f = Proven(kViewer, kA, kIncA);
        f.eventInstanceId = 7; // not the creature's owning map/instance
        ExpectFailVisible(f, P::FailReason::WrongMapThread);

        ExpectFailVisible(Proven({}, kA, kIncA), P::FailReason::InvalidKey, P::ViewerKey{});
        ExpectFailVisible(Proven(kViewer, {}, kIncA), P::FailReason::InvalidKey, kViewer, P::CreatureKey{});
        ExpectFailVisible(Proven(kViewer, kA, 0), P::FailReason::InvalidKey, kViewer, kA, 0);
    }

    void TestSuppressionStagesStaySeparate()
    {
        Proto p;
        CHECK(p.MayAnswerPositiveSuppression(kViewer, kA, kIncA, Proven(kViewer, kA, kIncA)));

        // Direct path, linked: destroy dispatched synchronously before the decision.
        CHECK(p.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        P::Evidence e = p.Find(kViewer, kA);
        CHECK(e.found && e.suppressIntent);
        CHECK(e.suppressServer == P::ServerStage::UnlinkedByF14);
        CHECK(e.suppressPacket == P::PacketStage::HandedToSessionDirect);
        CHECK(e.restoreServer == P::ServerStage::None && e.restorePacket == P::PacketStage::None);

        // Batched path, linked: only an implied, unobserved flush.
        CHECK(p.ObserveF14SuppressionDecision(kOther, kB, kIncB, true, P::VisibilityPath::Batched,
                                              Proven(kOther, kB, kIncB)) == P::Outcome::SuppressionRecorded);
        e = p.Find(kOther, kB);
        CHECK(e.suppressServer == P::ServerStage::UnlinkedByF14);
        CHECK(e.suppressPacket == P::PacketStage::ImpliedPendingBatchFlush);

        // Never linked: create withheld, no packet at all.
        CHECK(p.ObserveF14SuppressionDecision(kViewer, kC, kIncC, false, P::VisibilityPath::Batched,
                                              Proven(kViewer, kC, kIncC)) == P::Outcome::SuppressionRecorded);
        e = p.Find(kViewer, kC);
        CHECK(e.suppressServer == P::ServerStage::CreateWithheldByF14);
        CHECK(e.suppressPacket == P::PacketStage::None);

        // Repeated passes (now unlinked) never downgrade the first evidence.
        CHECK(p.ObserveF14SuppressionDecision(kViewer, kA, kIncA, false, P::VisibilityPath::Batched,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::DuplicateSuppression);
        e = p.Find(kViewer, kA);
        CHECK(e.suppressServer == P::ServerStage::UnlinkedByF14);
        CHECK(e.suppressPacket == P::PacketStage::HandedToSessionDirect);
        CHECK(p.Duplicates() == 1);
        CHECK(p.Size() == 3 && p.LedgerView().Size() == 3);
        CHECK(!p.GlobalRestoreRequired());
        CHECK(P::ClientStageOf() == P::ClientStage::NotServerObservable);
    }

    void TestOrdinaryLinkChangesAreNeverEvidence()
    {
        Proto p;
        // Phase, distance, GM, despawn, map change, out-of-sight sweep: all
        // arrive as generic transitions for pairs F14 never suppressed.
        for (int linked = 0; linked <= 1; ++linked)
            for (int path = 0; path <= 1; ++path)
                CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA, linked != 0,
                                              path ? P::VisibilityPath::Batched : P::VisibilityPath::Direct,
                                              Live(kViewer, kA, kIncA)) == P::Outcome::IgnoredOrdinary);
        CHECK(p.Size() == 0 && p.LedgerView().Size() == 0);
        CHECK(!p.GlobalRestoreRequired());
        CHECK(!p.Find(kViewer, kA).found);
        CHECK(p.MayAnswerPositiveSuppression(kViewer, kA, kIncA, Proven(kViewer, kA, kIncA)));

        // An ordinary unlink of an already F14-recorded pair changes nothing.
        CHECK(p.ObserveF14SuppressionDecision(kViewer, kA, kIncA, false, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA, false, P::VisibilityPath::Direct,
                                      Live(kViewer, kA, kIncA)) == P::Outcome::IgnoredOrdinary);
        P::Evidence const e = p.Find(kViewer, kA);
        CHECK(e.suppressServer == P::ServerStage::CreateWithheldByF14);
        CHECK(e.restoreServer == P::ServerStage::None);
        CHECK(!p.NeedsRestore(kViewer, kA));
    }

    void TestRestoreEvidenceNeverClearsTheLedger()
    {
        Proto p;
        CHECK(p.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        p.ObserveCreatureRetired(kA); // stop / death / corpse / respawn
        CHECK(p.NeedsRestore(kViewer, kA));
        CHECK(!p.MayAnswerPositiveSuppression(kOther, kB, kIncB, Proven(kOther, kB, kIncB)));

        // Relogged viewer: the old session's obligation is not acknowledged.
        CHECK(p.ObserveLinkTransition(kViewerRelog, kA, kIncA, true, P::VisibilityPath::Direct,
                                      Live(kViewerRelog, kA, kIncA)) == P::Outcome::IgnoredStale);
        // Event captured for the old epoch but the live session moved on.
        CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                      Live(kViewerRelog, kA, kIncA)) == P::Outcome::IgnoredStale);
        // Creature GUID reused by a new incarnation.
        CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA + 1, true, P::VisibilityPath::Direct,
                                      Live(kViewer, kA, kIncA + 1)) == P::Outcome::IgnoredRecycled);
        CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                      Live(kViewer, kA, kIncA + 1)) == P::Outcome::IgnoredRecycled);
        // Different map/instance thread.
        P::TransitionFacts wrong = Live(kViewer, kA, kIncA);
        wrong.eventInstanceId = 9;
        CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA, true, P::VisibilityPath::Direct, wrong) ==
              P::Outcome::IgnoredWrongMap);
        CHECK(p.Find(kViewer, kA).restoreServer == P::ServerStage::None);

        // Exact session + incarnation + map: restore server evidence only.
        CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                      Live(kViewer, kA, kIncA)) == P::Outcome::RestoreServerEvidence);
        P::Evidence const e = p.Find(kViewer, kA);
        CHECK(e.restoreServer == P::ServerStage::RelinkedAfterF14);
        CHECK(e.restorePacket == P::PacketStage::HandedToSessionDirect);
        // Still owed: only separately verified client proof could complete it.
        CHECK(p.NeedsRestore(kViewer, kA));
        CHECK(p.LedgerView().Size() == 1);
        CHECK(p.LedgerView().PendingRestores() == 1);
        CHECK(!p.MayAnswerPositiveSuppression(kViewer, kA, kIncA, Proven(kViewer, kA, kIncA)));
    }

    void TestBatchedNeverReachesHandedWithoutProposedHook()
    {
        Proto p;
        CHECK(p.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Batched,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        p.ObserveCreatureRetired(kA);
        CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA, true, P::VisibilityPath::Batched,
                                      Live(kViewer, kA, kIncA)) == P::Outcome::RestoreServerEvidence);
        P::Evidence e = p.Find(kViewer, kA);
        CHECK(e.suppressPacket == P::PacketStage::ImpliedPendingBatchFlush);
        CHECK(e.restorePacket == P::PacketStage::ImpliedPendingBatchFlush);

        // Only the PROPOSED flush hook (absent at the pinned Core) upgrades it.
        CHECK(p.ObserveProposedBatchFlush(kViewer, kA, kIncA + 1, false) == P::Outcome::IgnoredStale);
        CHECK(p.ObserveProposedBatchFlush(kViewerRelog, kA, kIncA, false) == P::Outcome::IgnoredStale);
        CHECK(p.ObserveProposedBatchFlush(kViewer, kA, kIncA, false) == P::Outcome::BatchFlushObserved);
        CHECK(p.ObserveProposedBatchFlush(kViewer, kA, kIncA, false) == P::Outcome::IgnoredOrdinary);
        CHECK(p.ObserveProposedBatchFlush(kViewer, kA, kIncA, true) == P::Outcome::BatchFlushObserved);
        e = p.Find(kViewer, kA);
        CHECK(e.suppressPacket == P::PacketStage::HandedToSessionBatched);
        CHECK(e.restorePacket == P::PacketStage::HandedToSessionBatched);
        CHECK(p.NeedsRestore(kViewer, kA)); // handed to session is not client proof
        CHECK(p.LedgerView().Size() == 1);

        // A direct-path stage is never relabelled by a batch flush.
        Proto d;
        CHECK(d.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        CHECK(d.ObserveProposedBatchFlush(kViewer, kA, kIncA, false) == P::Outcome::IgnoredOrdinary);
        CHECK(d.Find(kViewer, kA).suppressPacket == P::PacketStage::HandedToSessionDirect);
    }

    void TestUnexpectedRelinkWhileSuppressedBecomesObligation()
    {
        Proto p;
        CHECK(p.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        CHECK(!p.NeedsRestore(kViewer, kA));
        CHECK(p.ObserveLinkTransition(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                      Live(kViewer, kA, kIncA)) == P::Outcome::RestoreServerEvidence);
        CHECK(p.NeedsRestore(kViewer, kA));
        CHECK(p.LedgerView().Size() == 1);
    }

    void TestLifecycleInvalidationsPreserveObligations()
    {
        // Map unload revokes only that map/instance.
        Proto m;
        CHECK(m.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        CHECK(m.ObserveF14SuppressionDecision(kViewer, kB, kIncB, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kB, kIncB)) == P::Outcome::SuppressionRecorded);
        m.ObserveMapUnloaded(kAOtherInstance.mapId, kAOtherInstance.instanceId);
        CHECK(!m.NeedsRestore(kViewer, kA) && !m.NeedsRestore(kViewer, kB));
        m.ObserveMapUnloaded(kA.mapId, kA.instanceId);
        CHECK(m.NeedsRestore(kViewer, kA));
        CHECK(!m.NeedsRestore(kViewer, kB));
        CHECK(m.LedgerView().Size() == 2);

        // Relog: every epoch of the viewer is owed; the new session cannot hide.
        Proto r;
        CHECK(r.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        r.ObserveViewerSessionEnded(kViewer.guidRaw);
        CHECK(r.NeedsRestore(kViewer, kA));
        CHECK(r.ObserveF14SuppressionDecision(kViewerRelog, kB, kIncB, true, P::VisibilityPath::Direct,
                                              Proven(kViewerRelog, kB, kIncB)) ==
              P::Outcome::FailVisibleGlobalRestore);
        CHECK(r.Failures(P::FailReason::LedgerRefused) == 1);
        CHECK(r.NeedsRestore(kViewer, kA));
        CHECK(r.LedgerView().Size() == 1);

        // Viewer era/rights change.
        Proto e;
        CHECK(e.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        e.ObserveViewerEraOrRightsChanged(kOther.guidRaw);
        CHECK(!e.NeedsRestore(kViewer, kA));
        e.ObserveViewerEraOrRightsChanged(kViewer.guidRaw);
        CHECK(e.NeedsRestore(kViewer, kA));

        // Config OFF: global restore, entries kept.
        Proto c;
        CHECK(c.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        CHECK(c.ObserveF14SuppressionDecision(kOther, kB, kIncB, false, P::VisibilityPath::Batched,
                                              Proven(kOther, kB, kIncB)) == P::Outcome::SuppressionRecorded);
        c.ObserveConfigDisabled();
        CHECK(c.GlobalRestoreRequired());
        CHECK(c.NeedsRestore(kViewer, kA) && c.NeedsRestore(kOther, kB));
        CHECK(c.LedgerView().Size() == 2 && c.LedgerView().PendingRestores() == 2);
        CHECK(!c.MayAnswerPositiveSuppression(kViewer, kC, kIncC, Proven(kViewer, kC, kIncC)));
    }

    void TestRecycledIncarnationAndCapacity()
    {
        // Same creature key, new incarnation while the old record is live.
        Proto g;
        CHECK(g.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        CHECK(g.ObserveF14SuppressionDecision(kViewer, kA, kIncA + 1, true, P::VisibilityPath::Direct,
                                              Proven(kViewer, kA, kIncA + 1)) ==
              P::Outcome::FailVisibleGlobalRestore);
        CHECK(g.Failures(P::FailReason::LedgerRefused) == 1);
        CHECK(g.Find(kViewer, kA).incarnation == kIncA); // old record not overwritten
        CHECK(g.NeedsRestore(kViewer, kA));
        CHECK(g.Size() == 1 && g.LedgerView().Size() == 1);

        // Capacity: no eviction, every old pair still owed.
        P::Protocol<2> small;
        CHECK(small.ObserveF14SuppressionDecision(kViewer, kA, kIncA, true, P::VisibilityPath::Direct,
                                                  Proven(kViewer, kA, kIncA)) == P::Outcome::SuppressionRecorded);
        CHECK(small.ObserveF14SuppressionDecision(kViewer, kB, kIncB, true, P::VisibilityPath::Direct,
                                                  Proven(kViewer, kB, kIncB)) == P::Outcome::SuppressionRecorded);
        CHECK(!small.MayAnswerPositiveSuppression(kViewer, kC, kIncC, Proven(kViewer, kC, kIncC)));
        CHECK(small.ObserveF14SuppressionDecision(kViewer, kC, kIncC, true, P::VisibilityPath::Direct,
                                                  Proven(kViewer, kC, kIncC)) ==
              P::Outcome::FailVisibleGlobalRestore);
        CHECK(small.Size() == 2 && small.LedgerView().Size() == 2);
        CHECK(small.LedgerView().IsPoisoned());
        CHECK(small.NeedsRestore(kViewer, kA) && small.NeedsRestore(kViewer, kB));
        CHECK(small.Find(kViewer, kA).found && small.Find(kViewer, kB).found);
        CHECK(small.GlobalRestoreRequired());
    }

    // Long mixed sequence: the ledger never shrinks and the sticky risk
    // never clears through any protocol API.
    void TestMonotonicObligations()
    {
        P::Protocol<8> p;
        std::size_t lastSize = 0;
        bool sawRisk = false;
        for (std::uint64_t step = 1; step <= 400; ++step)
        {
            P::ViewerKey const v{100 + step % 3, 10 + step % 2};
            P::CreatureKey const c{static_cast<std::uint32_t>(step % 2), 0, 900 + step % 5};
            std::uint64_t const inc = 5000 + step % 4;
            P::DecisionFacts f = Proven(v, c, inc);
            if (step % 7 == 0)
                f.pairRightsProven = false;
            switch (step % 6)
            {
                case 0: p.ObserveCreatureRetired(c); break;
                case 1: p.ObserveF14SuppressionDecision(v, c, inc, step % 2 == 0,
                                                        P::VisibilityPath::Batched, f); break;
                case 2: p.ObserveLinkTransition(v, c, inc, true, P::VisibilityPath::Direct,
                                                Live(v, c, inc)); break;
                case 3: p.ObserveProposedBatchFlush(v, c, inc, step % 2 == 0); break;
                case 4: p.ObserveViewerSessionEnded(v.guidRaw); break;
                default: p.ObserveMapUnloaded(c.mapId, c.instanceId); break;
            }
            CHECK(p.LedgerView().Size() >= lastSize);
            CHECK(p.Size() == p.LedgerView().Size());
            lastSize = p.LedgerView().Size();
            sawRisk = sawRisk || p.UnattributedHideRisk();
            if (sawRisk)
                CHECK(p.UnattributedHideRisk() && p.GlobalRestoreRequired());
        }
        CHECK(sawRisk);
        CHECK(lastSize > 0);
    }
}

int main()
{
    TestDefaultFactsFailVisible();
    TestEachMissingFactFailsVisible();
    TestSuppressionStagesStaySeparate();
    TestOrdinaryLinkChangesAreNeverEvidence();
    TestRestoreEvidenceNeverClearsTheLedger();
    TestBatchedNeverReachesHandedWithoutProposedHook();
    TestUnexpectedRelinkWhileSuppressedBecomesObligation();
    TestLifecycleInvalidationsPreserveObligations();
    TestRecycledIncarnationAndCapacity();
    TestMonotonicObligations();
    std::printf("F14 VIEWER RESTORE PROTOCOL %u/%u passed; NO PACKET OR CLIENT PROOF\n",
                g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
