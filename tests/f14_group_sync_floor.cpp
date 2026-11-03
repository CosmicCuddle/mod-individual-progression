// Portable, no AzerothCore/DB: model only the F14 verified group downgrade floor.
#include "../src/IndividualProgressionF14LoginPolicy.h"
#include <cstdint>
#include <initializer_list>
#include <iostream>
namespace Policy = IndividualProgressionF14LoginPolicy;
static_assert(Policy::ExpansionRaceFloor(1) == 8, "new TBC-race floor");
static_assert(Policy::ExpansionRaceFloor(60) == 8, "Vanilla-level TBC-race floor");
static_assert(Policy::ExpansionRaceFloor(70) == 8, "TBC-level floor");
static_assert(Policy::ExpansionRaceFloor(71) == 13, "WotLK-level floor");
static_assert(Policy::SkipVerifiedGroupDowngrade(8, 7, 60), "no Vanilla downgrade");
static_assert(!Policy::SkipVerifiedGroupDowngrade(8, 8, 60), "same-era unchanged");
static_assert(!Policy::SkipVerifiedGroupDowngrade(9, 8, 60), "TBC resync allowed");
static_assert(!Policy::SkipVerifiedGroupDowngrade(0, 7, 60), "no invented rescue of reset");
static_assert(Policy::SkipVerifiedGroupDowngrade(13, 8, 71), "71+ Wrath floor");
static_assert(!Policy::SkipVerifiedGroupDowngrade(13, 13, 71), "Wrath resync allowed");
static_assert(!Policy::SkipVerifiedGroupDowngrade(12, 8, 71), "no bogus rescue of old stage");
int main()
{
    std::uint32_t checks = 0;
    std::uint32_t failed = 0;
    auto test = [&](bool got, bool want)
    {
        ++checks;
        if (got != want)
            ++failed;
    };
    for (std::uint32_t level : {std::uint32_t{1}, std::uint32_t{60}, std::uint32_t{70}, std::uint32_t{71}, std::uint32_t{80}})
    {
        std::uint8_t const floor = Policy::ExpansionRaceFloor(level);
        for (std::uint8_t current = 0; current <= 18; ++current)
            for (std::uint8_t target = 0; target <= 18; ++target)
                test(Policy::SkipVerifiedGroupDowngrade(current, target, level),
                     current >= floor && target < floor);
    }
    std::cout << "F14 GROUP FLOOR MODEL " << (checks - failed) << "/" << checks
              << " passed (identity and game runtime not modeled)\n";
    return failed ? 1 : 0;
}
