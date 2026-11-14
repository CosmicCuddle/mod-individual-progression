/*
 * CosmicCuddle Individual Progression — F14 verified RandomBot TBC floor
 * (one authoritative rule; default OFF).
 *
 * Owner rule (Information #10, 14 Nov 2026): a positively verified Blood
 * Elf/Draenei RandomBot has an absolute TBC minimum (stage 8). It may sync
 * upward (for example to Wrath) and later fall back only to TBC, never to
 * Vanilla. Altbots, SelfBots, humans and unknown identities are never
 * written by this rule; race alone is never proof.
 *
 * PURE, side-effect-free decisions only. The caller proves identity on the
 * login thread (verified epoch, bot account, headless session, BE/Draenei)
 * and performs the write. Era bands: Vanilla 0-7, TBC 8-12, Wrath 13-18.
 *
 * ProgressionLimit: 0 means no IP stage cap. A positive limit below 8 makes
 * the floor impossible: the rule then does nothing (fail closed, logged by
 * the caller). A target above a positive limit is capped at the limit. A
 * saved stage above a later-lowered limit is kept untouched.
 */

#ifndef INDIVIDUAL_PROGRESSION_F14_TBC_FLOOR_POLICY_H
#define INDIVIDUAL_PROGRESSION_F14_TBC_FLOOR_POLICY_H

#include <cstdint>

namespace IndividualProgressionF14TbcFloorPolicy
{
    constexpr std::uint8_t FirstTbcStage = 8;
    constexpr std::uint8_t FirstWrathStage = 13;
    constexpr std::uint8_t LastSupportedStage = 18;

    // The historical level-based login write: <=60 -> 0, 61-70 -> 8, 71+ -> 13.
    constexpr std::uint8_t LevelStage(std::uint32_t level)
    {
        if (level <= 60)
            return 0;
        if (level <= 70)
            return FirstTbcStage;
        return FirstWrathStage;
    }

    // The limit is usable for this rule: 0 (no cap) or a supported stage at
    // or above TBC. A negative, out-of-domain or Vanilla-only limit is not.
    constexpr bool LimitAllowsFloor(std::int32_t progressionLimit)
    {
        return progressionLimit == 0 ||
            (progressionLimit >= FirstTbcStage && progressionLimit <= LastSupportedStage);
    }

    constexpr bool FloorApplies(bool optionEnabled, bool identityPositivelyVerified,
                                std::int32_t progressionLimit)
    {
        return optionEnabled && identityPositivelyVerified && LimitAllowsFloor(progressionLimit);
    }

    // The stage a floored bot must hold: at least TBC, at least its level
    // stage (Wrath for 71+), at most a positive limit, and never below a
    // stage it already holds (a saved Wrath stage is kept). A corrupt saved
    // value (> 18) is returned unchanged, so the caller writes nothing.
    constexpr std::uint8_t TargetStage(std::uint8_t savedStage, std::uint32_t level,
                                       std::int32_t progressionLimit)
    {
        if (savedStage > LastSupportedStage)
            return savedStage;
        std::uint8_t const levelStage = LevelStage(level);
        std::uint8_t required = levelStage >= FirstTbcStage ? levelStage : FirstTbcStage;
        if (progressionLimit > 0 && required > progressionLimit)
            required = static_cast<std::uint8_t>(progressionLimit);
        return savedStage >= required ? savedStage : required;
    }

    // CS01 6884969131: with the unified rule ON, the four legacy F14 floor
    // options (PreserveVerifiedRandomBotStageOnLogin,
    // ForceVerifiedRandomBotTbcFloorOnLogin, ProtectVerifiedRandomBotStageFloor,
    // ProtectVerifiedRandomBotGroupFloor) are superseded and forced OFF at
    // config load: left ON, the stage floor would refuse .ip set / group set
    // requests below 8 instead of letting them land on TBC. Social isolation
    // is not a floor option and is unaffected.
    constexpr bool LegacyFloorOptionActive(bool unifiedOn, bool legacyRequested)
    {
        return legacyRequested && !unifiedOn;
    }

    constexpr bool NeedsRepair(std::uint8_t savedStage, std::uint8_t targetStage)
    {
        return savedStage < targetStage;
    }

    // Any other IP-owned write (group sync, GM set) for a floored bot: a
    // request below TBC lands on TBC instead. A Wrath bot can therefore
    // fall back to TBC, never to Vanilla. Same-era and higher requests are
    // unchanged.
    constexpr std::uint8_t ClampWrite(std::uint8_t requestedStage)
    {
        return requestedStage < FirstTbcStage ? FirstTbcStage : requestedStage;
    }
}

#endif
