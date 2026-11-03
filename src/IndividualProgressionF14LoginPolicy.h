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

    // F14 group-sync containment: Vanilla must never be the downgrade
    // destination of an already-stage8+ verified TBC-race RandomBot.
    // The level-71+ cohort keeps its existing WotLK minimum stage 13.
    // These are pure rules, with identity checked separately at login.
    constexpr std::uint8_t ExpansionRaceFloor(std::uint32_t level)
    {
        return level > 70 ? 13 : 8;
    }

    constexpr bool SkipVerifiedGroupDowngrade(std::uint8_t currentStage,
                                              std::uint8_t requestedStage,
                                              std::uint32_t level)
    {
        std::uint8_t const floor = ExpansionRaceFloor(level);
        // Only protect existing earned stage markers: never fabricate stage 8.
        // If a PB reset already deleted the stage, do not interfere with an
        // ordinary sync that may restore something; the PB reset is unsolved.
        return currentStage >= floor && requestedStage < floor;
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
