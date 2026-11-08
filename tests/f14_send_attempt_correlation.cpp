// Native tests for the pure F14 send-attempt correlation model (E1).
// GCC/Clang, C++17/C++20, with and without NDEBUG. Every positive below is
// SYNTHETIC: no Core pass-token producer exists, nothing is wired, and a
// correlated send CALL is never transmission, presentation or restoration.
#include "IndividualProgressionF14SendAttemptCorrelation.h"
#include "IndividualProgressionF14GuidSendAttemptEvidence.h"

#include <cstdint>
#include <cstdio>
#include <limits>

namespace C = IndividualProgressionF14SendAttemptCorrelation;
namespace P = IndividualProgressionF14ViewerRestoreProtocol;
namespace A = IndividualProgressionF14GuidSendAttemptEvidence;

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

static_assert(!C::IsSendCallClientPresentation(), "a send call is never client presentation");
static_assert(!C::HasCorePassTokenProducer(), "no Core pass-token producer exists at the pin");

namespace
{
    P::ViewerKey const kViewer{100, 10};
    P::ViewerKey const kRelog{100, 11};
    P::ViewerKey const kOther{200, 20};
    P::CreatureKey const kA{1, 2, 901};
    P::CreatureKey const kAOtherInstance{1, 3, 901}; // same raw GUID, other instance
    P::CreatureKey const kB{1, 2, 902};
    std::uint64_t const kInc = 9001;
    std::uint64_t const kPass = 50;

    using Table = C::Table<8, 4>;
    using Out = C::Direction;

    C::NominationFacts Facts(P::ViewerKey v, P::CreatureKey c, std::uint64_t inc, bool owed = false)
    {
        C::NominationFacts f;
        f.currentViewerEpoch = v.loginEpoch;
        f.currentCreatureIncarnation = inc;
        f.eventMapId = c.mapId;
        f.eventInstanceId = c.instanceId;
        f.globalRestoreRequired = false;
        f.priorF14SuppressionRecorded = true;
        f.restoreOwed = owed;
        return f;
    }

    C::SendFacts Send(P::ViewerKey v, P::CreatureKey c, Out d, std::uint64_t token)
    {
        C::SendFacts f;
        f.sourceAttributedByExactCoreF14Hook = true;
        f.viewerGuidRaw = v.guidRaw;
        f.currentViewerEpoch = v.loginEpoch;
        f.creatureGuidRaw = c.guidRaw;
        f.eventMapId = c.mapId;
        f.eventInstanceId = c.instanceId;
        f.direction = d;
        f.passToken = token;
        f.globalRestoreRequired = false;
        return f;
    }

    void TestDefaultsFailClosed()
    {
        Table t;
        C::NominationFacts const none{};
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, none) == C::Result::StaleEpoch);
        C::SendFacts const nothing{};
        CHECK(t.Correlate(nothing).result == C::Result::Unattributed);
        C::NominationFacts f = Facts(kViewer, kA, kInc);
        f.globalRestoreRequired = true; // default value of the field
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, f) == C::Result::GlobalFreeze);
        f = Facts(kViewer, kA, kInc);
        f.priorF14SuppressionRecorded = false; // default value of the field
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, f) == C::Result::NoPriorF14Suppression);
        CHECK(t.Candidates() == 0 && t.Viewers() == 0);
    }

    void TestKeyAndTokenValidation()
    {
        Table t;
        CHECK(t.Nominate({}, kA, kInc, Out::OutOfRange, kPass, Facts({}, kA, kInc)) == C::Result::InvalidKey);
        CHECK(t.Nominate(kViewer, {}, kInc, Out::OutOfRange, kPass, Facts(kViewer, {}, kInc)) == C::Result::InvalidKey);
        CHECK(t.Nominate(kViewer, kA, 0, Out::OutOfRange, kPass, Facts(kViewer, kA, 0)) == C::Result::InvalidKey);
        CHECK(t.Nominate(kViewer, kA, kInc, static_cast<Out>(7), kPass, Facts(kViewer, kA, kInc)) == C::Result::InvalidKey);
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, 0, Facts(kViewer, kA, kInc)) == C::Result::ZeroToken);
        CHECK(t.ObserveSuccessorPass(kViewer, 0) == C::Result::ZeroToken);

        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        C::SendFacts s = Send(kViewer, kA, Out::OutOfRange, 0);
        CHECK(t.Correlate(s).result == C::Result::ZeroToken);
        s = Send(kViewer, kA, Out::OutOfRange, kPass);
        s.creatureGuidRaw = 0;
        CHECK(t.Correlate(s).result == C::Result::InvalidKey);
        s = Send(kViewer, kA, Out::OutOfRange, kPass);
        s.sourceAttributedByExactCoreF14Hook = false; // ordinary Core out-of-range
        CHECK(t.Correlate(s).result == C::Result::Unattributed);
        CHECK(t.Candidates() == 1); // refusals before matching consume nothing
    }

    void TestNominationFactsReject()
    {
        Table t;
        C::NominationFacts f = Facts(kViewer, kA, kInc);
        f.currentViewerEpoch = kRelog.loginEpoch;
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, f) == C::Result::StaleEpoch);
        f = Facts(kViewer, kA, kInc);
        f.currentCreatureIncarnation = kInc + 1; // recycled GUID / changed incarnation
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, f) == C::Result::RecycledIncarnation);
        f = Facts(kViewer, kA, kInc);
        f.eventInstanceId = 3; // not the creature's map/instance
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, f) == C::Result::CrossMap);
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc, true)) ==
              C::Result::RestoreOwedForOut);
        CHECK(t.Nominate(kViewer, kA, kInc, Out::Create, kPass, Facts(kViewer, kA, kInc, false)) ==
              C::Result::RestoreNotOwedForCreate);
        CHECK(t.Candidates() == 0 && t.Viewers() == 0);
        CHECK(t.Count(C::Result::RestoreOwedForOut) == 1);
    }

    void TestUniqueMatchIsConsumedOnce()
    {
        Table t;
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        // Repeated identical visit in the same pass is idempotent.
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::DuplicateIgnored);
        CHECK(t.Candidates() == 1);
        // Wrong direction does not consume the candidate.
        CHECK(t.Correlate(Send(kViewer, kA, Out::Create, kPass)).result == C::Result::WrongDirection);
        C::Correlation const m = t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass));
        CHECK(m.Matched());
        CHECK(m.viewer == kViewer && m.creature == kA && m.incarnation == kInc);
        CHECK(m.direction == Out::OutOfRange && m.passToken == kPass);
        CHECK(t.Candidates() == 0);
        // A second send for the same slot has nothing left to match.
        CHECK(t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);
        // Another viewer / GUID never matches.
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(t.Correlate(Send(kOther, kA, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);
        CHECK(t.Correlate(Send(kViewer, kB, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);
        CHECK(t.Candidates() == 1);
    }

    void TestEpochAndSessionChanges()
    {
        Table t;
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        // Send observed with a newer live epoch: the recorded session is stale.
        CHECK(t.Correlate(Send(kRelog, kA, Out::OutOfRange, kPass)).result == C::Result::StaleEpoch);
        // A relogged session cannot nominate until the end is explicitly observed.
        CHECK(t.Nominate(kRelog, kB, kInc, Out::OutOfRange, kPass + 1, Facts(kRelog, kB, kInc)) ==
              C::Result::UnobservedSessionChange);
        t.ObserveViewerSessionEnded(kViewer.guidRaw);
        CHECK(t.Candidates() == 0 && t.Viewers() == 0);
        CHECK(t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);
        CHECK(t.Nominate(kRelog, kB, kInc, Out::OutOfRange, 1, Facts(kRelog, kB, kInc)) == C::Result::Nominated);
        CHECK(t.Correlate(Send(kRelog, kB, Out::OutOfRange, 1)).Matched());
    }

    void TestPassTokens()
    {
        // Stale send after the pass expired through an observed successor pass.
        Table t;
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(t.ObserveSuccessorPass(kViewer, kPass + 1) == C::Result::PassAccepted);
        CHECK(t.Candidates() == 0);
        CHECK(t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::StaleToken);
        // A send claiming a newer, never-nominated pass matches nothing and
        // does not advance the pass.
        CHECK(t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass + 9)).result == C::Result::NoCandidate);
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass + 1, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass + 1)).Matched());

        // Nominating with a newer token is itself an observed successor pass.
        Table n;
        CHECK(n.Nominate(kViewer, kA, kInc, Out::OutOfRange, 5, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(n.Nominate(kViewer, kB, kInc, Out::OutOfRange, 6, Facts(kViewer, kB, kInc)) == C::Result::Nominated);
        CHECK(n.Candidates() == 1);
        CHECK(n.Correlate(Send(kViewer, kA, Out::OutOfRange, 5)).result == C::Result::StaleToken);

        // Reused / reordered token poisons only that viewer session.
        Table r;
        CHECK(r.Nominate(kViewer, kA, kInc, Out::OutOfRange, 7, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(r.Nominate(kOther, kB, kInc, Out::OutOfRange, 3, Facts(kOther, kB, kInc)) == C::Result::Nominated);
        CHECK(r.Nominate(kViewer, kB, kInc, Out::OutOfRange, 6, Facts(kViewer, kB, kInc)) ==
              C::Result::TokenReorderedOrReused);
        CHECK(r.Correlate(Send(kViewer, kA, Out::OutOfRange, 7)).result == C::Result::ViewerPoisoned);
        CHECK(r.Nominate(kViewer, kA, kInc, Out::OutOfRange, 8, Facts(kViewer, kA, kInc)) == C::Result::ViewerPoisoned);
        CHECK(r.ObserveSuccessorPass(kViewer, 9) == C::Result::ViewerPoisoned);
        CHECK(r.Correlate(Send(kOther, kB, Out::OutOfRange, 3)).Matched()); // bounded scope
        CHECK(!r.IsPoisoned());
        r.ObserveViewerSessionEnded(kViewer.guidRaw); // explicit end clears the scope
        CHECK(r.Nominate(kViewer, kA, kInc, Out::OutOfRange, 1, Facts(kViewer, kA, kInc)) == C::Result::Nominated);

        // Wraparound is a reordered token, never a new pass.
        Table w;
        std::uint64_t const top = std::numeric_limits<std::uint64_t>::max();
        CHECK(w.Nominate(kViewer, kA, kInc, Out::OutOfRange, top, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(w.ObserveSuccessorPass(kViewer, 1) == C::Result::TokenReorderedOrReused);
        CHECK(w.Candidates() == 0);
        CHECK(w.Correlate(Send(kViewer, kA, Out::OutOfRange, top)).result == C::Result::ViewerPoisoned);
        CHECK(w.Correlate(Send(kViewer, kA, Out::OutOfRange, 1)).result == C::Result::ViewerPoisoned);

        // Explicit pass end / cancellation drops without poisoning.
        Table e;
        CHECK(e.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        e.ObservePassEnded(kViewer, kPass);
        CHECK(e.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);
        CHECK(e.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        e.CancelPass(kViewer, kPass);
        CHECK(e.Candidates() == 0);
    }

    void TestAmbiguityAndCrossMap()
    {
        // Two same-GUID candidates in different instances, same pass: both fail.
        Table t;
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(t.Nominate(kViewer, kAOtherInstance, kInc, Out::OutOfRange, kPass,
                         Facts(kViewer, kAOtherInstance, kInc)) == C::Result::Ambiguous);
        CHECK(t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::Ambiguous);
        CHECK(t.Candidates() == 0);
        CHECK(t.Correlate(Send(kViewer, kAOtherInstance, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);

        // Same slot, different incarnation (recycled GUID mid-pass): fail closed.
        Table g;
        CHECK(g.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(g.Nominate(kViewer, kA, kInc + 1, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc + 1)) == C::Result::Ambiguous);
        // Even an identical retry cannot rescue an ambiguous slot.
        CHECK(g.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Ambiguous);
        CHECK(g.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::Ambiguous);
        CHECK(g.Count(C::Result::Ambiguous) == 3);

        // Unresolved cross-map: the send happens on another map/instance.
        Table x;
        CHECK(x.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        C::SendFacts s = Send(kViewer, kA, Out::OutOfRange, kPass);
        s.eventInstanceId = 3;
        CHECK(x.Correlate(s).result == C::Result::CrossMap);
        CHECK(x.Candidates() == 0); // consumed: one attempt, one use
        CHECK(x.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);
    }

    void TestInvalidationsAndFreeze()
    {
        Table t;
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(t.Nominate(kViewer, kB, kInc, Out::OutOfRange, kPass, Facts(kViewer, kB, kInc)) == C::Result::Nominated);
        t.ObserveCreatureRetired(kA); // death / corpse / respawn / despawn
        CHECK(t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);
        t.ObserveMapUnloaded(kAOtherInstance.mapId, kAOtherInstance.instanceId);
        CHECK(t.Candidates() == 1);
        t.ObserveMapUnloaded(kB.mapId, kB.instanceId);
        CHECK(t.Candidates() == 0);

        // Rights / era revoked before the send.
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        t.ObserveViewerRightsOrEraChanged(kViewer.guidRaw);
        CHECK(t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::NoCandidate);

        // Global freeze refuses both directions at both ends.
        Table f;
        CHECK(f.Nominate(kViewer, kA, kInc, Out::Create, kPass, Facts(kViewer, kA, kInc, true)) == C::Result::Nominated);
        C::SendFacts s = Send(kViewer, kA, Out::Create, kPass);
        s.globalRestoreRequired = true;
        CHECK(f.Correlate(s).result == C::Result::GlobalFreeze);
        CHECK(f.Candidates() == 1); // nothing consumed, nothing upgraded
    }

    void TestOverflowPoisons()
    {
        C::Table<2, 4> c;
        CHECK(c.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(c.Nominate(kViewer, kB, kInc, Out::OutOfRange, kPass, Facts(kViewer, kB, kInc)) == C::Result::Nominated);
        P::CreatureKey const kC{1, 2, 903};
        CHECK(c.Nominate(kViewer, kC, kInc, Out::OutOfRange, kPass, Facts(kViewer, kC, kInc)) == C::Result::Capacity);
        CHECK(c.IsPoisoned() && c.Candidates() == 0);
        CHECK(c.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::TablePoisoned);
        CHECK(c.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::TablePoisoned);
        CHECK(c.ObserveSuccessorPass(kViewer, kPass + 1) == C::Result::TablePoisoned);
        CHECK(c.Count(C::Result::Capacity) == 1);

        C::Table<8, 1> v;
        CHECK(v.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, Facts(kViewer, kA, kInc)) == C::Result::Nominated);
        CHECK(v.Nominate(kOther, kB, kInc, Out::OutOfRange, kPass, Facts(kOther, kB, kInc)) == C::Result::Capacity);
        CHECK(v.IsPoisoned());
        CHECK(v.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass)).result == C::Result::TablePoisoned);
        CHECK(v.Count(C::Result::Capacity) == 1);
    }

    // End to end with the UNCHANGED #94 protocol and #103 adapter. The
    // ledger never shrinks, nothing is cleared, and only the send-call
    // packet stage can change.
    void TestComposesWithUnchangedProtocolAndAdapter()
    {
        P::DecisionFacts d;
        d.configEnabled = true;
        d.actorPositivelyVerifiedRandomBot = true;
        d.actorEntitled = true;
        d.pairRightsProven = true; // synthetic: the real rights policy is default false
        d.creatureAlive = true;
        d.currentViewerEpoch = kViewer.loginEpoch;
        d.currentCreatureIncarnation = kInc;
        d.eventMapId = kA.mapId;
        d.eventInstanceId = kA.instanceId;

        P::Protocol<4> p;
        Table t;
        // Stage 2 at the #86 decision (batched), then nominate in the same chain.
        CHECK(p.ObserveF14SuppressionDecision(kViewer, kA, kInc, true, P::VisibilityPath::Batched, d) ==
              P::Outcome::SuppressionRecorded);
        C::NominationFacts f = Facts(kViewer, kA, kInc);
        f.globalRestoreRequired = true;
        f.priorF14SuppressionRecorded = false;
        f.restoreOwed = true;
        C::ReadProtocolFacts(p, kViewer, kA, kInc, f);
        CHECK(!f.globalRestoreRequired && f.priorF14SuppressionRecorded && !f.restoreOwed);
        // A recycled incarnation, another viewer session or an unknown pair
        // is never "prior F14 suppression" for nomination.
        C::NominationFacts other = Facts(kViewer, kA, kInc + 1);
        C::ReadProtocolFacts(p, kViewer, kA, kInc + 1, other);
        CHECK(!other.priorF14SuppressionRecorded);
        CHECK(t.Nominate(kViewer, kA, kInc + 1, Out::OutOfRange, kPass, other) == C::Result::NoPriorF14Suppression);
        other = Facts(kRelog, kA, kInc);
        C::ReadProtocolFacts(p, kRelog, kA, kInc, other);
        CHECK(!other.priorF14SuppressionRecorded);
        other = Facts(kViewer, kB, kInc);
        C::ReadProtocolFacts(p, kViewer, kB, kInc, other);
        CHECK(!other.priorF14SuppressionRecorded);
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass, f) == C::Result::Nominated);

        // GUID-only tagged OUT attempt after SendDirectMessage.
        C::Correlation const m = t.Correlate(Send(kViewer, kA, Out::OutOfRange, kPass));
        CHECK(m.Matched());
        A::AttemptFacts a;
        a.sourceAttributedByExactCoreF14Hook = true;
        a.afterActualSessionSendDirectMessageCall = true;
        a.sameOwningMapThread = true;
        a.reportedGuidExactlyMatchesCreature = true;
        a.currentViewerLoginEpoch = m.viewer.loginEpoch;
        a.currentCreatureIncarnation = m.incarnation;
        a.eventMapId = m.creature.mapId;
        a.eventInstanceId = m.creature.instanceId;
        CHECK(A::ObserveExactTaggedBatchAttempt(p, m.viewer, m.creature, m.incarnation,
                                                A::Direction::OutOfRange, a) ==
              A::Outcome::ExactTaggedServerSendCallOnly);
        CHECK(p.Find(kViewer, kA).suppressPacket == P::PacketStage::HandedToSessionBatched);
        CHECK(p.LedgerView().Size() == 1 && !p.NeedsRestore(kViewer, kA));

        // Restore owed, batched re-link, CREATE nominate + correlate.
        p.ObserveCreatureRetired(kA);
        P::TransitionFacts tf;
        tf.currentViewerEpoch = kViewer.loginEpoch;
        tf.currentCreatureIncarnation = kInc;
        tf.eventMapId = kA.mapId;
        tf.eventInstanceId = kA.instanceId;
        CHECK(p.ObserveLinkTransition(kViewer, kA, kInc, true, P::VisibilityPath::Batched, tf) ==
              P::Outcome::RestoreServerEvidence);
        f = Facts(kViewer, kA, kInc);
        C::ReadProtocolFacts(p, kViewer, kA, kInc, f);
        CHECK(f.restoreOwed);
        CHECK(t.Nominate(kViewer, kA, kInc, Out::OutOfRange, kPass + 1, f) == C::Result::RestoreOwedForOut);
        CHECK(t.Nominate(kViewer, kA, kInc, Out::Create, kPass + 1, f) == C::Result::Nominated);
        C::Correlation const c = t.Correlate(Send(kViewer, kA, Out::Create, kPass + 1));
        CHECK(c.Matched());
        a.currentCreatureIncarnation = c.incarnation;
        CHECK(A::ObserveExactTaggedBatchAttempt(p, c.viewer, c.creature, c.incarnation,
                                                A::Direction::Create, a) ==
              A::Outcome::ExactTaggedServerSendCallOnly);
        CHECK(p.Find(kViewer, kA).restorePacket == P::PacketStage::HandedToSessionBatched);
        // Still owed: a send call is not client restoration.
        CHECK(p.NeedsRestore(kViewer, kA));
        CHECK(p.LedgerView().Size() == 1 && p.LedgerView().PendingRestores() == 1);

        // Global freeze (B4): nominations refused; #103 refuses too.
        p.ObserveConfigDisabled();
        f = Facts(kViewer, kB, kInc, true);
        C::ReadProtocolFacts(p, kViewer, kB, kInc, f);
        CHECK(f.globalRestoreRequired);
        CHECK(t.Nominate(kViewer, kB, kInc, Out::Create, kPass + 2, f) == C::Result::GlobalFreeze);
        CHECK(p.LedgerView().Size() == 1 && p.NeedsRestore(kViewer, kA));
    }

    // Long mixed sequence: a match is only ever returned for an identity
    // that was nominated in the viewer's current pass, and is used once.
    void TestRandomisedInvariants()
    {
        C::Table<16, 4> t;
        std::uint64_t seed = 0x9e3779b97f4a7c15ULL;
        auto next = [&]() { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed; };
        std::uint64_t token[3] = {1, 1, 1};
        unsigned matched = 0;
        for (int step = 0; step < 3000; ++step)
        {
            std::uint64_t const r = next();
            int const vi = static_cast<int>(r % 3);
            P::ViewerKey const v{300u + static_cast<std::uint64_t>(vi), 40};
            P::CreatureKey const c{1, static_cast<std::uint32_t>((r >> 8) % 2), 700 + (r >> 16) % 4};
            Out const d = ((r >> 24) & 1) ? Out::Create : Out::OutOfRange;
            switch ((r >> 32) % 6)
            {
                case 0:
                case 1:
                    t.Nominate(v, c, kInc, d, token[vi], Facts(v, c, kInc, d == Out::Create));
                    break;
                case 2:
                {
                    std::size_t const before = t.Candidates();
                    C::Correlation const m = t.Correlate(Send(v, c, d, token[vi]));
                    if (m.Matched())
                    {
                        ++matched;
                        CHECK(m.viewer == v && m.creature == c && m.incarnation == kInc && m.direction == d);
                        CHECK(t.Candidates() == before - 1);
                        CHECK(t.Correlate(Send(v, c, d, token[vi])).result != C::Result::Matched);
                    }
                    break;
                }
                case 3:
                    t.ObserveSuccessorPass(v, ++token[vi]);
                    break;
                case 4:
                    t.ObserveCreatureRetired(c);
                    break;
                default:
                    // A send for an expired pass never matches.
                    if (token[vi] > 1)
                        CHECK(t.Correlate(Send(v, c, d, token[vi] - 1)).result != C::Result::Matched);
                    break;
            }
            CHECK(t.Candidates() <= 16);
        }
        CHECK(matched > 0);
        CHECK(!t.IsPoisoned());
    }
}

int main()
{
    TestDefaultsFailClosed();
    TestKeyAndTokenValidation();
    TestNominationFactsReject();
    TestUniqueMatchIsConsumedOnce();
    TestEpochAndSessionChanges();
    TestPassTokens();
    TestAmbiguityAndCrossMap();
    TestInvalidationsAndFreeze();
    TestOverflowPoisons();
    TestComposesWithUnchangedProtocolAndAdapter();
    TestRandomisedInvariants();
    std::printf("F14 SEND ATTEMPT CORRELATION %u/%u passed; SYNTHETIC TOKENS, NO PACKET OR CLIENT PROOF\n",
                g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
