#pragma once
#include <cstdint>
// Pure adapter for Grim's level-80 GM legacy-raid teleport rules.
// No Core, session, player, database, or Worldserver dependency.
namespace IndividualProgressionGmTeleportPolicy
{
    constexpr bool CanTeleport(uint32_t level, uint32_t rawStage,
                               uint32_t firstWrathStage, bool excludedHuman,
                               bool completedWrathTier5, bool allowVanillaRaidAccess,
                               bool attuned)
    {
        if (!attuned || level == 0 || firstWrathStage == 0)
            return false;
        if (level <= 70) // exact legacy Cosmic eligibility
            return rawStage < firstWrathStage || excludedHuman;
        if (level == 80) // newly supported Grim eligibility
            return completedWrathTier5 || rawStage < firstWrathStage || allowVanillaRaidAccess;
        return false; // no 71-79/81+
    }
}
