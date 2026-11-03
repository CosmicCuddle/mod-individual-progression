/*
 * F14 / Playerbots ResetQuests NEGATIVE model and regression lock.
 *
 * This does NOT call Playerbots, restore a quest, repair a character, prove
 * that a Core/PB hook exists, or exercise a Worldserver. It demonstrates that
 * the current default-OFF/optional IP login guard cannot recover earned
 * progression markers 66001..66018 once a Playerbots reset has removed them.
 * Never use the reset as a test on actual player or bot characters.
 */
#include "../src/IndividualProgressionF14LoginPolicy.h"
#include <cstdint>
#include <iostream>

namespace F14 = IndividualProgressionF14LoginPolicy;

namespace
{
    unsigned checks = 0;
    unsigned failures = 0;

    void Check(char const* label, int got, int expected)
    {
        ++checks;
        if (got != expected)
        {
            ++failures;
            std::cerr << "FAIL: " << label << " got=" << got
                      << " expected=" << expected << '\n';
        }
    }

    // This is an explicit MODEL of the reviewed IP OnPlayerLogin guard, NOT
    // an execution of that callback or a live RandomPlayerbotMgr query.
    std::uint8_t ModelLoginStage(bool flagOn, bool verifiedHeadlessTbcRaceRrndbot,
                                 std::uint8_t rawSavedStage, std::uint32_t level,
                                 std::int32_t progressionLimit = 0)
    {
        if (flagOn && verifiedHeadlessTbcRaceRrndbot &&
            rawSavedStage >= 8 && rawSavedStage <= 18 &&
            F14::PreservesAtLeastLevelStage(rawSavedStage, level, progressionLimit))
            return rawSavedStage;
        return F14::LevelStage(level);
    }
}

int main()
{
    // Confirm the optional login safeguard is valuable only while the
    // ORIGINAL earned quest markers have NOT been erased.
    Check("level50 earned stage8 login keeps marker",
          ModelLoginStage(true, true, 8, 50), 8);
    Check("level65 earned stage12 login keeps marker",
          ModelLoginStage(true, true, 12, 65), 12);
    Check("level75 earned stage18 login keeps marker",
          ModelLoginStage(true, true, 18, 75), 18);

    // Model an upstream PlayerbotFactory::ResetQuests removing ALL rewarded
    // marker quests 66001..66018, including what the login guard later reads.
    // The resulting raw stage is zero; this is deliberately NOT a recovery.
    Check("reset marker at level50 relog returns vanilla zero",
          ModelLoginStage(true, true, 0, 50), 0);
    Check("reset marker at level65 relog returns level floor8 not earned12",
          ModelLoginStage(true, true, 0, 65), 8);
    Check("reset marker at level75 relog returns level floor13 not earned18",
          ModelLoginStage(true, true, 0, 75), 13);

    for (std::uint32_t level : {50u, 60u, 65u, 70u, 71u, 75u, 80u})
        for (std::uint8_t originallyEarned = 8; originallyEarned <= 18; ++originallyEarned)
        {
            // The saved marker is gone. No reconstructed entitlement,
            // synthetic quest reward, login-save hook or tier write is valid.
            std::uint8_t const afterReset = ModelLoginStage(true, true, 0, level);
            Check("reset does not recreate earned stage", afterReset,
                  F14::LevelStage(level));
            Check("group-floor guard cannot protect a missing stage",
                  F14::SkipVerifiedGroupDowngrade(0, 0, level), false);
            (void)originallyEarned;
        }

    // No existing stage -> no invented TBC entry on first bot creation.
    Check("new level50 BE/Draenei bot is NOT granted TBC",
          ModelLoginStage(true, true, 0, 50), 0);
    Check("unknown/Altbot identity never gains an inherited stage",
          ModelLoginStage(true, false, 12, 50), 0);
    Check("login safeguard OFF is the original untouched level write",
          ModelLoginStage(false, true, 12, 50), 0);

    std::cout << "F14 Playerbots ResetQuests BLOCKER model: "
              << (checks - failures) << "/" << checks
              << " checks passed; no quest restoration or gameplay effect.\n";
    return failures == 0 ? 0 : 1;
}
