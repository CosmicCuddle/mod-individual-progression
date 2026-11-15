// Native tests for IndividualProgressionStageSnapshot (pure; no AzerothCore).
// Includes a concurrent reader/publisher stress run (use -fsanitize=thread).
#include "IndividualProgressionStageSnapshot.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

namespace S = IndividualProgressionStageSnapshot;

namespace
{
    unsigned g_pass = 0, g_fail = 0;
#define CHECK(expr) do { if (expr) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

    S::PublishInput Human(std::uint8_t raw, std::uint8_t eff)
    {
        S::PublishInput in;
        in.ipEnabled = true;
        in.accountResolved = true;
        in.gatedHuman = true;
        in.rawStage = raw;
        in.effectiveStage = eff;
        return in;
    }

    void TestMake()
    {
        for (int raw = 0; raw <= 25; ++raw)
            for (int eff = 0; eff <= 25; ++eff)
            {
                S::Snapshot const s = S::Make(Human(raw, eff), 7);
                bool const valid = raw <= 18 && eff <= raw;
                CHECK((s.status == S::Status::Known) == valid);
                CHECK(s.generation == 7);
                if (valid)
                    CHECK(s.rawStage == raw && s.effectiveStage == eff && s.gatedHuman && s.reason == S::UnknownReason::None);
                else
                    CHECK(s.reason == S::UnknownReason::CorruptStage && s.rawStage == 0 && s.effectiveStage == 0);
            }
        S::PublishInput off = Human(13, 13);
        off.ipEnabled = false;
        CHECK(S::Make(off, 1).reason == S::UnknownReason::IpDisabled);
        S::PublishInput unresolved = Human(13, 12);
        unresolved.accountResolved = false;
        S::Snapshot const u = S::Make(unresolved, 1);
        CHECK(u.status == S::Status::Unknown && u.reason == S::UnknownReason::AccountUnresolved);
        CHECK(u.rawStage == 0 && u.effectiveStage == 0); // nothing guessed
        // Paid boundary: raw13/effective12 vs raw13/effective13.
        CHECK(S::Make(Human(13, 12), 1).effectiveStage == 12);
        CHECK(S::Make(Human(13, 13), 1).effectiveStage == 13);
        // Default-constructed snapshot is Unknown.
        S::Snapshot const d;
        CHECK(d.status == S::Status::Unknown && d.reason == S::UnknownReason::NotPublished);
    }

    void TestRegistryLifecycle()
    {
        S::Registry r;
        CHECK(r.Get(5).reason == S::UnknownReason::NotPublished);
        CHECK(r.Size() == 0);                       // a read never inserts
        r.Publish(5, Human(1, 1));                  // login
        CHECK(r.Get(5).status == S::Status::Known && r.Get(5).rawStage == 1);
        r.Publish(5, Human(2, 2));                  // stage write
        CHECK(r.Get(5).rawStage == 2);
        r.Publish(6, Human(13, 12));                // unpaid Wrath
        r.Publish(6, Human(13, 13));                // confirmed purchase
        CHECK(r.Get(6).effectiveStage == 13);
        std::uint32_t const g = r.Generation();
        r.InvalidateAll();                          // config reload
        CHECK(r.Generation() == g + 1);
        CHECK(r.Get(5).status == S::Status::Unknown && r.Get(5).reason == S::UnknownReason::Invalidated);
        CHECK(r.Get(6).status == S::Status::Unknown);
        r.Publish(5, Human(2, 2));                  // republish
        CHECK(r.Get(5).status == S::Status::Known && r.Get(5).generation == g + 1);
        CHECK(r.Get(6).status == S::Status::Unknown); // not yet republished
        r.Erase(5);                                 // logout
        CHECK(r.Get(5).reason == S::UnknownReason::NotPublished);
        CHECK(r.Size() == 1);
    }

    void TestConcurrentReadersAndPublisher()
    {
        S::Registry r;
        std::shared_mutex lock;
        std::atomic<bool> stop{false};
        std::atomic<unsigned> knownBad{0}, reads{0};
        std::vector<std::thread> readers;
        for (int t = 0; t < 4; ++t)
            readers.emplace_back([&]
            {
                while (!stop.load())
                    for (std::uint32_t guid = 1; guid <= 64; ++guid)
                    {
                        S::Snapshot s;
                        {
                            std::shared_lock<std::shared_mutex> g(lock);
                            s = r.Get(guid);
                        }
                        ++reads;
                        // A Known value is always internally consistent.
                        if (s.status == S::Status::Known && (s.rawStage > 18 || s.effectiveStage > s.rawStage))
                            ++knownBad;
                    }
            });
        for (int round = 0; round < 2000; ++round)
        {
            std::unique_lock<std::shared_mutex> g(lock);
            std::uint32_t const guid = 1 + round % 64;
            if (round % 97 == 0)
                r.InvalidateAll();
            else if (round % 13 == 0)
                r.Erase(guid);
            else
                r.Publish(guid, Human(static_cast<std::uint8_t>(round % 19),
                                      static_cast<std::uint8_t>((round % 19) / 2)));
        }
        stop = true;
        for (auto& t : readers)
            t.join();
        CHECK(knownBad.load() == 0);
        CHECK(reads.load() > 0);
    }
}

int main()
{
    TestMake();
    TestRegistryLifecycle();
    TestConcurrentReadersAndPublisher();
    std::printf("STAGE SNAPSHOT MODEL %u/%u passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
