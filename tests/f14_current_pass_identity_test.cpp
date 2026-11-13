#include "IndividualProgressionF14CurrentPassIdentity.h"
#include <cstdlib>
#include <iostream>

namespace P = IndividualProgressionF14CurrentPassIdentity;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)

int main()
{
    P::ViewerIdentity const a{100, 10, 1000, {1, 2}};
    P::ViewerIdentity const b{101, 11, 1001, {1, 2}};
    P::ViewerIdentity const newEpoch{100, 12, 1002, {1, 2}};
    P::ViewerIdentity const newMap{100, 10, 1003, {1, 3}};

    // Normal pass, batch evidence, end and released-slot reuse.
    {
        P::Tracker<2> t;
        CHECK(t.Begin(500, a) == P::Result::Recorded);
        CHECK(t.IsActive(500, a));
        CHECK(t.ObserveBatch(500, a) == P::Result::BatchObserved);
        CHECK(t.BatchWasObserved(500, a));
        CHECK(t.End(500, a) == P::Result::Ended);
        CHECK(!t.IsActive(500, a));
        CHECK(t.Begin(501, b) == P::Result::Recorded);
        CHECK(t.End(501, b) == P::Result::Ended);
        CHECK(t.Inspect().active == 0 && t.Inspect().closed == 2);
    }

    // A/B nesting is conservatively refused; B End cannot revive A.
    {
        P::Tracker<4> t;
        CHECK(t.Begin(600, a) == P::Result::Recorded);
        CHECK(t.Begin(601, a) == P::Result::Mismatch);
        CHECK(!t.IsActive(600, a));
        CHECK(t.End(601, a) == P::Result::Refused);
        CHECK(t.ObserveBatch(600, a) == P::Result::Quarantined);
        CHECK(t.End(600, a) == P::Result::Quarantined);
        CHECK(t.Begin(602, a) == P::Result::Quarantined);
        CHECK(t.Begin(603, b) == P::Result::Recorded);
        CHECK(t.IsActive(603, b) && !t.Inspect().poisoned);
    }

    // Definitively closed tokens never reopen, including after a new epoch.
    {
        P::Tracker<4> t;
        CHECK(t.Begin(700, a) == P::Result::Recorded);
        CHECK(t.End(700, a) == P::Result::Ended);
        CHECK(t.Begin(700, a) == P::Result::Refused);
        CHECK(t.Begin(700, newEpoch) == P::Result::Refused);
        CHECK(t.Begin(701, b) == P::Result::Recorded);
        CHECK(t.IsActive(701, b) && !t.Inspect().poisoned);
    }

    // Duplicate End and unknown End are local, retained, and fail visible.
    {
        P::Tracker<4> t;
        CHECK(t.Begin(800, a) == P::Result::Recorded);
        CHECK(t.End(800, a) == P::Result::Ended);
        CHECK(t.End(800, a) == P::Result::Refused);
        CHECK(t.Begin(801, a) == P::Result::Quarantined);
        CHECK(t.End(810, b) == P::Result::Refused);
        CHECK(t.Begin(810, b) == P::Result::Refused);
        CHECK(t.Begin(811, newEpoch) == P::Result::Recorded);
        CHECK(!t.Inspect().poisoned);
    }
    {
        P::Tracker<4> t;
        CHECK(t.ObserveBatch(850, a) == P::Result::Refused);
        CHECK(t.Begin(850, a) == P::Result::Refused);
        CHECK(t.Begin(851, a) == P::Result::Quarantined);
        CHECK(t.Begin(852, b) == P::Result::Recorded);
        CHECK(t.IsActive(852, b) && !t.Inspect().poisoned);
    }

    // Session and map-generation mismatches quarantine only the token owner.
    {
        P::Tracker<4> t;
        CHECK(t.Begin(900, a) == P::Result::Recorded);
        CHECK(t.ObserveBatch(900, newEpoch) == P::Result::Mismatch);
        CHECK(!t.IsActive(900, a));
        CHECK(t.Begin(901, b) == P::Result::Recorded);
        CHECK(t.IsActive(901, b));
        CHECK(t.End(900, newEpoch) == P::Result::Mismatch);
        CHECK(t.End(901, b) == P::Result::Ended);
        CHECK(!t.Inspect().poisoned);
    }
    {
        P::Tracker<4> t;
        CHECK(t.Begin(910, a) == P::Result::Recorded);
        CHECK(t.ObserveBatch(910, newMap) == P::Result::Mismatch);
        CHECK(!t.BatchWasObserved(910, a));
        CHECK(t.Begin(911, b) == P::Result::Recorded);
        CHECK(t.IsActive(911, b));
        CHECK(t.End(910, a) == P::Result::Quarantined);
    }

    // An old event-time revoke cannot remove a newer epoch/map pass.
    {
        P::Tracker<4> t;
        CHECK(t.Begin(1000, a) == P::Result::Recorded);
        CHECK(t.RevokeViewer(a.guid) == 0);
        CHECK(t.IsActive(1000, a));
        CHECK(t.RevokeViewer(a) == 1);
        CHECK(t.Begin(1001, newEpoch) == P::Result::Recorded);
        CHECK(t.RevokeViewer(a.guid) == 0);
        CHECK(t.RevokeViewer(a) == 0);
        CHECK(t.IsActive(1001, newEpoch));
        CHECK(t.End(1001, newEpoch) == P::Result::Ended);
        CHECK(t.Begin(1002, newMap) == P::Result::Recorded);
        CHECK(t.RevokeViewer(a) == 0);
        CHECK(t.IsActive(1002, newMap));
        CHECK(t.RevokeViewer(newMap) == 1);
        CHECK(t.Begin(1002, newMap) == P::Result::Refused);
    }

    // Exact 64 active passes fit. The 65th is refused without evicting or
    // poisoning existing passes; a fresh token can use a legitimately freed slot.
    {
        P::Tracker<64> t;
        for (std::uint64_t i = 0; i < 64; ++i)
            CHECK(t.Begin(2000 + i, {1000 + i, 20 + i, 3000 + i, {1, 2}}) == P::Result::Recorded);
        CHECK(t.Inspect().active == 64);
        CHECK(t.Begin(2064, {1064, 84, 3064, {1, 2}}) == P::Result::Refused);
        CHECK(t.Inspect().active == 64 && !t.Inspect().poisoned);
        CHECK(t.IsActive(2000, {1000, 20, 3000, {1, 2}}));
        CHECK(t.End(2000, {1000, 20, 3000, {1, 2}}) == P::Result::Ended);
        CHECK(t.Begin(2065, {1065, 85, 3065, {1, 2}}) == P::Result::Recorded);
        CHECK(t.Begin(2064, {1064, 84, 3064, {1, 2}}) == P::Result::Refused);
    }

    // Once bounded closed evidence is full, all future new admissions stop.
    // Existing active passes remain usable and can end safely.
    {
        P::Tracker<2, 2> t;
        CHECK(t.Begin(3000, a) == P::Result::Recorded);
        CHECK(t.Begin(3001, b) == P::Result::Recorded);
        CHECK(t.End(3000, a) == P::Result::Ended);
        CHECK(t.IsActive(3001, b));
        CHECK(t.End(3001, b) == P::Result::Ended);
        CHECK(t.Inspect().admissionsClosed && !t.Inspect().poisoned);
        CHECK(t.Begin(3002, newEpoch) == P::Result::Refused);
        CHECK(t.Begin(3000, a) == P::Result::Refused);
    }

    // A real whole-table token ownership contradiction still fails closed.
    {
        P::Tracker<2> t;
        CHECK(t.Begin(4000, a) == P::Result::Recorded);
        CHECK(t.Begin(4000, b) == P::Result::Poisoned);
        CHECK(t.Inspect().poisoned && !t.IsActive(4000, a));
        CHECK(t.Begin(4001, b) == P::Result::Poisoned);
    }
    {
        P::Tracker<2> t;
        CHECK(t.Begin(4010, a) == P::Result::Recorded);
        t.PoisonForGlobalCorruption();
        CHECK(t.Inspect().poisoned && !t.IsActive(4010, a));
    }

    // Invalid facts confer no active identity.
    {
        P::Tracker<2> t;
        CHECK(t.Begin(0, a) == P::Result::Refused);
        CHECK(t.Begin(1, {}) == P::Result::Refused);
        CHECK(t.Begin(1, a) == P::Result::Refused);
        CHECK(t.Begin(2, b) == P::Result::Recorded);
        CHECK(t.IsActive(2, b));
        CHECK(t.End(0, a) == P::Result::Refused);
        CHECK(t.RevokeViewer(P::ViewerIdentity{}) == 0);
        CHECK(!t.Inspect().poisoned);
    }
    {
        P::Tracker<3> t;
        CHECK(t.Begin(5, a) == P::Result::Recorded);
        CHECK(t.Begin(5, {}) == P::Result::Mismatch);
        CHECK(!t.IsActive(5, a));
        CHECK(t.Begin(6, b) == P::Result::Recorded);
        CHECK(t.IsActive(6, b) && !t.Inspect().poisoned);
        CHECK(t.End(5, a) == P::Result::Quarantined);
        CHECK(t.Begin(5, a) == P::Result::Refused);
    }

    std::cout << "PASS F14 pass identity lifecycle hardening\n";
    return EXIT_SUCCESS;
}
