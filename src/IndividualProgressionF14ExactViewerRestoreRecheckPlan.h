/*
 * CosmicCuddle F14 — PURE exact prior-hidden viewer/creature Core-visibility
 * RECHECK plan. Review-only; NEVER wired or an authorization to hide.
 *
 * Requires original Claude #94 protocol, READ-ONLY. No Core pointers,
 * packet creation, callbacks, thread ownership or client ACK. A caller must
 * already enumerate exact prior-F14-hidden viewer and creature keys safely.
 * This planner cannot enumerate #94's private ledger by itself.
 *
 * A "RequestNormalCoreVisibilityPass" is only permission to consider a
 * normal Core CanSeeOrDetect-based reevaluation on a separately VERIFIED
 * owning map thread, not a force-CREATE, packet receipt, or debt release.
 * No function in this header ever clears historical restore obligations.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_EXACT_VIEWER_RESTORE_RECHECK_PLAN_H
#define INDIVIDUAL_PROGRESSION_F14_EXACT_VIEWER_RESTORE_RECHECK_PLAN_H

#include "IndividualProgressionF14ViewerRestoreProtocol.h"
#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14ExactViewerRestoreRecheckPlan
{
    namespace Original = IndividualProgressionF14ViewerRestoreProtocol;
    using ViewerKey = Original::ViewerKey;
    using CreatureKey = Original::CreatureKey;

    enum class Result : std::uint8_t
    {
        NoExactF14Hide,                 // never infer hiding from generic visibility
        WrongCreatureIncarnation,       // recycled GUID must not inherit debt
        FirstRecordRevocationRequired,  // must mark stop/rights/death before retry
        AlreadyServerRelinkedClientUnknown,
        NoCurrentRealHumanSession,      // null/headless/Altbot/unknown account
        StaleViewerLoginEpoch,
        ViewerNotInWorld,
        CreatureNoLongerInWorld,
        MissingTrustedOwningMapContext,
        DifferentMapOrInstance,
        RequestNormalCoreVisibilityPass // normal Core checks still own CREATE
    };

    struct Facts
    {
        // Independent LIVE proofs, never populated from the old ledger
        // itself. Every default is the fail-safe / no-action value.
        bool normalAccountHumanNonHeadlessSession = false;
        bool viewerInWorld = false;
        bool creatureInWorld = false;
        bool currentOwningMapThreadProven = false;
        std::uint64_t currentViewerGuidRaw = 0;
        std::uint64_t currentViewerLoginEpoch = 0;
        std::uint64_t currentCreatureGuidRaw = 0;
        std::uint64_t currentCreatureIncarnation = 0;
        std::uint32_t viewerMapId = 0;
        std::uint32_t viewerInstanceId = 0;
        std::uint32_t creatureMapId = 0;
        std::uint32_t creatureInstanceId = 0;
        std::uint32_t currentEventMapId = 0;
        std::uint32_t currentEventInstanceId = 0;
        // Era/GM/entitlement may change AFTER an original F14 hide.
        // It is NEVER a reason to suppress a safe normal Core recheck:
        // restoration remains owed after feature OFF or era promotion.
        bool viewerStillVanilla = false;
        bool featureStillEnabled = false;
    };

    template<std::size_t Capacity>
    Result SelectNormalRecheck(
        Original::Protocol<Capacity> const& original,
        ViewerKey previouslyHiddenViewer, CreatureKey previouslyHiddenCreature,
        std::uint64_t originalIncarnation, Facts const& now)
    {
        if (!previouslyHiddenViewer.IsValid() ||
            !previouslyHiddenCreature.IsValid() || !originalIncarnation)
            return Result::NoExactF14Hide;

        Original::Evidence const e =
            original.Find(previouslyHiddenViewer, previouslyHiddenCreature);
        if (!e.found || !e.suppressIntent ||
            (e.suppressServer != Original::ServerStage::UnlinkedByF14 &&
             e.suppressServer != Original::ServerStage::CreateWithheldByF14))
            return Result::NoExactF14Hide;

        if (e.incarnation != originalIncarnation)
            return Result::WrongCreatureIncarnation;

        // No generic OUT/link or timing event is treated as a revocation.
        if (!original.NeedsRestore(previouslyHiddenViewer, previouslyHiddenCreature))
            return Result::FirstRecordRevocationRequired;

        // Never fire a repeat CREATE simply because a sent packet is not an
        // ACK. Previous server link evidence does NOT clear original debt.
        if (e.restoreServer == Original::ServerStage::RelinkedAfterF14)
            return Result::AlreadyServerRelinkedClientUnknown;

        if (!now.normalAccountHumanNonHeadlessSession ||
            now.currentViewerGuidRaw != previouslyHiddenViewer.guidRaw)
            return Result::NoCurrentRealHumanSession;

        if (now.currentViewerLoginEpoch != previouslyHiddenViewer.loginEpoch)
            return Result::StaleViewerLoginEpoch;

        if (!now.viewerInWorld)
            return Result::ViewerNotInWorld;
        if (!now.creatureInWorld)
            return Result::CreatureNoLongerInWorld;
        if (!now.currentOwningMapThreadProven)
            return Result::MissingTrustedOwningMapContext;

        if (now.currentCreatureGuidRaw != previouslyHiddenCreature.guidRaw ||
            now.currentCreatureIncarnation != originalIncarnation ||
            now.viewerMapId != previouslyHiddenCreature.mapId ||
            now.viewerInstanceId != previouslyHiddenCreature.instanceId ||
            now.creatureMapId != previouslyHiddenCreature.mapId ||
            now.creatureInstanceId != previouslyHiddenCreature.instanceId ||
            now.currentEventMapId != previouslyHiddenCreature.mapId ||
            now.currentEventInstanceId != previouslyHiddenCreature.instanceId)
            return Result::DifferentMapOrInstance;

        // Only a pure value-only scheduling recommendation. A future caller
        // MUST not hold #94 protocol/registry locks across Core visibility.
        // Normal Core CanSeeOrDetect may still say FALSE due to range, phases
        // etc, and this code cannot prove a 3.3.5a client CREATE/render.
        return Result::RequestNormalCoreVisibilityPass;
    }

    constexpr bool HasClientAcknowledgement() { return false; }
    constexpr bool MayClearOriginalRestorationDebt() { return false; }
    constexpr bool EnablesPositiveHide() { return false; }
}
#endif
