#include "IndividualProgressionF14GroupSyncView.h"
#include <cstdio>

namespace View = IndividualProgressionF14GroupSyncView;
static unsigned checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) { std::printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main()
{
    View::Snapshots<2> views;
    std::uint8_t stage = 255;
    CHECK(!views.Read(1, 10, stage));
    views.Request(1);
    auto work = views.TakeWork();
    CHECK(work.size() == 1);
    auto const stale = work.front();
    views.Request(1); // membership/progression/reconnect invalidates immediately
    CHECK(!views.Publish(stale, 10, 13));
    work = views.TakeWork();
    CHECK(work.size() == 1 && work.front().second != stale.second);
    CHECK(views.Publish(work.front(), 10, 13));
    CHECK(views.Read(1, 10, stage) && stage == 13);
    CHECK(!views.Read(1, 11, stage)); // changed leader cannot borrow old facts
    CHECK(!views.Read(2, 10, stage)); // switched group cannot borrow old facts
    views.Forget(1); // disband/leave
    CHECK(!views.Read(1, 10, stage));
    CHECK(!views.Publish(work.front(), 10, 13));
    views.Request(1);
    auto const beforeReload = views.TakeWork().front();
    views.Clear();
    views.Request(1);
    auto const afterReload = views.TakeWork().front();
    CHECK(beforeReload.second != afterReload.second);
    CHECK(!views.Publish(beforeReload, 10, 18));
    CHECK(views.Publish(afterReload, 10, 8));
    CHECK(views.Read(1, 10, stage) && stage == 8);
    CHECK(!views.Publish(afterReload, 0, 8));
    CHECK(!views.Publish(afterReload, 10, 19));
    views.Request(2);
    views.Request(3); // bounded overflow invalidates every positive snapshot
    CHECK(!views.Read(1, 10, stage));
    CHECK(views.TakeWork().empty());
    views.Clear();
    views.Request(1);
    CHECK(views.Publish(views.TakeWork().front(), 10, 8));
    View::Snapshots<1> exhausted(std::numeric_limits<std::uint64_t>::max() - 1);
    exhausted.Request(1);
    CHECK(exhausted.Publish(exhausted.TakeWork().front(), 10, 8));
    exhausted.Request(1);
    CHECK(!exhausted.Read(1, 10, stage));
    CHECK(exhausted.TakeWork().empty());
    exhausted.Clear();
    exhausted.Request(1);
    CHECK(exhausted.TakeWork().empty()); // reset never reuses an exhausted revision

    for (std::uint8_t race : {std::uint8_t{10}, std::uint8_t{11}})
        for (std::uint8_t earned = 0; earned <= 18; ++earned)
            for (std::uint8_t leader = 0; leader <= 18; ++leader)
            {
                auto const desired = leader < 8 ? 8 : leader;
                CHECK(View::EffectiveStage(race, earned, leader, 0) == desired);
                for (std::int32_t limit : {8, 12, 13, 18})
                    CHECK(View::EffectiveStage(race, earned, leader, limit) == std::min(desired, int(limit)));
            }
    for (unsigned earned = 0; earned < 256; ++earned)
        CHECK(View::UnavailableStage(static_cast<std::uint8_t>(earned)) ==
              (earned <= 18 ? 8 : 0));

    // The same earned Wrath bot transitions TBC -> Wrath -> TBC with no save.
    constexpr std::uint8_t earned = 17;
    CHECK(View::EffectiveStage(10, earned, 8, 0) == 8);
    CHECK(View::EffectiveStage(10, earned, 15, 0) == 15);
    CHECK(View::EffectiveStage(10, earned, 8, 0) == 8);
    CHECK(View::Floor::AfterLeaving(true, 10, earned).earnedEra == View::Floor::Era::Wrath);
    CHECK(View::EffectiveStage(10, earned, 255, 0) <= 8); // bad leader never confers Wrath
    CHECK(View::EffectiveStage(10, 255, 13, 0) == 0); // corrupt earned evidence is not permission
    std::printf("F14 GROUP VIEW %u/%u passed (pure snapshot and stage policy)\n", checks, checks);
    return 0;
}
