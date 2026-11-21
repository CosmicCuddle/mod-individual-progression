#ifndef INDIVIDUAL_PROGRESSION_F14_GROUP_SYNC_ERA_FLOOR_H
#define INDIVIDUAL_PROGRESSION_F14_GROUP_SYNC_ERA_FLOOR_H

#include "IndividualProgressionGroupPolicy.h"
#include <cstdint>

namespace IndividualProgressionF14GroupSyncEraFloor
{
    namespace GroupPolicy = IndividualProgressionGroupPolicy;
    using Era = GroupPolicy::Expansion;

    enum class Status : std::uint8_t
    {
        Available,
        UnverifiedBot,
        InvalidRace,
        InvalidStage,
        EarnedFloorMissing
    };

    struct GroupedDecision
    {
        Status status = Status::UnverifiedBot;
        Era temporaryEra = Era::Invalid;
        explicit constexpr operator bool() const { return status == Status::Available; }
    };

    struct DepartureEvidence
    {
        Status status = Status::UnverifiedBot;
        Era earnedEra = Era::Invalid;
        explicit constexpr operator bool() const { return status == Status::Available; }
    };

    constexpr bool IsSupportedRace(std::uint8_t race)
    {
        return (race >= 1 && race <= 8) ||
            race == GroupPolicy::BloodElfRace || race == GroupPolicy::DraeneiRace;
    }

    // A pure, temporary group-era view. The caller must independently prove
    // this is a bot eligible for group synchronization. Race alone is never
    // proof of RNDBOT/Altbot/SelfBot identity. The result is an Era, not a
    // ProgressionState or quest-stage write authorization.
    constexpr GroupedDecision WhileGrouped(bool eligibleBot, std::uint8_t race,
                                           std::uint8_t earnedStage, std::uint8_t groupStage)
    {
        if (!eligibleBot)
            return {Status::UnverifiedBot, Era::Invalid};
        if (!IsSupportedRace(race))
            return {Status::InvalidRace, Era::Invalid};
        Era const earned = GroupPolicy::GetExpansion(earnedStage);
        Era const group = GroupPolicy::GetExpansion(groupStage);
        if (earned == Era::Invalid || group == Era::Invalid)
            return {Status::InvalidStage, Era::Invalid};
        if (GroupPolicy::IsExpansionRace(race) && group == Era::Vanilla)
            return {Status::Available, Era::BurningCrusade};
        return {Status::Available, group};
    }

    // Leaving exposes exact earned-era evidence. A TBC-race bot with a
    // Vanilla/missing earned marker is diagnosed, not given a TBC solo view.
    constexpr DepartureEvidence AfterLeaving(bool eligibleBot, std::uint8_t race,
                                             std::uint8_t earnedStage)
    {
        if (!eligibleBot)
            return {Status::UnverifiedBot, Era::Invalid};
        if (!IsSupportedRace(race))
            return {Status::InvalidRace, Era::Invalid};
        Era const earned = GroupPolicy::GetExpansion(earnedStage);
        if (earned == Era::Invalid)
            return {Status::InvalidStage, Era::Invalid};
        if (GroupPolicy::IsExpansionRace(race) && earned == Era::Vanilla)
            return {Status::EarnedFloorMissing, earned};
        return {Status::Available, earned};
    }
}

#endif
