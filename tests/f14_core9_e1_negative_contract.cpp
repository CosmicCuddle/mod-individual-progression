// Core9-shaped F14 E1 failure-boundary model: no production listener or packet.
// Real Core9 provides post-send batch GUID + process-global pass token, but no
// verified human epoch, creature incarnation, prior-F14 nomination or client ACK.
// All correlations in this rehearsal MUST refuse/close; actual Core9 cannot
// supply positive provenance. The pure header WOULD match synthetic fabricated
// positive pre-send facts fed after a send. The protection is procedural and
// requires a real future Core producer; this test does not prove otherwise.
#include "IndividualProgressionF14SendAttemptCorrelation.h"
#include <cstdint>
#include <cstdio>

namespace C = IndividualProgressionF14SendAttemptCorrelation;
namespace P = IndividualProgressionF14ViewerRestoreProtocol;

static unsigned passed = 0;
static unsigned failed = 0;
#define CHECK(expr) do { if (expr) { ++passed; } else { ++failed; \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

static_assert(!C::IsSendCallClientPresentation(), "Core9 send-CALL != client presentation");

namespace
{
    P::CreatureKey const creature{1, 2, 555};
    P::ViewerKey const alice{101, 11};
    P::ViewerKey const bob{202, 22};
    using Table = C::Table<16, 256>;

    C::NominationFacts NoPrior(P::ViewerKey viewer)
    {
        C::NominationFacts f{};
        f.currentViewerEpoch = viewer.loginEpoch;
        f.currentCreatureIncarnation = 8;
        f.eventMapId = creature.mapId;
        f.eventInstanceId = creature.instanceId;
        f.globalRestoreRequired = false;
        f.priorF14SuppressionRecorded = false; // Core9 has NO positive F14 provider
        f.restoreOwed = false;
        return f;
    }

    // TEST-ONLY fabricated positive facts. Core9 cannot supply these honestly.
    // Use solely to prove closed/untagged routes refuse even with candidates.
    C::NominationFacts SyntheticPrior(P::ViewerKey viewer)
    {
        C::NominationFacts f = NoPrior(viewer);
        f.priorF14SuppressionRecorded = true;
        return f;
    }

    C::SendFacts Tagged(P::ViewerKey viewer, std::uint64_t token, bool batch = true)
    {
        C::SendFacts f{};
        f.sourceAttributedByExactCoreF14Hook = batch;
        f.viewerGuidRaw = viewer.guidRaw;
        f.currentViewerEpoch = viewer.loginEpoch;
        f.creatureGuidRaw = creature.guidRaw;
        f.eventMapId = creature.mapId;
        f.eventInstanceId = creature.instanceId;
        f.direction = C::Direction::OutOfRange;
        f.passToken = token;
        f.globalRestoreRequired = false;
        return f;
    }

    void NoHumanEpochCannotBecomeEvidence()
    {
        Table t;
        // Nonzero epochs below are synthetic values, NOT Core9 authentication.
        P::ViewerKey const unknownEpoch{alice.guidRaw, 0};
        CHECK(t.ObservePassEnded(unknownEpoch, 91) == C::Result::InvalidKey);
        CHECK(t.Nominate(unknownEpoch, creature, 8, C::Direction::OutOfRange, 91,
                         NoPrior(unknownEpoch)) == C::Result::InvalidKey);
        CHECK(t.Correlate(Tagged(unknownEpoch, 91)).result == C::Result::InvalidKey);
        CHECK(t.Viewers() == 0 && t.Candidates() == 0);
    }

    void NoPriorSuppressionCannotNominate()
    {
        Table t;
        CHECK(t.Nominate(alice, creature, 8, C::Direction::OutOfRange, 95,
                         NoPrior(alice)) == C::Result::NoPriorF14Suppression);
        CHECK(t.Correlate(Tagged(alice, 95)).result == C::Result::NoCandidate);
        CHECK(t.Viewers() == 0 && t.Candidates() == 0);
    }

    void PostSendGuidWithoutNominationFindsNothing()
    {
        Table t;
        // Core9's GUID event is already AFTER the real SendDirectMessage.
        // There was no earlier token+epoch+incarnation+F14-caused nomination.
        // This test does NOT prove the table rejects a fabricated post-send
        // Nominate: a matching synthetic prior=true nomination WOULD match.
        CHECK(t.Correlate(Tagged(alice, 100)).result == C::Result::NoCandidate);
        CHECK(t.ObservePassEnded(alice, 100) == C::Result::PassAccepted);
        CHECK(t.Correlate(Tagged(alice, 100)).result == C::Result::PassClosed);
        CHECK(t.Correlate(Tagged(alice, 101)).result == C::Result::NoCandidate);
        CHECK(t.Candidates() == 0);
    }

    void ImmediateDirectRouteHasNoBatchAttribution()
    {
        Table t;
        // Direct Player::UpdateVisibilityOf(WorldObject*) has no batch pass token.
        // Do not borrow a prior batch token or mark a normal Core send as F14.
        CHECK(t.Nominate(alice, creature, 8, C::Direction::OutOfRange, 55,
                         SyntheticPrior(alice)) == C::Result::Nominated);
        CHECK(t.Candidates() == 1);
        CHECK(t.Correlate(Tagged(alice, 55, false)).result == C::Result::Unattributed);
        CHECK(t.Candidates() == 1); // direct refusal did NOT consume batch candidate
        CHECK(t.Count(C::Result::Matched) == 0);
    }

    void GlobalTokensAcrossViewersStayUnmatched()
    {
        Table t;
        // One process-global Core9 token sequence: not per-viewer or contiguous.
        CHECK(t.Nominate(alice, creature, 8, C::Direction::OutOfRange, 300,
                         SyntheticPrior(alice)) == C::Result::Nominated);
        CHECK(t.ObservePassEnded(bob, 301) == C::Result::PassAccepted);
        CHECK(t.ObservePassEnded(alice, 307) == C::Result::PassAccepted);
        CHECK(t.Correlate(Tagged(alice, 300)).result == C::Result::StaleToken);
        CHECK(t.Correlate(Tagged(alice, 307)).result == C::Result::PassClosed);
        CHECK(t.Correlate(Tagged(bob, 301)).result == C::Result::PassClosed);
        CHECK(!t.IsPoisoned() && t.Candidates() == 0);
        CHECK(t.Count(C::Result::Matched) == 0);
    }

    void NestedPassIsFailClosedRatherThanProof()
    {
        Table t;
        // Hypothetical nested same-viewer notifier; preserve no-evidence result.
        CHECK(t.Nominate(alice, creature, 8, C::Direction::OutOfRange, 400,
                         SyntheticPrior(alice)) == C::Result::Nominated);
        CHECK(t.ObserveSuccessorPass(alice, 401) == C::Result::PassAccepted);
        CHECK(t.ObservePassEnded(alice, 401) == C::Result::PassAccepted);
        CHECK(t.ObservePassEnded(alice, 400) == C::Result::StaleToken);
        CHECK(t.Correlate(Tagged(alice, 400)).result == C::Result::StaleToken);
        CHECK(t.Correlate(Tagged(alice, 401)).result == C::Result::PassClosed);
        CHECK(t.Candidates() == 0 && !t.IsPoisoned());
        CHECK(t.Count(C::Result::Matched) == 0);
    }

    void UnknownEndCapacityPoisonsButNeverMatches()
    {
        Table t;
        for (std::uint64_t i = 0; i < 256; ++i)
        {
            P::ViewerKey const viewer{10000 + i, 100 + i};
            CHECK(t.ObservePassEnded(viewer, 1000 + i) == C::Result::PassAccepted);
        }
        CHECK(t.Viewers() == 256 && t.Candidates() == 0);
        P::ViewerKey const last{99999, 500};
        CHECK(t.ObservePassEnded(last, 2000) == C::Result::Capacity);
        CHECK(t.IsPoisoned());
        CHECK(t.Correlate(Tagged(alice, 2000)).result == C::Result::TablePoisoned);
        t.ObserveViewerSessionEnded(10000); // cannot unpoison the whole table
        CHECK(t.ObservePassEnded(alice, 3000) == C::Result::TablePoisoned);
        CHECK(t.IsPoisoned() && t.Candidates() == 0);
    }
}

int main()
{
    // Read-only bridge exercise only: protocol never enters Table as a mutable
    // dependency. This does NOT establish actual Core9 ledger integration.
    P::Protocol<16> debt;
    std::size_t const beforeSize = debt.Size();
    std::size_t const beforeLedger = debt.LedgerView().Size();
    std::size_t const beforePending = debt.LedgerView().PendingRestores();
    bool const beforeGlobal = debt.GlobalRestoreRequired();
    C::NominationFacts protocolFacts{};
    protocolFacts.currentViewerEpoch = alice.loginEpoch;
    protocolFacts.currentCreatureIncarnation = 8;
    protocolFacts.eventMapId = creature.mapId;
    protocolFacts.eventInstanceId = creature.instanceId;
    C::ReadProtocolFacts(debt, alice, creature, 8, protocolFacts);
    CHECK(!protocolFacts.priorF14SuppressionRecorded);
    CHECK(!protocolFacts.restoreOwed);
    CHECK(protocolFacts.globalRestoreRequired == beforeGlobal);
    Table constFactsTable;
    CHECK(constFactsTable.Nominate(alice, creature, 8, C::Direction::OutOfRange,
                                  700, protocolFacts) == C::Result::NoPriorF14Suppression);
    CHECK(constFactsTable.Count(C::Result::Matched) == 0);
    NoHumanEpochCannotBecomeEvidence();
    NoPriorSuppressionCannotNominate();
    PostSendGuidWithoutNominationFindsNothing();
    ImmediateDirectRouteHasNoBatchAttribution();
    GlobalTokensAcrossViewersStayUnmatched();
    NestedPassIsFailClosedRatherThanProof();
    UnknownEndCapacityPoisonsButNeverMatches();
    CHECK(debt.Size() == beforeSize);
    CHECK(debt.LedgerView().Size() == beforeLedger);
    CHECK(debt.LedgerView().PendingRestores() == beforePending);
    CHECK(debt.GlobalRestoreRequired() == beforeGlobal);
    if (failed == 0)
        std::printf("F14 CORE9 E1 NEGATIVE CONTRACT %u/%u passed; ZERO SYNTHETIC MATCHES\n",
                    passed, passed + failed);
    else
        std::printf("F14 CORE9 E1 NEGATIVE CONTRACT FAILED %u/%u\n", passed, passed + failed);
    return failed ? 1 : 0;
}
