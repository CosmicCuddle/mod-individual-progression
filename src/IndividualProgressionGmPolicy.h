/*
 * Individual Progression GM command access/audit decisions (pure, default-OFF).
 * No AzerothCore types so policy changes can be tested independently.
 *
 * Owner rule: players may query their OWN .ip get stage, never someone else's.
 * GM/console keep administrative lookup rights. No GM .ip set privilege is
 * removed and this file never modifies a paid-expansion flag.
 */
#ifndef INDIVIDUAL_PROGRESSION_GM_POLICY_H
#define INDIVIDUAL_PROGRESSION_GM_POLICY_H

namespace IndividualProgressionGmPolicy
{
    // A console/GM can ask about other players. A non-GM must have an
    // authenticated in-game session and be the same character as the target.
    constexpr bool CanViewTarget(bool privileged, bool inGameSession, bool sameCharacter)
    {
        return privileged || (inGameSession && sameCharacter);
    }

    // Only a NEW unpurchased era grant is warned/logged. Ordinary GMs keep
    // their established ability to recover progression markers.
    constexpr bool NewlyGrantedEra(bool paidFeatureActive, bool normalAccount,
                                   bool ownedBefore, bool ownedAfter)
    {
        return paidFeatureActive && normalAccount && !ownedBefore && ownedAfter;
    }
}

#endif
