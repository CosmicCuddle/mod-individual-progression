#include "IndividualProgressionF14OwnedRestoreShards.h"
#include <cstdlib>
#include <iostream>
namespace F = IndividualProgressionF14ViewerRestoreProtocol;
using S = IndividualProgressionF14OwnedRestoreShards::Store<2, 2>;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)
F::DecisionFacts Facts()
{
    F::DecisionFacts f;
    f.configEnabled = f.actorPositivelyVerifiedRandomBot = f.actorEntitled =
        f.pairRightsProven = f.creatureAlive = true;
    f.currentViewerEpoch = 7;
    f.currentCreatureIncarnation = 11;
    f.eventMapId = 1; f.eventInstanceId = 2;
    return f;
}
int main()
{
    F::ViewerKey v{100,7};
    F::CreatureKey c{1,2,200}, d{1,2,201}, e{1,2,202}, other{1,3,200};
    auto f = Facts();
    static_assert(S::MaxPayloadBytes == 2 * sizeof(F::Protocol<2>), "whole payload budget");
    {
        S s;
        auto a = s.Open(1,2), b = s.Open(1,3);
        CHECK(a && b && !s.Open(1,2) && !s.Open(2,4));
        CHECK(!s.Reserve(a,v,other,11,f));
        auto r = s.Reserve(a,v,c,11,f);
        CHECK(r && !s.Reserve(a,v,d,11,f));
        CHECK(s.Commit(r,v,c,11,true,F::VisibilityPath::Batched,f) == S::CommitResult::Recorded);
        CHECK(!s.Reserve(a,v,c,11,f));
        auto r2 = s.Reserve(a,v,d,11,f);
        CHECK(r2 && s.Commit(r2,v,d,11,false,F::VisibilityPath::Direct,f) == S::CommitResult::Recorded);
        CHECK(!s.Reserve(a,v,e,11,f));
        CHECK(s.RevokeCreature(a,c));
        F::TransitionFacts t{7,11,1,2};
        CHECK(s.ObserveLink(a,v,c,11,true,F::VisibilityPath::Direct,t) == F::Outcome::RestoreServerEvidence);
        CHECK(s.Seal(a,true) == S::SealResult::Retained);
        auto status = s.Inspect(a);
        CHECK(status.found && status.retained && status.records == 2 && status.ledgerPairs == 2);
        CHECK(status.pendingRestores == 2);
        auto evidence = s.ReadEvidence(a,v,c);
        CHECK(evidence.found && evidence.suppressPacket == F::PacketStage::ImpliedPendingBatchFlush);
        CHECK(evidence.restorePacket == F::PacketStage::HandedToSessionDirect);
        CHECK(s.ReadEvidence(a,v,d).suppressServer == F::ServerStage::CreateWithheldByF14);
        CHECK(!s.RevokeCreature(a,c));
        CHECK(s.ObserveLink(a,v,c,11,true,F::VisibilityPath::Direct,t) == F::Outcome::IgnoredWrongMap);
        CHECK(s.Inspect().fenced && s.Inspect().retained == 1 && s.Inspect().occupied == 2);
        CHECK(!s.Open(3,4));
        auto fb = f; fb.eventInstanceId = 3;
        CHECK(!s.Reserve(b,v,other,11,fb));
    }
    {
        S s;
        auto a = s.Open(1,2);
        auto r = s.Reserve(a,v,c,11,f);
        CHECK(r && s.CancelBeforeEffect(r));
        CHECK(!s.CancelBeforeEffect(r));
        CHECK(s.Seal(a,true) == S::SealResult::CleanReleased);
        auto b = s.Open(1,2);
        CHECK(b && b.generation != a.generation && !s.Inspect(a).found);
        CHECK(!s.CancelBeforeEffect(r));
        CHECK(s.Commit(r,v,c,11,true,F::VisibilityPath::Direct,f) == S::CommitResult::Rejected);
        CHECK(s.Inspect().fenced && s.Inspect(b).records == 0);
    }
    {
        S s;
        auto a = s.Open(1,2);
        auto r = s.Reserve(a,v,c,11,f);
        CHECK(r && s.Seal(a,true) == S::SealResult::Retained);
        auto state = s.Inspect(a);
        CHECK(state.retained && state.uncertain && state.pending && state.reservedCreature == c);
        CHECK(state.reservedViewer == v && state.reservedIncarnation == 11);
        CHECK(!s.CancelBeforeEffect(r));
        CHECK(s.Commit(r,v,c,11,true,F::VisibilityPath::Direct,f) == S::CommitResult::Rejected);
        CHECK(s.Inspect().inFlight == 1 && s.Inspect().fenced);
    }
    {
        S s;
        auto a = s.Open(1,2);
        auto r = s.Reserve(a,v,c,11,f);
        CHECK(r && s.RevokeViewer(a,v.guidRaw));
        CHECK(s.Commit(r,v,c,11,true,F::VisibilityPath::Direct,f) == S::CommitResult::Uncertain);
        CHECK(!s.CancelBeforeEffect(r));
        CHECK(s.Seal(a,true) == S::SealResult::Retained);
        CHECK(s.Inspect(a).uncertain && s.Inspect(a).frozen && s.Inspect(a).reservedCreature == c);
    }
    {
        S s;
        auto a = s.Open(1,2);
        auto r = s.Reserve(a,v,c,11,f);
        CHECK(r);
        auto invalid = f; invalid.currentViewerEpoch = 9;
        CHECK(s.Commit(r,v,c,11,true,F::VisibilityPath::Direct,invalid) == S::CommitResult::Uncertain);
        CHECK(s.Seal(a,true) == S::SealResult::Retained);
        CHECK(s.Inspect(a).unattributed && s.Inspect().fenced);
    }
    {
        S s;
        auto a = s.Open(1,2);
        auto r = s.Reserve(a,v,c,11,f);
        CHECK(r);
        CHECK(s.Commit(r,v,d,11,true,F::VisibilityPath::Direct,f) == S::CommitResult::Uncertain);
        CHECK(s.Seal(a,true) == S::SealResult::Retained);
        CHECK(s.Inspect(a).reservedCreature == c && !s.ReadEvidence(a,v,d).found);
    }
    {
        S s;
        auto a = s.Open(1,2);
        CHECK(s.Seal(a,false) == S::SealResult::Retained);
        CHECK(s.Inspect(a).uncertain && s.Inspect().fenced);
    }
    std::cout << "PASS whole-shard ownership, exact evidence, immutable retention, stale and uncertain decisions\n";
}
