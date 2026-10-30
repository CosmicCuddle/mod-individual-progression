// Standalone tests for the actual header used by the IP GroupScript.
// No AzerothCore, database, Playerbots, or live server required.
#include "../src/IndividualProgressionGroupPolicy.h"

#include <cstdint>
#include <initializer_list>
#include <iostream>

using namespace IndividualProgressionGroupPolicy;

namespace
{
    int failures = 0;
    int checks = 0;

    void Check(bool actual, bool expected, char const* context)
    {
        ++checks;
        if (actual != expected)
        {
            ++failures;
            std::cerr << "FAIL: " << context << " (actual=" << actual
                      << ", expected=" << expected << ")\n";
        }
    }

    Expansion ExpectedEra(std::uint8_t stage)
    {
        if (stage <= 7) return Expansion::Vanilla;
        if (stage <= 12) return Expansion::BurningCrusade;
        if (stage <= 18) return Expansion::Wrath;
        return Expansion::Invalid;
    }
}

static_assert(GetExpansion(0) == Expansion::Vanilla, "tier 0 must be Vanilla");
static_assert(GetExpansion(7) == Expansion::Vanilla, "tier 7 must be Vanilla");
static_assert(GetExpansion(8) == Expansion::BurningCrusade, "tier 8 must be TBC");
static_assert(GetExpansion(12) == Expansion::BurningCrusade, "tier 12 must be TBC");
static_assert(GetExpansion(13) == Expansion::Wrath, "tier 13 must be WotLK");
static_assert(GetExpansion(18) == Expansion::Wrath, "tier 18 must be WotLK");
static_assert(GetExpansion(19) == Expansion::Invalid, "invalid tier must not be accepted");
static_assert(!IsAllowedRandomBotStage(10, 7) && IsAllowedRandomBotStage(10, 8),
              "Blood Elf RNDBOT floor must be TBC");
static_assert(!IsAllowedRandomBotStage(11, 0) && IsAllowedRandomBotStage(11, 13),
              "Draenei RNDBOT floor must be TBC");

int main()
{
    for (int stageA = 0; stageA <= 19; ++stageA)
        for (int stageB = 0; stageB <= 19; ++stageB)
            for (int raceA : { 1, 10, 11 })
                for (int raceB : { 1, 10, 11 })
                {
                    Expansion const a = ExpectedEra(static_cast<std::uint8_t>(stageA));
                    Expansion const b = ExpectedEra(static_cast<std::uint8_t>(stageB));
                    bool const expected = a != Expansion::Invalid && a == b &&
                        !(a == Expansion::Vanilla && (raceA == 10 || raceA == 11 || raceB == 10 || raceB == 11));
                    Check(MayGroup(static_cast<std::uint8_t>(stageA),
                                   static_cast<std::uint8_t>(raceA),
                                   static_cast<std::uint8_t>(stageB),
                                   static_cast<std::uint8_t>(raceB), 0),
                          expected, "all stage / race pairs, uncapped");
                }

    for (int race : { 1, 10, 11 })
        for (int stage = 0; stage <= 19; ++stage)
        {
            bool const expected = stage <= 18 && ((race != 10 && race != 11) || stage >= 8);
            Check(IsAllowedRandomBotStage(static_cast<std::uint8_t>(race),
                                          static_cast<std::uint8_t>(stage)),
                  expected, "rndbot starting floor");
        }

    Check(MayGroup(1, 1, 7, 2, 0), true, "different Vanilla raid tiers may group");
    Check(MayGroup(8, 1, 12, 2, 0), true, "different TBC raid tiers may group");
    Check(MayGroup(13, 1, 18, 2, 0), true, "different WotLK tiers may group");
    Check(MayGroup(7, 1, 8, 2, 0), false, "Vanilla / TBC denied");
    Check(MayGroup(12, 1, 13, 2, 0), false, "TBC / WotLK denied");
    Check(MayGroup(0, 10, 7, 1, 0), false, "human Blood Elf blocked from Vanilla group");
    Check(MayGroup(7, 11, 2, 1, 0), false, "human Draenei blocked from Vanilla group");
    Check(MayGroup(8, 10, 12, 1, 0), true, "Blood Elf allowed within TBC");
    Check(MayGroup(13, 11, 18, 1, 0), true, "Draenei allowed within Wrath");
    Check(MayGroup(9, 1, 12, 1, 7), true, "effective cap changes both humans to Vanilla");
    Check(MayGroup(9, 10, 12, 1, 7), false, "effective Vanilla cap forbids Blood Elf grouping");
    Check(MayGroup(19, 1, 18, 1, 7), false, "invalid raw tier not fixed by a cap");
    Check(MayGroup(8, 1, 8, 1, 19), false, "invalid progression cap denied");
    Check(MayGroup(8, 1, 8, 1, 0), true, "zero cap disables clamping");

    std::cout << "Group expansion policy: " << (checks - failures) << "/" << checks
              << " checks passed\n";
    return failures ? 1 : 0;
}
