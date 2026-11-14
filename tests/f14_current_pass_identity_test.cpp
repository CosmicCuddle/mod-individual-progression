#include "IndividualProgressionF14CurrentPassIdentity.h"
#include <cstdlib>
#include <iostream>

namespace P = IndividualProgressionF14CurrentPassIdentity;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)

int main()
{
    P::ViewerIdentity a{100,10,1000,{1,2}};
    P::ViewerIdentity b{101,11,1001,{1,2}};

    {
        P::Tracker<4> t;
        CHECK(t.Begin(500,a) == P::Result::Recorded);
        CHECK(t.IsActive(500,a));
        CHECK(t.ObserveBatch(500,a) == P::Result::BatchObserved);
        CHECK(t.BatchWasObserved(500,a));
        CHECK(t.Begin(501,b) == P::Result::Recorded);
        CHECK(t.Inspect().active == 2);
        CHECK(t.End(500,a) == P::Result::Ended);
        CHECK(!t.IsActive(500,a));
        CHECK(t.End(501,b) == P::Result::Ended);
        auto const s = t.Inspect();
        CHECK(!s.poisoned && s.active == 0 && s.begins == 2 && s.batches == 1 && s.ends == 2);
    }

    {
        P::Tracker<4> t;
        CHECK(t.Begin(600,a) == P::Result::Recorded);
        CHECK(t.Begin(601,a) == P::Result::Poisoned);
        CHECK(t.Inspect().poisoned);
        CHECK(!t.IsActive(600,a));
    }

    {
        P::Tracker<4> t;
        CHECK(t.Begin(700,a) == P::Result::Recorded);
        P::ViewerIdentity stale{100,10,1002,{1,3}};
        CHECK(t.ObserveBatch(700,stale) == P::Result::Mismatch);
        CHECK(t.Inspect().poisoned && t.Inspect().mismatches == 1);
    }

    {
        P::Tracker<4> t;
        CHECK(t.Begin(750,a) == P::Result::Recorded);
        CHECK(t.End(750,{}) == P::Result::Mismatch);
        auto const s = t.Inspect();
        CHECK(s.poisoned && s.active == 0 && s.mismatches == 1);
        CHECK(t.Begin(751,a) == P::Result::Poisoned);
        CHECK(t.Inspect().active == 0);
    }

    {
        P::Tracker<4> t;
        CHECK(t.Begin(800,a) == P::Result::Recorded);
        CHECK(t.RevokeViewer(a.guid) == 1);
        CHECK(t.Inspect().active == 0 && t.Inspect().revocations == 1);
        CHECK(t.ObserveBatch(800,a) == P::Result::Refused);
        P::ViewerIdentity relog{100,12,1003,{1,2}};
        CHECK(t.Begin(801,relog) == P::Result::Recorded);
        CHECK(t.IsActive(801,relog));
    }

    {
        P::Tracker<1> t;
        CHECK(t.Begin(900,a) == P::Result::Recorded);
        CHECK(t.Begin(901,b) == P::Result::Poisoned);
        CHECK(t.Inspect().poisoned);
    }

    {
        P::Tracker<2> t;
        CHECK(t.Begin(0,a) == P::Result::Refused);
        CHECK(t.Begin(1,{}) == P::Result::Refused);
        CHECK(!t.Inspect().poisoned);
    }

    std::cout << "PASS current pass identity: exact begin/batch/end, multi-viewer, nested/mismatch fail-visible, lifecycle revoke and stale-token refusal\n";
    return EXIT_SUCCESS;
}