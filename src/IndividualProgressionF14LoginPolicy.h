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

    // F14 verified RandomBot TBC stage floor, independently OFF in runtime.
    // This is a PURE calculation only. A caller MUST separately prove:
    // - account is a bot; positive selected RandomBot identity (not Altbot)
    // - headless session and Blood Elf/Draenei race
    // - full observer/WHO/combat visibility refresh bridge is installed
    // The stage write itself remains behind a default-OFF config and Core
    // build-time bridge check. A higher saved stage is never lowered.
    constexpr std::uint8_t DesiredVerifiedTbcRaceStage(std::uint8_t savedStage, std::uint32_t level)
    {
        std::uint8_t const levelStage = LevelStage(level);
        std::uint8_t const required = levelStage >= 8 ? levelStage : 8;
        return savedStage >= required ? savedStage : required;
    }

    constexpr bool CanSetVerifiedTbcRaceStage(std::uint8_t savedStage,
                                              std::uint32_t level,
                                              std::int32_t progressionLimit)
    {
        // Do not accept corrupt/unsupported stages or bypass a configured
        // limit. Limit=0 means IP has no stage cap; not a level-cap decision.
        // A negative or out-of-domain cap is UNKNOWN, never "uncapped".
        // Only 0 has the documented no-IP-stage-cap meaning. That STILL
        // does not prove world TBC availability, first birth or paid rights;
        // the separate default-OFF runtime caller is not release-ready.
        if (savedStage > 18 || progressionLimit < 0 || progressionLimit > 18)
            return false;
        return progressionLimit == 0 ||
            DesiredVerifiedTbcRaceStage(savedStage, level) <= progressionLimit;
    }

    // F14 group-sync containment: a verified TBC-race RandomBot with an
    // already earned stage 8+ must never be downgraded into Vanilla at ANY
    // level, including immediately after 70->71. No separate Wrath-stage
    // group floor is enforced here: that awaits an owner gameplay decision.
    // Identity is checked separately on the login thread.
    constexpr std::uint8_t ExpansionRaceFloor(std::uint32_t /*level*/)
    {
        return 8;
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
