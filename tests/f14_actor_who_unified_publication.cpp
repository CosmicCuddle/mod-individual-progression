// Owner F14 pure actor-/who-published-identity policy model, not runtime.
#include "IndividualProgressionF14ActorWhoPublicationContract.h"
#include <cstdio>
#include <cstdlib>
using namespace IndividualProgressionF14ActorWhoPublicationContract;
static void Require(bool ok, char const* reason)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}
int main()
{
    Publication p;
    p.ownerFeatureOptionEnabled = true;
    p.ipProgressionEnabled = true;
    p.positiveCurrentLoginProducerProof = true;
    p.activeAndNotRetired = true;
    p.genuineHeadlessRandomBot = true;
    p.notAltbotAndNotHuman = true;
    p.sourceIdentityStableForThisQuery = true;
    p.fullPlayerGuid = 12345;
    p.loginEpoch = 7;
    p.accountId = 91;
    p.race = BloodElf;
    Observer o;
    o.positivelyNormalHuman = true;
    o.inWorld = true;
    o.sessionLive = true;
    o.currentEffectiveStageVerified = true;
    o.fullPlayerGuid = 111;
    o.loginEpoch = 4;
    o.effectiveStage = 7;
    LiveActor a;
    a.liveWorldObjectPlayer = true;
    a.sessionLive = true;
    a.headless = true;
    a.fullPlayerGuid = p.fullPlayerGuid;
    a.loginEpoch = p.loginEpoch;
    a.accountId = p.accountId;
    a.race = p.race;
    CachedWho w{p.fullPlayerGuid, p.race};

    auto Both = [&](){return MayAskActorSurface(p, o, a) && MayAskWhoSurface(p, o, w);};
    Require(Both(), "fully supplied synthetic facts match both surfaces");
    Require(!AuthorizesRuntimeActorOrWhoHiding() &&
            !AuthorizesNpcHidingOrRightsDenial() &&
            !AuthorizesStage8OrPaidEntitlement(), "actual permission always FALSE");

    int cases=0;
    // Every individual missing/uncertain input fails BOTH, never hide by race.
#define REJECT_FIELD(member) {auto q=p; q.member = false; Require(!MayAskActorSurface(q,o,a)&&!MayAskWhoSurface(q,o,w), #member); ++cases;}
    REJECT_FIELD(ownerFeatureOptionEnabled)
    REJECT_FIELD(ipProgressionEnabled)
    REJECT_FIELD(positiveCurrentLoginProducerProof)
    REJECT_FIELD(activeAndNotRetired)
    REJECT_FIELD(genuineHeadlessRandomBot)
    REJECT_FIELD(notAltbotAndNotHuman)
    REJECT_FIELD(sourceIdentityStableForThisQuery)
#undef REJECT_FIELD
#define REJECT_VIEWER(member) {auto q=o; q.member = false; Require(!MayAskActorSurface(p,q,a)&&!MayAskWhoSurface(p,q,w), #member); ++cases;}
    REJECT_VIEWER(positivelyNormalHuman)
    REJECT_VIEWER(inWorld)
    REJECT_VIEWER(sessionLive)
    REJECT_VIEWER(currentEffectiveStageVerified)
#undef REJECT_VIEWER
    {
        auto q=o; q.gmMode=true; Require(!MayAskActorSurface(p,q,a)&&!MayAskWhoSurface(p,q,w),"GM viewer");++cases;
        q=o; q.headless=true; Require(!MayAskActorSurface(p,q,a)&&!MayAskWhoSurface(p,q,w),"headless viewer");++cases;
        q=o; q.eraChangePending=true; Require(!MayAskActorSurface(p,q,a)&&!MayAskWhoSurface(p,q,w),"era transition");++cases;
        q=o; q.effectiveStage=8; Require(!MayAskActorSurface(p,q,a)&&!MayAskWhoSurface(p,q,w),"TBC viewer");++cases;
        q=o; q.effectiveStage=13; Require(!MayAskActorSurface(p,q,a)&&!MayAskWhoSurface(p,q,w),"Wrath viewer");++cases;
        q=o; q.fullPlayerGuid=p.fullPlayerGuid; Require(!MayAskActorSurface(p,q,a)&&!MayAskWhoSurface(p,q,w),"self");++cases;
        q=o; q.loginEpoch=0; Require(!MayAskActorSurface(p,q,a)&&!MayAskWhoSurface(p,q,w),"stale viewer epoch");++cases;
        auto retired=p; retired.activeAndNotRetired=false; Require(!MayAskWhoSurface(retired,o,w),"retired publication");++cases;
    }
    // Old actor, replaced session, wrong account or Altbot: actor must fail.
    {
        auto q=a; q.loginEpoch=6; Require(!MayAskActorSurface(p,o,q),"recycled actor session");++cases;
        q=a; q.accountId=93; Require(!MayAskActorSurface(p,o,q),"reused GUID different account");++cases;
        q=a; q.isAltbot=true; Require(!MayAskActorSurface(p,o,q),"headless Altbot");++cases;
        q=a; q.headless=false; Require(!MayAskActorSurface(p,o,q),"human player");++cases;
        q=a; q.sessionLive=false; Require(!MayAskActorSurface(p,o,q),"logout");++cases;
        q=a; q.fullPlayerGuid=0; Require(!MayAskActorSurface(p,o,q),"missing actor GUID");++cases;
    }
    // WHO's cached race/GUID is NEVER an identity lease. Identity publication
    // revocation must take effect before any GUID can be reused.
    {
        auto q=w; q.fullPlayerGuid=54321; Require(!MayAskWhoSurface(p,o,q),"WHO stale GUID");++cases;
        q=w; q.race=Draenei; Require(!MayAskWhoSurface(p,o,q),"WHO reused race");++cases;
        auto pr=p; pr.loginEpoch=0; Require(!MayAskWhoSurface(pr,o,w),"missing producer epoch");++cases;
        pr=p; pr.fullPlayerGuid=0; Require(!MayAskWhoSurface(pr,o,w),"missing publication GUID");++cases;
        pr=p; pr.accountId=0; Require(!MayAskWhoSurface(pr,o,w),"missing account origin");++cases;
        pr=p; pr.race=1; Require(!MayAskWhoSurface(pr,o,w),"real human non-TBC actor");++cases;
    }
    // Exhaustive masks for the 7 required producer bools; ONLY all seven true
    // can pass either surface when the other synthetic facts are fixed.
    for (unsigned mask=0;mask<128;++mask)
    {
        auto q=p;
        q.ownerFeatureOptionEnabled=mask&1;
        q.ipProgressionEnabled=mask&2;
        q.positiveCurrentLoginProducerProof=mask&4;
        q.activeAndNotRetired=mask&8;
        q.genuineHeadlessRandomBot=mask&16;
        q.notAltbotAndNotHuman=mask&32;
        q.sourceIdentityStableForThisQuery=mask&64;
        bool expected=mask==127;
        Require(MayAskWhoSurface(q,o,w)==expected,"WHO all 128 producer masks");
        Require(MayAskActorSurface(q,o,a)==expected,"actor all 128 producer masks");
        ++cases;
    }
    std::printf("PASS %d pure F14 WHO/player parity and stale-identity negative cases; live hide always OFF\n", cases);
    return 0;
}
