#include "IndividualProgressionF14PassIdentityBinding.h"
#include <cstdlib>
#include <iostream>

namespace P = IndividualProgressionF14PassIdentityBinding;

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)

int main()
{
    P::ViewerIdentity a{100,10,1000,{1,2}};
    P::ViewerIdentity aNextMap{100,10,1001,{1,3}};
    P::ViewerIdentity aRelog{100,11,1002,{1,2}};
    P::ViewerIdentity b{101,20,2000,{1,2}};

    P::Registry<3> r;
    CHECK(r.Begin(100,a) == P::Result::Begun);
    CHECK(r.Validate(100,a) == P::Result::Current);
    CHECK(r.Begin(100,a) == P::Result::Replayed);
    CHECK(r.Validate(101,a) == P::Result::Missing);

    CHECK(r.Begin(101,b) == P::Result::Begun);
    CHECK(r.Validate(101,b) == P::Result::Current);
    CHECK(r.Validate(100,aNextMap) == P::Result::Mismatch);
    CHECK(r.Validate(100,a) == P::Result::Invalidated);
    CHECK(r.End(100,a) == P::Result::Invalidated);
    CHECK(r.Validate(100,a) == P::Result::Replayed);

    CHECK(r.Begin(102,aRelog) == P::Result::Begun);
    CHECK(r.RevokeViewer(aRelog.guid,aRelog.loginEpoch) == 1);
    CHECK(r.Validate(102,aRelog) == P::Result::Invalidated);
    CHECK(r.End(102,aRelog) == P::Result::Invalidated);

    CHECK(r.Validate(99,a) == P::Result::Replayed);
    CHECK(r.Begin(103,aNextMap) == P::Result::Begun);
    CHECK(r.End(103,aNextMap) == P::Result::Ended);
    CHECK(r.Inspect().highwater == 103);

    P::Registry<1> tiny;
    CHECK(tiny.Begin(1,a) == P::Result::Begun);
    CHECK(tiny.Begin(2,b) == P::Result::Poisoned);
    CHECK(tiny.Inspect().poisoned);
    CHECK(tiny.Validate(1,a) == P::Result::Poisoned);

    P::Registry<2> invalid;
    CHECK(invalid.Begin(0,a) == P::Result::Refused);
    CHECK(invalid.Begin(1,{}) == P::Result::Refused);
    CHECK(invalid.Begin(1,a) == P::Result::Begun);
    CHECK(invalid.End(1,b) == P::Result::Mismatch);
    CHECK(invalid.Validate(1,a) == P::Result::Replayed);

    std::cout << "PASS F14 pass identity binding: exact token/viewer epoch/map provenance, replay refusal, revocation, mismatch invalidation, capacity fail-visible\n";
    return EXIT_SUCCESS;
}