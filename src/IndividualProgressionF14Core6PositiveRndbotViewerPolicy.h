#ifndef COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_VIEWER_POLICY_H
#define COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_VIEWER_POLICY_H

// Pure C++17 *opt-in development model*. Actual Core6 actor/WHO callbacks
// remain default-FALSE unless a separate explicit positive experimental build
// opt-in AND IP verified-actor publication are both compiled and enabled.
// True means HIDE (Core6's API), NOT the old true-means-ALLOW IP observer API.
#include <cstdint>

namespace CosmicIPF14Core6PositiveRndbot
{
constexpr std::uint8_t BloodElfRace = 10;
constexpr std::uint8_t DraeneiRace = 11;
constexpr std::uint8_t FirstTbcStage = 8;
constexpr std::uint8_t LastKnownStage = 18;

struct Viewer
{
    bool confirmedNormalHumanAccount = false;
    bool confirmedNonGM = false;
    bool activeNonHeadlessSession = false;
    bool inWorld = false;
    bool confirmedNotSelf = false;
    bool effectiveStageKnown = false;
    std::uint8_t effectiveStage = 255;
};

struct Publication
{
    // This MUST originate from the login-verified, current generation, atomic
    // IP publisher; never accept race-only, no-socket or guessed bot identity.
    bool verifiedSelectedRandomBot = false;
    bool fullGuidMatches = false;
    bool currentAccountProven = false;
    bool raceMatches = false;
    bool nonzeroCurrentLoginEpoch = false;
    std::uint8_t publishedRace = 0;
};

constexpr bool ViewerIsVanilla(Viewer const& viewer)
{
    return viewer.confirmedNormalHumanAccount && viewer.confirmedNonGM &&
           viewer.activeNonHeadlessSession && viewer.inWorld &&
           viewer.confirmedNotSelf && viewer.effectiveStageKnown &&
           viewer.effectiveStage < FirstTbcStage;
}

constexpr bool HasPositiveTargetProof(Publication const& source)
{
    return source.verifiedSelectedRandomBot && source.fullGuidMatches &&
           source.currentAccountProven && source.raceMatches &&
           source.nonzeroCurrentLoginEpoch &&
           (source.publishedRace == BloodElfRace ||
            source.publishedRace == DraeneiRace);
}

// Actor path: verify live headless session and target world status *in addition*
// to the current IP verified-source publication; never hide humans/Altbots.
constexpr bool HideActor(Viewer const& viewer, Publication const& source,
                         bool actorInWorld, bool actorHeadless, bool actorNonGM)
{
    return ViewerIsVanilla(viewer) && HasPositiveTargetProof(source) &&
           actorInWorld && actorHeadless && actorNonGM;
}

// WHO path cannot dereference a Player or infer active map/lifecycle from the
// cached row. It must use a fresh read of IP's revocable current-login source
// and match the cached row race/full GUID with that source.
constexpr bool ExcludeWho(Viewer const& viewer, Publication const& source)
{
    return ViewerIsVanilla(viewer) && HasPositiveTargetProof(source);
}
}

#endif // COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_VIEWER_POLICY_H
