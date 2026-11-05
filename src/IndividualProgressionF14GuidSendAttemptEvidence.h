/*
 * CosmicCuddle F14 — PURE bridge of tagged Core per-GUID SendDirectMessage
 * ATTEMPTS into Claude #94's original, unchanged viewer restore protocol.
 *
 * Deliberately unwired. There is NO live registration for #93 OUT or #95
 * CREATE hooks. A Core tagged attempt is NOT client display/receipt/ACK.
 * Does NOT clear prior-hidden ledgers, authorize hiding or deliver packets.
 *
 * The caller of a future adapter must hold exactly one owner-thread/mutex
 * for the Protocol during this single synchronous operation. Never hold it
 * across Core visibility or ScriptMgr callbacks. This header stores nothing.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_GUID_SEND_ATTEMPT_EVIDENCE_H
#define INDIVIDUAL_PROGRESSION_F14_GUID_SEND_ATTEMPT_EVIDENCE_H

#include "IndividualProgressionF14ViewerRestoreProtocol.h"

#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14GuidSendAttemptEvidence
{
    namespace Restore = IndividualProgressionF14ViewerRestoreProtocol;
    using ViewerKey = Restore::ViewerKey;
    using CreatureKey = Restore::CreatureKey;

    enum class Direction : std::uint8_t
    {
        OutOfRange,  // #93 explicit F14-tagged per-GUID OUT-OF-RANGE
        Create       // #95 explicitly tagged per-GUID CREATE send-call
    };

    enum class Outcome : std::uint8_t
    {
        MissingOrUnverifiedCoreAttribution,
        StaleOrCrossMapViewerCreature,
        InvalidDirection,
        FrozenGlobalRestore,
        MissingMatchingPriorF14Suppression,
        WrongDirectionOrServerStage,
        NotPendingSameDirectionBatch,
        ProtocolRejected,
        ExactTaggedServerSendCallOnly
    };

    struct AttemptFacts
    {
        // External real Core facts; all default FAIL VISIBLE.
        bool sourceAttributedByExactCoreF14Hook = false;
        bool afterActualSessionSendDirectMessageCall = false;
        bool sameOwningMapThread = false;
        bool reportedGuidExactlyMatchesCreature = false;
        std::uint64_t currentViewerLoginEpoch = 0;
        std::uint64_t currentCreatureIncarnation = 0;
        std::uint32_t eventMapId = 0;
        std::uint32_t eventInstanceId = 0;
    };

    // #93 and #95 only prove that Core called SendDirectMessage after its
    // per-GUID buffering logic. No 3.3.5a object ACK exists in this model.
    constexpr bool HasIndependentlyVerifiedClientPresentation() { return false; }
    constexpr bool IsRuntimeF14ConcealmentAuthorized() { return false; }

    template<std::size_t Capacity>
    Outcome ObserveExactTaggedBatchAttempt(
        Restore::Protocol<Capacity>& protocol,
        ViewerKey viewer,
        CreatureKey creature,
        std::uint64_t incarnation,
        Direction direction,
        AttemptFacts const& facts)
    {
        if (!facts.sourceAttributedByExactCoreF14Hook ||
            !facts.afterActualSessionSendDirectMessageCall)
            return Outcome::MissingOrUnverifiedCoreAttribution;

        if (!viewer.IsValid() || !creature.IsValid() || incarnation == 0 ||
            !facts.sameOwningMapThread ||
            !facts.reportedGuidExactlyMatchesCreature ||
            facts.currentViewerLoginEpoch != viewer.loginEpoch ||
            facts.currentCreatureIncarnation != incarnation ||
            facts.eventMapId != creature.mapId ||
            facts.eventInstanceId != creature.instanceId)
            return Outcome::StaleOrCrossMapViewerCreature;

        if (direction != Direction::OutOfRange && direction != Direction::Create)
            return Outcome::InvalidDirection;

        // Once a protocol is globally frozen or poisoned, reject late
        // send-call notifications and preserve every restore obligation.
        if (protocol.GlobalRestoreRequired())
            return Outcome::FrozenGlobalRestore;

        Restore::Evidence const evidence = protocol.Find(viewer, creature);
        if (!evidence.found || !evidence.suppressIntent ||
            evidence.incarnation != incarnation)
            return Outcome::MissingMatchingPriorF14Suppression;

        if (direction == Direction::OutOfRange)
        {
            // A delayed destroy send-call report cannot upgrade suppression
            // evidence AFTER stop/death/rights change made this exact pair
            // owe restoration. Ordering matters even on the same map thread.
            if (protocol.NeedsRestore(viewer, creature))
                return Outcome::WrongDirectionOrServerStage;
            if (evidence.suppressServer != Restore::ServerStage::UnlinkedByF14)
                return Outcome::WrongDirectionOrServerStage;
            if (evidence.suppressPacket != Restore::PacketStage::ImpliedPendingBatchFlush)
                return Outcome::NotPendingSameDirectionBatch;
        }
        else
        {
            // A generic CREATE after an ordinary link must not be treated
            // as restoration. Require an exact prior-F14-hidden obligation.
            if (!protocol.NeedsRestore(viewer, creature) ||
                evidence.restoreServer != Restore::ServerStage::RelinkedAfterF14)
                return Outcome::WrongDirectionOrServerStage;
            if (evidence.restorePacket != Restore::PacketStage::ImpliedPendingBatchFlush)
                return Outcome::NotPendingSameDirectionBatch;
        }

        // This upgrades only the evidence that a specific GUID's update
        // was handed to the session send API; client proof remains unknown.
        Restore::Outcome const updated = protocol.ObserveProposedBatchFlush(
            viewer, creature, incarnation, direction == Direction::Create);
        if (updated != Restore::Outcome::BatchFlushObserved)
            return Outcome::ProtocolRejected;

        return Outcome::ExactTaggedServerSendCallOnly;
    }
}
#endif
