// Standalone tests for the F19 login floor used by the default-OFF F14 login
// safeguard. No AzerothCore, database, Playerbots or live server needed.
// Section "M" models the full login decision (labelled MODEL, mirroring the
// guard in IndividualProgressionPlayer.cpp), not the AzerothCore code itself.
#include "../src/IndividualProgressionF14LoginPolicy.h"

#include <cstdint>
#include <iostream>
#include <string>

using namespace IndividualProgressionF14LoginPolicy;

namespace
{
    int failures = 0;
    int checks = 0;

    void Check(int actual, int expected, std::string const& context)
    {
        ++checks;
        if (actual != expected)
        {
            ++failures;
            std::cerr << "FAIL: " << context << " (actual=" << actual << ", expected=" << expected << ")\n";
        }
    }

    // ---- MODEL of the login decision (labelled) ----
    // Returns the stage after login for a non-normal account.
    int ModelLoginStage(bool flagOn, bool tbcRace, bool headless, bool verifiedRandomBot,
                        int savedStage, std::uint32_t level, std::int32_t limit)
    {
        bool preserve = false;
        if (flagOn && tbcRace && headless && savedStage >= 8 && savedStage <= 18 &&
            PreservesAtLeastLevelStage(static_cast<std::uint8_t>(savedStage), level, limit))
            preserve = verifiedRandomBot;
        return preserve ? savedStage : LevelStage(level);
    }
}

static_assert(LevelStage(1) == 0 && LevelStage(60) == 0, "<=60 -> 0");
static_assert(LevelStage(61) == 8 && LevelStage(70) == 8, "61..70 -> 8");
static_assert(LevelStage(71) == 13 && LevelStage(80) == 13, "71+ -> 13");

int main()
{
    // ---- 1. Level stage boundaries ----
    for (std::uint32_t level = 1; level <= 85; ++level)
    {
        int const expected = level <= 60 ? 0 : (level <= 70 ? 8 : 13);
        Check(LevelStage(level), expected, "level stage " + std::to_string(level));
    }

    // ---- 2. Full matrix: preserve iff saved >= level stage and within limit ----
    for (std::int32_t limit : {0, 7, 8, 12, 13, 14, 18})
        for (std::uint32_t level : {1u, 30u, 60u, 61u, 65u, 70u, 71u, 75u, 80u})
            for (int saved = 0; saved <= 18; ++saved)
            {
                bool const expected = saved >= LevelStage(level) && (limit == 0 || saved <= limit);
                Check(PreservesAtLeastLevelStage(static_cast<std::uint8_t>(saved), level, limit), expected,
                      "limit " + std::to_string(limit) + " level " + std::to_string(level) + " saved " + std::to_string(saved));
            }

    // ---- 3. Named cases from the review ----
    for (std::uint32_t level = 71; level <= 80; ++level)
        for (int saved = 8; saved <= 12; ++saved)
            Check(PreservesAtLeastLevelStage(static_cast<std::uint8_t>(saved), level, 0), false,
                  "F19-1: level " + std::to_string(level) + " saved " + std::to_string(saved) + " gets original 13");
    Check(PreservesAtLeastLevelStage(8, 60, 0), true, "level 60 saved 8 preserved");
    Check(PreservesAtLeastLevelStage(12, 70, 0), true, "level 70 saved 12 preserved");
    Check(PreservesAtLeastLevelStage(13, 71, 0), true, "level 71 saved 13 preserved");
    Check(PreservesAtLeastLevelStage(18, 80, 0), true, "level 80 saved 18 preserved");
    Check(PreservesAtLeastLevelStage(9, 60, 8), false, "F19-2: saved 9 above limit 8 not preserved");
    Check(PreservesAtLeastLevelStage(8, 60, 8), true, "saved 8 at limit 8 preserved");
    Check(PreservesAtLeastLevelStage(14, 80, 13), false, "F19-2: saved 14 above limit 13 not preserved");
    Check(PreservesAtLeastLevelStage(12, 60, -1), true, "negative limit treated as no limit (IP uses 0)");

    // ---- M. MODEL: whole login decision ----
    // Flag OFF / not a verified headless TBC-race RandomBot: always the original write.
    for (std::uint32_t level : {60u, 70u, 80u})
        for (int saved : {0, 8, 12, 13, 18})
        {
            std::string const c = " level " + std::to_string(level) + " saved " + std::to_string(saved);
            Check(ModelLoginStage(false, true, true, true, saved, level, 0), LevelStage(level), "M: OFF" + c);
            Check(ModelLoginStage(true, false, true, true, saved, level, 0), LevelStage(level), "M: non-TBC race" + c);
            Check(ModelLoginStage(true, true, false, true, saved, level, 0), LevelStage(level), "M: not headless" + c);
            Check(ModelLoginStage(true, true, true, false, saved, level, 0), LevelStage(level), "M: unverified/Altbot" + c);
        }
    // ON, verified: never lower than the original write, never above the limit.
    for (std::int32_t limit : {0, 8, 12, 13})
        for (std::uint32_t level : {50u, 60u, 65u, 70u, 71u, 80u})
            for (int saved = 0; saved <= 18; ++saved)
            {
                int const after = ModelLoginStage(true, true, true, true, saved, level, limit);
                std::string const c = " limit " + std::to_string(limit) + " level " + std::to_string(level) + " saved " + std::to_string(saved);
                Check(after >= LevelStage(level), 1, "M: never below original" + c);
                if (after != LevelStage(level))
                    Check(limit == 0 || after <= limit, 1, "M: preserved within limit" + c);
            }
    Check(ModelLoginStage(true, true, true, true, 10, 75, 0), 13, "M: level 75 saved 10 -> original 13");
    Check(ModelLoginStage(true, true, true, true, 10, 60, 0), 10, "M: level 60 saved 10 -> kept 10");
    Check(ModelLoginStage(true, true, true, true, 5, 60, 0), 0, "M: saved 5 never preserved (original 0)");

    std::cout << "F19 login floor policy: " << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
