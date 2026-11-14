/*
 * CosmicCuddle Individual Progression — F14 verified RandomBot stage-8 floor
 * DURABILITY (default OFF).
 *
 * PURE, side-effect-free rule. It decides only whether an IP-owned
 * destructive progression write (ForceUpdateProgressionState) must be
 * refused because it would push a positively verified headless Blood
 * Elf/Draenei RandomBot below an ALREADY EARNED TBC stage (8+).
 *
 * It never creates stage 8 and never lowers a higher earned stage to 8.
 * A positive ProgressionLimit is NOT consulted: refusing a destructive
 * write is a no-op, not a grant, so it cannot bypass the cap for new
 * grants (UpdateProgressionState still enforces it). An earned stage above
 * a later-lowered cap is preserved untouched (quarantined), never silently
 * deleted; any reconciliation needs a separate owner-approved feature.
 * The caller must prove identity
 * separately (login-thread verified epoch, bot account, headless session,
 * BE/Draenei race); race, account regex, headless or level alone are never
 * positive proof. Era bands: Vanilla 0-7, TBC 8-12, Wrath 13-18.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_STAGE_FLOOR_POLICY_H
#define INDIVIDUAL_PROGRESSION_F14_STAGE_FLOOR_POLICY_H

#include <cstdint>

namespace IndividualProgressionF14StageFloorPolicy
{
    constexpr std::uint8_t FirstTbcStage = 8;
    constexpr std::uint8_t LastSupportedStage = 18;

    // True => refuse the write and keep the current earned stage unchanged.
    // False => historical behaviour (the write proceeds exactly as before).
    constexpr bool BlocksVerifiedFloorDowngrade(bool optionEnabled,
                                                bool identityPositivelyVerified,
                                                std::uint8_t currentStage,
                                                std::uint8_t requestedStage)
    {
        if (!optionEnabled || !identityPositivelyVerified)
            return false;
        // Only an already earned, supported TBC/Wrath stage is protected.
        if (currentStage < FirstTbcStage || currentStage > LastSupportedStage)
            return false;
        // Only a write that would land in Vanilla is a floor violation; a
        // same-era or higher write is left to the established route.
        return requestedStage < FirstTbcStage;
    }

    // Diagnostic only (never a decision input): the preserved earned stage
    // is above a positive ProgressionLimit, i.e. a historic over-cap record
    // kept untouched pending a separately approved reconciliation.
    constexpr bool IsHistoricOverCapStage(std::uint8_t currentStage, std::int32_t progressionLimit)
    {
        return progressionLimit > 0 && currentStage > progressionLimit;
    }
}

#endif
