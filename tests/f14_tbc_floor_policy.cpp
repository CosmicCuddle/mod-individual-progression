#include "../src/IndividualProgressionF14LoginPolicy.h"
#include <cassert>
#include <cstdint>

namespace P = IndividualProgressionF14LoginPolicy;

int main()
{
    static_assert(P::TbcRaceFloorStage(1) == 8, "new TBC race floor");
    static_assert(P::TbcRaceFloorStage(60) == 8, "vanilla level cap");
    static_assert(P::TbcRaceFloorStage(61) == 8, "TBC level");
    static_assert(P::TbcRaceFloorStage(70) == 8, "TBC level cap");
    static_assert(P::TbcRaceFloorStage(71) == 13, "Wrath band");
    static_assert(P::TbcRaceFloorStage(80) == 13, "Wrath cap");
    static_assert(!P::NeedsTbcRaceFloor(8, 60), "do not overwrite earned TBC");
    static_assert(!P::NeedsTbcRaceFloor(13, 60), "do not downgrade Wrath");
    static_assert(!P::NeedsTbcRaceFloor(18, 80), "do not downgrade full progress");
    static_assert(P::NeedsTbcRaceFloor(7, 60), "Vanilla race bot must upgrade");
    static_assert(P::NeedsTbcRaceFloor(8, 71), "Wrath-level floor");

    unsigned count = 0;
    for (std::uint32_t level : {1u, 59u, 60u, 61u, 70u, 71u, 79u, 80u})
        for (std::uint8_t saved = 0; saved <= 18; ++saved)
        {
            std::uint8_t const floor = P::TbcRaceFloorStage(level);
            std::uint8_t const expected = level <= 70 ? 8 : 13;
            assert(floor == expected);
            bool const needs = saved < floor;
            assert(P::NeedsTbcRaceFloor(saved, level) == needs);
            std::uint8_t const result = needs ? floor : saved;
            assert(result >= 8 && result >= saved && result <= 18);
            ++count;
        }
    assert(count == 8 * 19);
    return 0;
}
