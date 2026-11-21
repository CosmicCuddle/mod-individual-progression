#include "IndividualProgressionF14GroupSyncEraFloor.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace Floor = IndividualProgressionF14GroupSyncEraFloor;
using Era = Floor::Era;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)

constexpr Era ExpectedEarned(std::uint8_t stage)
{
    return stage <= 7 ? Era::Vanilla : stage <= 12 ? Era::BurningCrusade : Era::Wrath;
}
constexpr Era ExpectedGroup(std::uint8_t race, std::uint8_t groupStage)
{
    Era const group = ExpectedEarned(groupStage);
    return (race == 10 || race == 11) && group == Era::Vanilla
        ? Era::BurningCrusade : group;
}

static_assert(Floor::WhileGrouped(true, 1, 14, 0).temporaryEra == Era::Vanilla,
              "normal bot can downsync to Vanilla");
static_assert(Floor::WhileGrouped(true, 10, 14, 0).temporaryEra == Era::BurningCrusade,
              "Blood Elf has a hard TBC minimum");
static_assert(Floor::WhileGrouped(true, 11, 14, 0).temporaryEra == Era::BurningCrusade,
              "Draenei has a hard TBC minimum");
static_assert(Floor::WhileGrouped(true, 10, 8, 13).temporaryEra == Era::Wrath,
              "TBC-race bot can rise to Wrath");
static_assert(Floor::AfterLeaving(true, 10, 14).earnedEra == Era::Wrath,
              "leaving exposes earned Wrath era");
static_assert(Floor::AfterLeaving(true, 11, 0).earnedEra == Era::Vanilla,
              "missing earned marker remains visible as Vanilla evidence");
static_assert(Floor::AfterLeaving(true, 11, 0).status == Floor::Status::EarnedFloorMissing,
              "missing earned floor is not usable solo evidence");

int main()
{
    std::size_t checks = 0;
    for (std::uint8_t race : std::array<std::uint8_t, 10>{1, 2, 3, 4, 5, 6, 7, 8, 10, 11})
    {
        for (std::uint8_t earned = 0; earned <= 18; ++earned)
        {
            for (std::uint8_t group = 0; group <= 18; ++group)
            {
                std::uint8_t const originalEarned = earned;
                auto const decision = Floor::WhileGrouped(true, race, earned, group);
                CHECK(decision.status == Floor::Status::Available);
                CHECK(decision.temporaryEra == ExpectedGroup(race, group));
                CHECK(earned == originalEarned); // No earned quest/stage mutation.
                ++checks;
            }
            auto const solo = Floor::AfterLeaving(true, race, earned);
            bool const missingFloor = (race == 10 || race == 11) && earned <= 7;
            CHECK(solo.status == (missingFloor ? Floor::Status::EarnedFloorMissing
                                              : Floor::Status::Available));
            CHECK(solo.earnedEra == ExpectedEarned(earned));
            CHECK(bool(solo) == !missingFloor);
        }
    }

    // One bot may rise to Wrath, fall to TBC even with a Vanilla group, and
    // recover its legitimate earned Wrath era on leave without a stage write.
    {
        std::uint8_t earned = 15;
        CHECK(Floor::WhileGrouped(true, 10, earned, 8).temporaryEra == Era::BurningCrusade);
        CHECK(Floor::WhileGrouped(true, 10, earned, 13).temporaryEra == Era::Wrath);
        CHECK(Floor::WhileGrouped(true, 10, earned, 0).temporaryEra == Era::BurningCrusade);
        CHECK(Floor::AfterLeaving(true, 10, earned).earnedEra == Era::Wrath);
        CHECK(earned == 15);
    }
    {
        std::uint8_t earned = 0; // Missing marker is not silently granted.
        CHECK(Floor::WhileGrouped(true, 11, earned, 0).temporaryEra == Era::BurningCrusade);
        auto const departure = Floor::AfterLeaving(true, 11, earned);
        CHECK(departure.earnedEra == Era::Vanilla);
        CHECK(departure.status == Floor::Status::EarnedFloorMissing && !departure);
        CHECK(earned == 0);
    }

    // Race does not establish bot eligibility; invalid stages confer no era.
    CHECK(Floor::WhileGrouped(false, 10, 8, 0).status == Floor::Status::UnverifiedBot);
    CHECK(Floor::WhileGrouped(false, 10, 8, 0).temporaryEra == Era::Invalid);
    CHECK(Floor::AfterLeaving(false, 11, 14).status == Floor::Status::UnverifiedBot);
    CHECK(Floor::WhileGrouped(true, 0, 8, 8).status == Floor::Status::InvalidRace);
    CHECK(Floor::WhileGrouped(true, 9, 8, 8).status == Floor::Status::InvalidRace);
    CHECK(Floor::AfterLeaving(true, 255, 8).earnedEra == Era::Invalid);
    CHECK(Floor::WhileGrouped(true, 10, 19, 8).status == Floor::Status::InvalidStage);
    CHECK(Floor::WhileGrouped(true, 10, 8, 19).status == Floor::Status::InvalidStage);
    CHECK(Floor::AfterLeaving(true, 10, 255).earnedEra == Era::Invalid);

    std::cout << "PASS F14 temporary group-era floor: " << checks
              << " grouped combinations plus solo, transitions and invalid input\n";
    return EXIT_SUCCESS;
}
