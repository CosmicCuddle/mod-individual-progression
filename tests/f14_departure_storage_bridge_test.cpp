#include "IndividualProgressionF14OwnedRestoreShards.h"
#include <cstdlib>
#include <iostream>
namespace V = IndividualProgressionF14ViewerRestoreProtocol;
using S = IndividualProgressionF14OwnedRestoreShards::Store<3,4>;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while(false)
V::DecisionFacts Facts(V::ViewerKey viewer, V::CreatureKey creature)
{
    V::DecisionFacts f;
    f.configEnabled = f.actorPositivelyVerifiedRandomBot = f.actorEntitled =
        f.pairRightsProven = f.creatureAlive = true;
    f.currentViewerEpoch = viewer.loginEpoch;
    f.currentCreatureIncarnation = 11;
    f.eventMapId = creature.mapId; f.eventInstanceId = creature.instanceId;
    return f;
}
int main()
{
    V::ViewerKey v{100,7}, old{100,6}, other{101,8};
    V::CreatureKey a{1,2,200}, b{1,3,201}, c{1,2,202}, d{1,4,203};
    {
        S s;
        CHECK(!s.BeforeViewerDeparture(100) && !s.BeforeViewerDeparture(0));
        CHECK(s.Inspect().occupied == 0 && !s.Inspect().fenced);
        auto m = s.Open(1,2), n = s.Open(1,3), q = s.Open(1,4);
        CHECK(m && n && q);
        auto r = s.Reserve(m,v,a,11,Facts(v,a));
        CHECK(r && s.Commit(r,v,a,11,true,V::VisibilityPath::Direct,Facts(v,a)) == S::CommitResult::Recorded);
        r = s.Reserve(n,old,b,11,Facts(old,b));
        CHECK(r && s.Commit(r,old,b,11,true,V::VisibilityPath::Batched,Facts(old,b)) == S::CommitResult::Recorded);
        r = s.Reserve(m,other,c,11,Facts(other,c));
        CHECK(r && s.Commit(r,other,c,11,true,V::VisibilityPath::Direct,Facts(other,c)) == S::CommitResult::Recorded);
        CHECK(!s.BeforeViewerDeparture(999) && !s.Inspect().fenced);
        CHECK(s.BeforeViewerDeparture(v.guidRaw));
        CHECK(s.Inspect().fenced && s.Inspect().active == 3 && s.Inspect().retained == 0);
        CHECK(s.Inspect(m).records == 2 && s.Inspect(m).pendingRestores == 1);
        CHECK(s.Inspect(n).records == 1 && s.Inspect(n).pendingRestores == 1);
        CHECK(s.Inspect(q).records == 0 && s.Inspect(q).pendingRestores == 0);
        CHECK(!s.Reserve(q,v,d,11,Facts(v,d)));
        CHECK(s.ReadEvidence(m,v,a).state == S::EvidenceState::Known);
        CHECK(s.ReadEvidence(m,other,c).evidence.found);
        CHECK(s.BeforeViewerDeparture(v.guidRaw)); // repeat is idempotent
        CHECK(s.Inspect(m).pendingRestores == 1 && s.Inspect(n).pendingRestores == 1);
        CHECK(s.Seal(m,true) == S::SealResult::Retained);
        CHECK(s.ReadEvidence(m,v,a).evidence.found);
    }
    {
        S s;
        auto m = s.Open(1,2), n = s.Open(1,3);
        auto pending = s.Reserve(m,v,a,11,Facts(v,a));
        CHECK(pending);
        CHECK(!s.BeforeViewerDeparture(other.guidRaw));
        CHECK(s.BeforeViewerDeparture(v.guidRaw));
        CHECK(s.Inspect().fenced && s.Inspect().inFlight == 1);
        CHECK(!s.Reserve(n,old,b,11,Facts(old,b)));
        CHECK(s.Commit(pending,v,a,11,true,V::VisibilityPath::Direct,Facts(v,a)) == S::CommitResult::Uncertain);
        CHECK(s.ReadEvidence(m,v,a).state == S::EvidenceState::Uncertain);
        CHECK(s.Seal(m,true) == S::SealResult::Retained);
        CHECK(s.Inspect(m).reservedViewer == v && s.Inspect(m).reservedCreature == a);
    }
    {
        S s;
        auto m = s.Open(1,2);
        auto r = s.Reserve(m,v,a,11,Facts(v,a));
        CHECK(r && s.Commit(r,v,a,11,true,V::VisibilityPath::Direct,Facts(v,a)) == S::CommitResult::Recorded);
        CHECK(s.BeforeViewerDeparture(0));
        CHECK(s.HasUnattributedCommitRisk() && s.Inspect().fenced);
        CHECK(s.Inspect(m).frozen && s.Inspect(m).pendingRestores == 1);
        CHECK(s.ReadEvidence(m,v,a).state == S::EvidenceState::Uncertain);
    }
    std::cout << "PASS departure fanout, multi-map/epoch debt, unrelated viewers, pending and unknown identity\n";
}
