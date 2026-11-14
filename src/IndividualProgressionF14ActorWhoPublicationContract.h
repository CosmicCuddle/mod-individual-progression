/*
 * CosmicCuddle F14: UNWIRED, PURE actor-vs-WHO consistency contract.
 *
 * A verified login publication (not a cached race, stale map pointer or
 * guessed bot account) is the only hypothetical common input for the two
 * actual Core6 client surfaces. No active WorldObjectScript is registered.
 * No live hide, stage grant, database write or client ACK is authorised.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_ACTOR_WHO_PUBLICATION_CONTRACT_H
#define INDIVIDUAL_PROGRESSION_F14_ACTOR_WHO_PUBLICATION_CONTRACT_H

#include <cstdint>

namespace IndividualProgressionF14ActorWhoPublicationContract
{
    constexpr std::uint8_t BloodElf = 10;
    constexpr std::uint8_t Draenei = 11;
    constexpr std::uint8_t FirstTbcStage = 8;

    struct Publication
    {
        // Must be published and revoked by one authoritative PB/IP login
        // lifecycle. No such Core6 listener/provider is installed by this.
        bool ownerFeatureOptionEnabled = false;
        bool ipProgressionEnabled = false;
        bool positiveCurrentLoginProducerProof = false;
        bool activeAndNotRetired = false;
        bool genuineHeadlessRandomBot = false;
        bool notAltbotAndNotHuman = false;
        bool sourceIdentityStableForThisQuery = false;
        std::uint64_t fullPlayerGuid = 0;
        std::uint64_t loginEpoch = 0;
        std::uint32_t accountId = 0;
        std::uint8_t race = 0;
    };

    struct Observer
    {
        bool positivelyNormalHuman = false;
        bool inWorld = false;
        bool sessionLive = false;
        bool gmMode = false;
        bool headless = false;
        bool currentEffectiveStageVerified = false;
        bool eraChangePending = false;
        std::uint64_t fullPlayerGuid = 0;
        std::uint64_t loginEpoch = 0;
        std::uint8_t effectiveStage = 0;
    };

    // A live actor can prove its exact incarnation; cached WHO data cannot.
    struct LiveActor
    {
        bool liveWorldObjectPlayer = false;
        bool sessionLive = false;
        bool headless = false;
        bool isAltbot = false;
        std::uint64_t fullPlayerGuid = 0;
        std::uint64_t loginEpoch = 0;
        std::uint32_t accountId = 0;
        std::uint8_t race = 0;
    };

    struct CachedWho
    {
        // The normal WHO cache supplies target GUID + race, not login epoch.
        // Live lifetime authority must come from the publication producer,
        // never from cached race, account, name or guessing an epoch.
        std::uint64_t fullPlayerGuid = 0;
        std::uint8_t race = 0;
    };

    constexpr bool IsTbcRace(std::uint8_t race)
    {
        return race == BloodElf || race == Draenei;
    }

    constexpr bool CommonSourceAndViewerMayAsk(Publication const& p,
                                                Observer const& o)
    {
        return p.ownerFeatureOptionEnabled && p.ipProgressionEnabled &&
               p.positiveCurrentLoginProducerProof && p.activeAndNotRetired &&
               p.genuineHeadlessRandomBot && p.notAltbotAndNotHuman &&
               p.sourceIdentityStableForThisQuery &&
               p.fullPlayerGuid != 0 && p.loginEpoch != 0 &&
               p.accountId != 0 && IsTbcRace(p.race) &&
               o.positivelyNormalHuman && o.inWorld && o.sessionLive &&
               !o.gmMode && !o.headless &&
               o.currentEffectiveStageVerified && !o.eraChangePending &&
               o.fullPlayerGuid != 0 && o.loginEpoch != 0 &&
               o.fullPlayerGuid != p.fullPlayerGuid &&
               o.effectiveStage < FirstTbcStage;
    }

    // Hypothetical current-world Player actor only. The session epoch must
    // agree with the single published, revocable login identity.
    constexpr bool MayAskActorSurface(Publication const& p,
                                      Observer const& o, LiveActor const& a)
    {
        return CommonSourceAndViewerMayAsk(p, o) &&
               a.liveWorldObjectPlayer && a.sessionLive && a.headless &&
               !a.isAltbot && a.fullPlayerGuid == p.fullPlayerGuid &&
               a.loginEpoch == p.loginEpoch &&
               a.accountId == p.accountId && a.race == p.race;
    }

    // WHO is a snapshot: no race-only filtering, current producer lease is
    // mandatory. An old WHO entry with recycled GUID but stale positive
    // publication must not be allowed to survive a missing revoke.
    constexpr bool MayAskWhoSurface(Publication const& p,
                                    Observer const& o, CachedWho const& w)
    {
        return CommonSourceAndViewerMayAsk(p, o) &&
               w.fullPlayerGuid == p.fullPlayerGuid && w.race == p.race;
    }

    // Deliberately FALSE, even when synthetic facts are all true.
    // No Core/IP/PB runtime consumer exists for this header.
    constexpr bool AuthorizesRuntimeActorOrWhoHiding() { return false; }
    constexpr bool AuthorizesNpcHidingOrRightsDenial() { return false; }
    constexpr bool AuthorizesStage8OrPaidEntitlement() { return false; }
}
#endif
