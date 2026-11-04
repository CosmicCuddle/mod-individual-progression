// Offline model only: requires runtime to verify bot account, headless race10/11,
// selected RandomBot GUID and complete F14 world visibility bridge.
#include "../src/IndividualProgressionF14LoginPolicy.h"
#include <cstdint>
#include <iostream>
using namespace IndividualProgressionF14LoginPolicy;

static_assert(DesiredVerifiedTbcRaceStage(0, 60) == 8, "Vanilla bot must reach stage8");
static_assert(DesiredVerifiedTbcRaceStage(8, 60) == 8, "Preserve TBC");
static_assert(DesiredVerifiedTbcRaceStage(11, 60) == 11, "Never lower saved stage");
static_assert(DesiredVerifiedTbcRaceStage(0, 71) == 13, "71+ requires 13");
static_assert(DesiredVerifiedTbcRaceStage(18, 80) == 18, "Never lower stage18");
static_assert(!CanSetVerifiedTbcRaceStage(0, 60, 7), "limit7 blocks");
static_assert(!CanSetVerifiedTbcRaceStage(0, 71, 12), "limit12 blocks level71");
static_assert(!CanSetVerifiedTbcRaceStage(19, 60, 0), "invalid stage blocks");
static_assert(CanSetVerifiedTbcRaceStage(0, 60, 8), "limit8 permits");
static_assert(CanSetVerifiedTbcRaceStage(0, 71, 13), "limit13 permits");

constexpr std::uint8_t Model(bool flag, bool verified, bool bot, bool headless,
                             bool race, std::uint8_t saved, unsigned level, int limit)
{
    return flag && verified && bot && headless && race &&
           CanSetVerifiedTbcRaceStage(saved, level, limit)
        ? DesiredVerifiedTbcRaceStage(saved, level) : LevelStage(level);
}

int main()
{
    int tests=0, failed=0;
    auto check=[&](bool okay) { ++tests; if (!okay) ++failed; };
    for (unsigned level: {1u, 40u, 60u, 61u, 70u, 71u, 80u})
        for (unsigned saved=0; saved<=19; ++saved)
            for (int limit: {0,7,8,12,13,18})
            {
                auto stage=static_cast<std::uint8_t>(saved);
                auto legacy=LevelStage(level);
                auto desired=DesiredVerifiedTbcRaceStage(stage, level);
                bool eligible=CanSetVerifiedTbcRaceStage(stage, level, limit);
                check(eligible==(saved<=18 && (limit<=0 || desired<=limit)));
                check(Model(false,true,true,true,true,stage,level,limit)==legacy);
                check(Model(true,false,true,true,true,stage,level,limit)==legacy);
                check(Model(true,true,false,true,true,stage,level,limit)==legacy);
                check(Model(true,true,true,false,true,stage,level,limit)==legacy);
                check(Model(true,true,true,true,false,stage,level,limit)==legacy);
                check(Model(true,true,true,true,true,stage,level,limit)==(eligible?desired:legacy));
                if (eligible)
                {
                    check(desired>=8 && desired>=stage && desired>=legacy);
                    check(limit<=0 || desired<=limit);
                }
            }
    std::cout<<"F14 TBC floor: "<<(tests-failed)<<"/"<<tests<<" policy assertions passed\n";
    return failed==0?0:1;
}
