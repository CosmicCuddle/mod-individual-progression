/*
 * F14 PURE bounded previously-hidden snapshot revalidation.
 * OFF / UNWIRED. A copied list may go stale after viewer logout,
 * creature death/respawn, new F14 suppression, config OFF, or a Core relink.
 *
 * This is NOT an owning-map-thread or normal-account proof. The future
 * caller must independently establish serialization while reading original
 * #94 and must RELEASE its lock before any Core visibility callback.
 * Revalidation and Core use must run in one genuinely serialized owning-map
 * operation; no queued cross-thread pointers or fabricated client ACK.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_STALE_SNAPSHOT_RECHECK_GUARD_H
#define INDIVIDUAL_PROGRESSION_F14_STALE_SNAPSHOT_RECHECK_GUARD_H

#include "IndividualProgressionF14ExactViewerRestoreRecheckPlan.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14StaleSnapshotRecheckGuard
{
    namespace Original = IndividualProgressionF14ViewerRestoreProtocol;
    namespace Planner = IndividualProgressionF14ExactViewerRestoreRecheckPlan;

    enum class Result : std::uint8_t
    {
        NoProvenSerializedOwner,
        CallerOutputAliasesSnapshot,
        SnapshotNotComplete,
        StaleSnapshotNoDispatch,
        InvalidPairIndex,
        OriginalPairNowIneligible,
        CandidateNormalCoreRecheckOnly,
        CandidateKnownPairWithUnattributedRisk
    };

    // Snapshot and scratch arrays MUST be distinct, fixed-size value buffers.
    // This function only computes a recommendation; the caller is responsible
    // for enforcing and preserving real Core/map owner serialization before,
    // during and immediately after this read-only gate. A true boolean passed
    // to this model cannot by itself prove any real ownership.
    template<std::size_t ProtocolCapacity, std::size_t OutputCapacity>
    Result RecheckCapturedKnownPair(
        Original::Protocol<ProtocolCapacity> const& original,
        std::array<typename Original::Protocol<ProtocolCapacity>::ExactPendingRestore,
                   OutputCapacity> const& captured,
        std::size_t capturedCount,
        typename Original::Protocol<ProtocolCapacity>::SnapshotResult capturedOutcome,
        bool globalRestoreWasRequiredAtCapture,
        std::array<typename Original::Protocol<ProtocolCapacity>::ExactPendingRestore,
                   OutputCapacity>& scratch,
        std::size_t requestedIndex,
        Planner::Facts const& freshLiveFacts,
        bool sourceProtocolReadSerializedOnOwningMap)
    {
        using Proto = Original::Protocol<ProtocolCapacity>;
        using Snapshot = typename Proto::SnapshotResult;

        if (!sourceProtocolReadSerializedOnOwningMap)
            return Result::NoProvenSerializedOwner;

        // Prevent caller from passing the same memory for source and scratch.
        if (&captured == &scratch)
            return Result::CallerOutputAliasesSnapshot;

        if (capturedOutcome == Snapshot::OutputTooSmallNoCopy ||
            capturedCount > OutputCapacity)
            return Result::SnapshotNotComplete;

        if (requestedIndex >= capturedCount)
            return Result::InvalidPairIndex;

        std::size_t currentCount = 0;
        std::size_t required = 0;
        Snapshot const current =
            original.CopyExactPendingRestores(scratch, currentCount, required);

        // The whole set must still be the same: never handle a subset while
        // newly revocated pairs are omitted by an old snapshot.
        if (current == Snapshot::OutputTooSmallNoCopy ||
            current != capturedOutcome ||
            currentCount != capturedCount ||
            required != capturedCount ||
            original.GlobalRestoreRequired() != globalRestoreWasRequiredAtCapture)
            return Result::StaleSnapshotNoDispatch;

        for (std::size_t i = 0; i < capturedCount; ++i)
        {
            auto const& old = captured[i];
            auto const& now = scratch[i];
            if (!(old.viewer == now.viewer) ||
                !(old.creature == now.creature) ||
                old.incarnation != now.incarnation ||
                old.suppressedAtServer != now.suppressedAtServer ||
                old.relinkAtServer != now.relinkAtServer ||
                old.restorePacket != now.restorePacket)
                return Result::StaleSnapshotNoDispatch;
        }

        auto const& item = captured[requestedIndex];
        if (Planner::SelectNormalRecheck(
                original, item.viewer, item.creature,
                item.incarnation, freshLiveFacts) !=
            Planner::Result::RequestNormalCoreVisibilityPass)
            return Result::OriginalPairNowIneligible;

        // This is a candidate for NORMAL Core CanSeeOrDetect; no "success",
        // send, forced visibility, client receipt or ledger-clearing call.
        return current == Snapshot::KnownPairsWithUnattributedRisk
            ? Result::CandidateKnownPairWithUnattributedRisk
            : Result::CandidateNormalCoreRecheckOnly;
    }

    constexpr bool MayDispatchCoreDirectly() { return false; }
    constexpr bool GuaranteesOwningMapThread() { return false; }
    constexpr bool CanClearOriginalLedger() { return false; }
    constexpr bool HasClientObjectAck() { return false; }
}
#endif
