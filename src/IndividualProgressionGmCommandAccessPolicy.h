/*
 * CosmicCuddle IP: side-effect-free, temporary Maintenance GM-command policy.
 * No Core/database dependencies; source usage must preserve GM operations.
 */
#ifndef COSMIC_IP_GM_COMMAND_ACCESS_POLICY_H
#define COSMIC_IP_GM_COMMAND_ACCESS_POLICY_H

namespace IndividualProgressionGmCommandAccessPolicy
{
    enum class Lookup : unsigned char { UseOriginal, UseSelf, Refuse };

    // Console and GMs retain original selected/named target resolution.
    // Normal players may read only their connected character; an explicitly
    // different named target is refused, while mere selection is ignored.
    constexpr Lookup DecideGetTarget(bool hasSession, bool gmOrHigher,
                                     bool explicitName, bool nameIsSelf)
    {
        if (!hasSession || gmOrHigher)
            return Lookup::UseOriginal;
        return explicitName && !nameIsSelf ? Lookup::Refuse : Lookup::UseSelf;
    }

    // Only GM overrides which newly open a paid expansion need extra audit.
    // This cannot make a grant: existing GM commands retain full authority.
    constexpr bool NeedsPaidGrantAudit(bool paidFeatureOn, bool normalAccount,
                                       bool tbcBefore, bool tbcAfter,
                                       bool wrathBefore, bool wrathAfter)
    {
        return paidFeatureOn && normalAccount &&
               ((!tbcBefore && tbcAfter) || (!wrathBefore && wrathAfter));
    }
}
#endif
