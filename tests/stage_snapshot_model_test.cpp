// Native tests for IndividualProgressionStageSnapshot (pure; no AzerothCore).
// Includes a concurrent reader / publisher / invalidator stress run
// (build with -fsanitize=thread).
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

    constexpr std::uint64_t G1 = 0x0000000000000001ull;   // player low guid 1
    constexpr std::uint64_t G1Other = 0x0010000000000001ull; // same low guid, different high part

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

    S::PublishResult PublishNow(S::Registry& r, std::uint64_t g, std::uint64_t epoch, S::PublishInput const& in)
    {
        return r.Publish(g, r.BeginPublish(g, epoch), in);
    }

    void TestMake()
    {
        for (int raw = 0; raw <= 25; ++raw)
            for (int eff = 0; eff <= 25; ++eff)
            {
                S::Snapshot const s = S::Make(Human(raw, eff), 9, 7);
                bool const valid = raw <= 18 && eff <= raw;
                CHECK((s.status == S::Status::Known) == valid);
                CHECK(s.generation == 7 && s.sessionEpoch == 9);
                if (valid)
                    CHECK(s.rawStage == raw && s.effectiveStage == eff && s.gatedHuman && s.reason == S::UnknownReason::None);
                else
                    CHECK(s.reason == S::UnknownReason::CorruptStage && s.rawStage == 0 && s.effectiveStage == 0);
            }
        S::PublishInput off = Human(13, 13);
        off.ipEnabled = false;
        CHECK(S::Make(off, 1, 1).reason == S::UnknownReason::IpDisabled);
        S::PublishInput unresolved = Human(13, 12);
        unresolved.accountResolved = false;
        S::Snapshot const u = S::Make(unresolved, 1, 1);
        CHECK(u.status == S::Status::Unknown && u.reason == S::UnknownReason::AccountUnresolved);
        CHECK(u.rawStage == 0 && u.effectiveStage == 0);       // nothing guessed
        CHECK(S::Make(Human(13, 12), 1, 1).effectiveStage == 12); // unpaid / unconfirmed Wrath
        CHECK(S::Make(Human(13, 13), 1, 1).effectiveStage == 13); // confirmed purchase
        S::Snapshot const d;
        CHECK(d.status == S::Status::Unknown && d.reason == S::UnknownReason::NotPublished);
        for (std::uint32_t q = 66000; q <= 66019; ++q)
            CHECK(S::IsStageMarkerQuest(q) == (q >= 66001 && q <= 66018));
    }

    void TestFullGuidKey()
    {
        S::Registry r;
        std::uint64_t const e1 = r.Register(G1);
        CHECK(PublishNow(r, G1, e1, Human(5, 5)) == S::PublishResult::Published);
        CHECK(r.Get(G1Other).reason == S::UnknownReason::NotPublished);   // distinct key
        CHECK(r.Get(G1).rawStage == 5);
    }

    void TestLifecycle()
    {
        S::Registry r;
        CHECK(r.Get(G1).reason == S::UnknownReason::NotPublished);
        CHECK(r.Size() == 0);                                   // a read never inserts
        std::uint64_t const e1 = r.Register(G1);                // login
        CHECK(r.Get(G1).status == S::Status::Unknown && r.Get(G1).reason == S::UnknownReason::NotPublished);
        CHECK(PublishNow(r, G1, e1, Human(1, 1)) == S::PublishResult::Published); // owner thread
        S::Snapshot const a = r.Get(G1);
        CHECK(a.status == S::Status::Known && a.rawStage == 1 && a.sessionEpoch == e1);
        CHECK(!r.BeginPublish(G1, e1).valid);                    // clean: nothing to publish
        r.Invalidate(G1);                                       // before a stage write
        CHECK(r.Get(G1).status == S::Status::Unknown && r.Get(G1).reason == S::UnknownReason::Invalidated);
        CHECK(!S::SameKnownSnapshot(a, r.Get(G1)));             // PB re-read denies
        CHECK(PublishNow(r, G1, e1, Human(0, 0)) == S::PublishResult::Published); // downgrade published
        CHECK(r.Get(G1).rawStage == 0);
        r.Erase(G1);                                            // logout
        CHECK(r.Get(G1).reason == S::UnknownReason::NotPublished);
        CHECK(PublishNow(r, G1, e1, Human(9, 9)) == S::PublishResult::RejectedNotRegistered);
        CHECK(r.Size() == 0);
    }

    void TestLogoutReloginAba()
    {
        S::Registry r;
        std::uint64_t const oldEpoch = r.Register(G1);
        S::Ticket const late = r.BeginPublish(G1, oldEpoch);   // old session started computing
        CHECK(late.valid);
        r.Erase(G1);                                            // logout
        std::uint64_t const newEpoch = r.Register(G1);          // relogin
        CHECK(newEpoch != oldEpoch);
        CHECK(r.Publish(G1, late, Human(18, 18)) == S::PublishResult::RejectedStaleEpoch);
        CHECK(r.Get(G1).status == S::Status::Unknown);          // not resurrected
        CHECK(PublishNow(r, G1, newEpoch, Human(3, 3)) == S::PublishResult::Published);
        CHECK(r.Get(G1).rawStage == 3);
    }

    void TestReloadStaleWriter()
    {
        S::Registry r;
        std::uint64_t const e = r.Register(G1);
        S::Ticket const before = r.BeginPublish(G1, e);
        r.InvalidateAll();                                      // config reload
        CHECK(r.Publish(G1, before, Human(13, 13)) == S::PublishResult::RejectedStaleGeneration);
        CHECK(r.Get(G1).reason == S::UnknownReason::Invalidated);
        CHECK(PublishNow(r, G1, e, Human(13, 12)) == S::PublishResult::Published); // owner republish
        CHECK(r.Get(G1).effectiveStage == 12);
        S::Snapshot const k = r.Get(G1);
        r.InvalidateAll();
        CHECK(!S::SameKnownSnapshot(k, r.Get(G1)));
    }

    void TestInvalidatedWhileComputing()
    {
        S::Registry r;
        std::uint64_t const e = r.Register(G1);
        S::Ticket const t = r.BeginPublish(G1, e);
        r.Invalidate(G1);                                       // e.g. GM .quest remove meanwhile
        CHECK(r.Publish(G1, t, Human(18, 18)) == S::PublishResult::RejectedInvalidatedSince);
        CHECK(r.Get(G1).status == S::Status::Unknown);
        CHECK(PublishNow(r, G1, e, Human(10, 10)) == S::PublishResult::Published);
        S::PublishInput unresolved = Human(10, 10);
        unresolved.accountResolved = false;
        r.Invalidate(G1);
        CHECK(PublishNow(r, G1, e, unresolved) == S::PublishResult::Published);
        CHECK(r.Get(G1).reason == S::UnknownReason::AccountUnresolved);
        CHECK(r.BeginPublish(G1, e).valid);                     // stays dirty: retried later
        CHECK(PublishNow(r, G1, e, Human(10, 10)) == S::PublishResult::Published);
        CHECK(!r.BeginPublish(G1, e).valid && r.Get(G1).status == S::Status::Known);
        S::Ticket bogus;
        CHECK(r.Publish(G1, bogus, Human(1, 1)) == S::PublishResult::RejectedNotRegistered);
    }

    void TestCommandDetection()
    {
        for (char const* yes : {"quest remove 66003", "q rem 66001", "quest reward 66018",
                                "quest complete |cffffff00|Hquest:66010:60|h[x]|h|r", "66005", "a 1 66012 b"})
            CHECK(S::MentionsStageMarker(yes));
        for (char const* no : {"quest remove 66000", "quest remove 66019", "quest remove 166003",
                               "quest remove 660030", "ip set Bob 5", "", "66 001", "99999999999999999999"})
            CHECK(!S::MentionsStageMarker(no));
    }

    void TestSameKnownSnapshot()
    {
        S::Snapshot a = S::Make(Human(7, 7), 4, 2);
        S::Snapshot b = a;
        CHECK(S::SameKnownSnapshot(a, b));
        b.sessionEpoch = 5;
        CHECK(!S::SameKnownSnapshot(a, b));
        b = a; b.generation = 3;
        CHECK(!S::SameKnownSnapshot(a, b));
        b = a; b.rawStage = 6;
        CHECK(!S::SameKnownSnapshot(a, b));
        b = a; b.effectiveStage = 6;
        CHECK(!S::SameKnownSnapshot(a, b));
        S::Snapshot u;
        CHECK(!S::SameKnownSnapshot(u, u));
    }

    void TestConcurrency()
    {
        S::Registry r;
        std::shared_mutex lock;
        std::vector<std::uint64_t> epochs(32);
        for (std::uint64_t g = 0; g < 32; ++g)
            epochs[g] = r.Register(g + 1);
        std::atomic<bool> stop{false};
        std::atomic<unsigned> badKnown{0}, reads{0}, published{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 3; ++t)
            threads.emplace_back([&]
            {
                while (!stop.load())
                    for (std::uint64_t g = 1; g <= 32; ++g)
                    {
                        S::Snapshot s;
                        {
                            std::shared_lock<std::shared_mutex> l(lock);
                            s = r.Get(g);
                        }
                        ++reads;
                        if (s.status == S::Status::Known && (s.rawStage > 18 || s.effectiveStage > s.rawStage))
                            ++badKnown;
                    }
            });
        threads.emplace_back([&]      // invalidator (other threads / reload)
        {
            for (int i = 0; i < 3000 && !stop.load(); ++i)
            {
                std::unique_lock<std::shared_mutex> l(lock);
                if (i % 50 == 0)
                    r.InvalidateAll();
                else
                    r.Invalidate(1 + i % 32);
            }
        });
        for (int round = 0; round < 6000; ++round)   // owner-thread publisher
        {
            std::uint64_t const g = 1 + round % 32;
            S::Ticket t;
            {
                std::shared_lock<std::shared_mutex> l(lock);
                t = r.BeginPublish(g, epochs[g - 1]);
            }
            S::PublishInput const in = Human(static_cast<std::uint8_t>(round % 19), static_cast<std::uint8_t>((round % 19) / 2));
            std::unique_lock<std::shared_mutex> l(lock);
            if (r.Publish(g, t, in) == S::PublishResult::Published)
                ++published;
        }
        stop = true;
        for (auto& t : threads)
            t.join();
        CHECK(badKnown.load() == 0);
        CHECK(reads.load() > 0 && published.load() > 0);
    }
}

int main()
{
    TestMake();
    TestFullGuidKey();
    TestLifecycle();
    TestLogoutReloginAba();
    TestReloadStaleWriter();
    TestInvalidatedWhileComputing();
    TestCommandDetection();
    TestSameKnownSnapshot();
    TestConcurrency();
    std::printf("STAGE SNAPSHOT MODEL %u/%u passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
