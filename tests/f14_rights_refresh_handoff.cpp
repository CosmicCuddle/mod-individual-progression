// Pure C++ F14 human-assist map-thread queue witness. NO GAMEPLAY.
#include "IndividualProgressionF14LifecycleRegistry.h"
#include "IndividualProgressionF14LogoutRefreshQueue.h"
#include <cstdio>
namespace R = IndividualProgressionF14LifecycleRegistry;
namespace Q = IndividualProgressionF14LogoutRefreshQueue;
static int passed=0, failed=0;
#define CHECK(x) do { if(x) ++passed; else { ++failed; std::printf("FAIL %d: %s\n",__LINE__,#x); } } while(0)
int main()
{
    R::CreatureKey const a{0,0,10001}, b{0,1,10001}, c{1,0,10002};
    Q::Queue<3> pending;
    R::Registry registry;
    auto const aa=registry.OnParticipantBegin(a,0,9001,nullptr,0);
    auto const bb=registry.OnParticipantBegin(b,0,9002,nullptr,0);
    auto const cc=registry.OnParticipantBegin(c,0,9001,nullptr,0);
    CHECK(aa.stamp && bb.stamp && cc.stamp);
    CHECK(registry.OnParticipantEvidence(a,aa.stamp,0));
    CHECK(pending.Remember(a,aa.stamp));
    CHECK(pending.Remember(a,aa.stamp));
    CHECK(pending.Size()==1);
    CHECK(!pending.Take(b,aa.stamp));
    CHECK(pending.Take(a,aa.stamp));
    CHECK(!pending.Take(a,aa.stamp));
    unsigned fanout=0;
    CHECK(registry.OnAssistOfActor(9001,0,[&](R::CreatureKey const& key,std::uint64_t stamp)
    {
        ++fanout;
        CHECK(pending.Remember(key,stamp));
    })==2);
    CHECK(fanout==2);
    CHECK(pending.Size()==2);
    CHECK(pending.Take(a,aa.stamp));
    CHECK(pending.Take(c,cc.stamp));
    CHECK(!pending.HasPending());
    fanout=0;
    CHECK(registry.OnAssistOfActor(9001,9001,[&](R::CreatureKey const&,std::uint64_t){ ++fanout; })==0);
    CHECK(registry.OnAssistOfActor(0,0,[&](R::CreatureKey const&,std::uint64_t){ ++fanout; })==0);
    CHECK(fanout==0);
    CHECK(pending.Remember(a,aa.stamp));
    CHECK(!pending.Take(a,aa.stamp+1));
    CHECK(!pending.HasPending());
    Q::Queue<1> bounded;
    CHECK(bounded.Remember(a,aa.stamp));
    CHECK(!bounded.Remember(c,cc.stamp));
    CHECK(bounded.IsPoisoned());
    CHECK(!bounded.HasPending());
    std::printf("F14 rights refresh pure model %d/%d checks. NO GAMEPLAY.\n",passed,passed+failed);
    return failed?1:0;
}
