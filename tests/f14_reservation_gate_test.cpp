#include "IndividualProgressionF14ReservationGate.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

using G = IndividualProgressionF14ReservationGate::Gate<2>;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)
int main()
{
    {
        G g;
        CHECK(!g.Open(false));
        auto a = g.Open(true), b = g.Open(true);
        CHECK(a && b && !g.Open(true));
        auto r = g.Reserve(a);
        CHECK(r && !g.Reserve(a));
        CHECK(g.Settle(r, G::Settlement::Unused));
        CHECK(!g.Settle(r, G::Settlement::Recorded));
        CHECK(g.Seal(a, true, true) == G::SealResult::CleanReleased);
        auto c = g.Open(true);
        CHECK(c && c.slot == a.slot && c.generation != a.generation);
        CHECK(!g.Reserve(a) && !g.Settle(r, G::Settlement::Recorded));
        CHECK(g.Seal(a, true, true) == G::SealResult::Rejected);
        auto d = g.Reserve(c);
        CHECK(d && g.Settle(d, G::Settlement::Recorded));
        CHECK(g.Seal(c, true, true) == G::SealResult::Retained);
        CHECK(!g.Reserve(b) && !g.Open(true));
        auto s = g.Inspect();
        CHECK(s.fenced && s.retained == 1 && s.active == 1 && s.occupied == 2);
    }
    {
        G g;
        auto a = g.Open(true);
        auto r = g.Reserve(a);
        CHECK(r && g.Seal(a, true, true) == G::SealResult::Retained);
        CHECK(!g.Settle(r, G::Settlement::Unused));
        auto s = g.Inspect();
        CHECK(s.fenced && s.inFlight == 1 && s.retained == 1);
    }
    {
        G g;
        auto a = g.Open(true), b = g.Open(true);
        auto r = g.Reserve(a);
        CHECK(g.Settle(r, G::Settlement::Uncertain));
        CHECK(!g.Reserve(b));
        CHECK(g.Seal(a, true, true) == G::SealResult::Retained);
    }
    {
        G g;
        auto a = g.Open(true);
        CHECK(g.Seal(a, false, true) == G::SealResult::Retained);
        CHECK(g.Inspect().fenced);
    }
    {
        G g;
        auto a = g.Open(true);
        CHECK(g.Seal(a, true, false) == G::SealResult::Retained);
        CHECK(g.Inspect().fenced);
    }
    // Both orders of the serialized reservation/closure race must be safe.
    for (int i = 0; i < 256; ++i)
    {
        G g;
        auto a = g.Open(true), b = g.Open(true);
        std::atomic<bool> start{false};
        G::Reservation r{};
        std::thread admit([&] { while (!start.load()) std::this_thread::yield(); r = g.Reserve(a); });
        std::thread close([&] { while (!start.load()) std::this_thread::yield(); g.CloseFence(); });
        start.store(true);
        admit.join(); close.join();
        CHECK(!g.Reserve(b) && !g.Open(true));
        CHECK(g.Inspect().inFlight == (r ? 1u : 0u));
        if (r) CHECK(g.Settle(r, G::Settlement::Recorded));
        CHECK(g.Seal(a, true, true) == (r ? G::SealResult::Retained : G::SealResult::CleanReleased));
        CHECK(g.Inspect().fenced);
    }
    std::cout << "PASS reservation accounting: capacity, stale handles, settlement, retention, concurrent fence\n";
}
