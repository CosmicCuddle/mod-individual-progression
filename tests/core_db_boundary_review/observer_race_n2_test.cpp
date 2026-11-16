// Concurrent map-thread saves vs world-thread drain (observer only, no DB).
#include "PB59CharacterSaveObserver.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
int main() {
    PB59::CharacterSaveObserver<256> obs;
    std::atomic<bool> stop{false};
    std::atomic<long> reported{0}, full{0}, sealed{0}, failed{0}, expectFail{0}, reportedOk{0};
    std::thread drain([&] { while (!stop) obs.Drain([&](auto const& r) { ++reported; if (!r.result.IsCommitted()) ++failed; }); });
    std::vector<std::thread> maps;
    for (int t = 0; t < 4; ++t) maps.emplace_back([&, t] {
        for (int i = 0; i < 20000; ++i) {
            auto r = obs.Reserve(1000 + t * 100 + (i % 50), 7, 9);
            if (r.status != std::remove_reference_t<decltype(obs)>::Store::Admission::Accepted) { ++full; continue; }
            PB59::SaveEvidence e; e.AddQuest(66001 + (i % 18)); e.AddItem(42 + i);
            assert(obs.Seal(r.ticket, e)); ++sealed; if (i % 10 == 0) ++expectFail;
            std::promise<DatabaseTransactionResult> p; auto f = p.get_future();
            assert(obs.Attach(r.ticket, std::move(f)));
            std::thread w([p = std::move(p), i]() mutable { p.set_value(DatabaseTransactionResult::FromExecutionCode(i % 10 ? 0 : -2)); });
            w.detach();
        }
    });
    for (auto& m : maps) m.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stop = true; drain.join();
    obs.Drain([&](auto const& r) { ++reported; if (!r.result.IsCommitted()) ++failed; }, true);
    std::printf("sealed=%ld reported=%ld nonCommitted=%ld full=%ld\n", sealed.load(), reported.load(), failed.load(), full.load());
    std::printf("expectedNonCommitted=%ld\n", expectFail.load()); assert(failed == expectFail); assert(reported <= sealed);
    std::puts("PASS race");
}
