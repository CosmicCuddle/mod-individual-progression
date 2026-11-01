/*
 * CosmicCuddle Individual Progression — F14 saved-stage login floor (F19).
 *
 * Pure, side-effect-free rules for the default-OFF F14 login safeguard in
 * IndividualProgressionPlayer.cpp. The safeguard may keep a verified
 * RandomBot's saved stage only when that stage is at least what the
 * original level-based login write would give, and within a nonzero
 * progression limit. Otherwise the original write runs, so turning the
 * safeguard ON never leaves a bot lower than the original behaviour.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_LOGIN_POLICY_H
#define INDIVIDUAL_PROGRESSION_F14_LOGIN_POLICY_H

#include <cstdint>

namespace IndividualProgressionF14LoginPolicy
{
    // Mirrors the original non-normal-account login write:
    // level <= 60 -> 0, 61..70 -> 8, 71+ -> 13.
    constexpr std::uint8_t LevelStage(std::uint32_t level)
    {
        if (level <= 60)
            return 0;
        if (level <= 70)
            return 8;
        return 13;
    }

    // F19-1: never preserve a stage below the original level stage.
    // F19-2: never preserve a stage above a nonzero progression limit.
    constexpr bool PreservesAtLeastLevelStage(std::uint8_t savedStage, std::uint32_t level, std::int32_t progressionLimit)
    {
        if (savedStage < LevelStage(level))
            return false;
        if (progressionLimit > 0 && savedStage > progressionLimit)
            return false;
        return true;
    }
}

#endif
