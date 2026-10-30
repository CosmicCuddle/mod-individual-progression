/*
 * CosmicCuddle Individual Progression — expansion-banded grouping rules.
 * Pure, side-effect-free policy for use by GroupScript and player scripts.
 * Existing IP ProgressionState boundaries are asserted in the caller.
 */
#ifndef INDIVIDUAL_PROGRESSION_GROUP_POLICY_H
#define INDIVIDUAL_PROGRESSION_GROUP_POLICY_H

#include <cstdint>

namespace IndividualProgressionGroupPolicy
{
    enum class Expansion : std::uint8_t
    {
        Vanilla,
        BurningCrusade,
        Wrath,
        Invalid
    };

    constexpr std::uint8_t FirstTbcStage = 8;
    constexpr std::uint8_t FirstWrathStage = 13;
    constexpr std::uint8_t LastSupportedStage = 18;
    constexpr std::uint8_t BloodElfRace = 10;
    constexpr std::uint8_t DraeneiRace = 11;

    constexpr Expansion GetExpansion(std::uint8_t stage)
    {
        if (stage < FirstTbcStage)
            return Expansion::Vanilla;
        if (stage < FirstWrathStage)
            return Expansion::BurningCrusade;
        if (stage <= LastSupportedStage)
            return Expansion::Wrath;
        return Expansion::Invalid;
    }

    // A 0 limit means "no progression cap" in Individual Progression.
    constexpr std::uint8_t EffectiveStage(std::uint8_t stage, std::uint8_t limit)
    {
        return limit && stage > limit ? limit : stage;
    }

    constexpr bool IsExpansionRace(std::uint8_t race)
    {
        return race == BloodElfRace || race == DraeneiRace;
    }

    // All players, including real humans, must be in the same expansion when
    // EnforceGroupRules is ON. TBC races must never join a Vanilla-era group.
    // When enforcement is OFF, callers keep the established stock/group logic.
    constexpr bool MayGroup(std::uint8_t firstStage, std::uint8_t firstRace,
                            std::uint8_t secondStage, std::uint8_t secondRace,
                            std::uint8_t progressionLimit)
    {
        // Invalid quest-stage data must not become valid merely because a
        // lower server cap clamps its value.
        if (firstStage > LastSupportedStage || secondStage > LastSupportedStage ||
            progressionLimit > LastSupportedStage)
            return false;

        Expansion const first = GetExpansion(EffectiveStage(firstStage, progressionLimit));
        Expansion const second = GetExpansion(EffectiveStage(secondStage, progressionLimit));

        if (first == Expansion::Invalid || first != second)
            return false;

        if (first == Expansion::Vanilla && (IsExpansionRace(firstRace) || IsExpansionRace(secondRace)))
            return false;

        return true;
    }

    // Separate F14 design invariant, not wired into runtime yet: positive
    // RandomBot identity is needed before enforcing a starting tier floor.
    constexpr bool IsAllowedRandomBotStage(std::uint8_t race, std::uint8_t stage)
    {
        return stage <= LastSupportedStage && (!IsExpansionRace(race) || stage >= FirstTbcStage);
    }
}

#endif
