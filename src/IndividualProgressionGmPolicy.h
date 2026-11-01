/*
 * CosmicCuddle Individual Progression — command access and GM audit rules.
 *
 * Pure, side-effect-free decisions used by cs_individualProgression.cpp via
 * IndividualProgressionGmTools, so they can be unit tested on their own.
 */
#ifndef INDIVIDUAL_PROGRESSION_GM_POLICY_H
#define INDIVIDUAL_PROGRESSION_GM_POLICY_H

#include <cstdint>

namespace IndividualProgressionGmPolicy
{
    // Owner rule: any player may use .ip get, but only for their own
    // character. GMs and the console keep explicit name / selection lookup.
    enum class GetTarget : std::uint8_t
    {
        RequestedOrSelection, // existing behaviour: explicit name, else selection or self
        Self,                 // ordinary player: always their own character
        RefuseOther           // ordinary player naming someone else
    };

    constexpr GetTarget DecideGetTarget(bool console, bool gameMaster, bool hasRequested, bool requestedIsSelf)
    {
        if (console || gameMaster)
            return GetTarget::RequestedOrSelection;
        if (!hasRequested || requestedIsSelf)
            return GetTarget::Self;
        return GetTarget::RefuseOther;
    }

    // A GM .ip set "newly grants" an era when the target did not own it
    // before the write and does afterwards.
    constexpr bool NewlyGranted(bool ownedBefore, bool ownedAfter)
    {
        return !ownedBefore && ownedAfter;
    }
}

#endif
