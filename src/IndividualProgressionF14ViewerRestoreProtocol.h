/*
 * CosmicCuddle F14: per-viewer source-attributed suppression/restore EVIDENCE
 * protocol. PURE, NOT WIRED, value-only (no Core pointers, no packets).
 *
 * It keeps four evidence stages separate, for both suppression and restore:
 *   1. F14 policy intent        (server-observable: #86 query answered true)
 *   2. F14 server link change   (server-observable: #86 decision, wasLinked)
 *   3. packet handed to session (direct path: synchronous; batched path:
 *                                only IMPLIED by call structure until a
 *                                proposed batch-flush Core hook exists)
 *   4. client presentation      (NEVER server-observable in 3.3.5a: there is
 *                                no object update/destroy ACK opcode)
 *
 * It feeds the #85 prior-hidden Ledger ONLY with stage-2 F14 attribution and
 * owns it privately, so nothing here can clear a ledger entry: there is no
 * call to the ledger's client-proof completion or frozen-reset APIs.
 *
 * Fail visible: a positive F14 decision that cannot be recorded exactly
 * (config OFF, unverified or human/Altbot actor, no entitlement, rights not
 * proven, stale viewer session, recycled creature incarnation, dead
 * creature, wrong map, ledger frozen/full) sets a sticky global-restore
 * requirement. Ordinary Core link changes (phase, distance, GM, despawn,
 * map change, out-of-sight sweep) are never suppression evidence.
 *
 * Not thread-safe. A future caller must use one owner (or one mutex) and
 * must never hold it across a Core visibility call. Not a feature toggle.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_VIEWER_RESTORE_PROTOCOL_H
#define INDIVIDUAL_PROGRESSION_F14_VIEWER_RESTORE_PROTOCOL_H

#include "IndividualProgressionF14PriorHiddenLedger.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14ViewerRestoreProtocol
{
    namespace Ledger = IndividualProgressionF14PriorHiddenLedger;
    namespace Registry = IndividualProgressionF14LifecycleRegistry;

    using ViewerKey = Ledger::ViewerKey;
    using CreatureKey = Registry::CreatureKey;

    enum class VisibilityPath : std::uint8_t
    {
        Direct,   // Player::UpdateVisibilityOf(WorldObject*)
        Batched   // Player::UpdateVisibilityOf(T*, UpdateData&, ...)
    };

    enum class ServerStage : std::uint8_t
    {
        None,
        UnlinkedByF14,        // suppression: link removed by the F14 branch
        CreateWithheldByF14,  // suppression: baseline create withheld by F14
        RelinkedAfterF14      // restore: same viewer session + incarnation linked again
    };

    enum class PacketStage : std::uint8_t
    {
        None,                       // nothing dispatched (e.g. withheld create)
        ImpliedPendingBatchFlush,   // appended to UpdateData; flush NOT observed
        HandedToSessionDirect,      // direct path: SendDirectMessage already ran
        HandedToSessionBatched      // ONLY via the proposed flush hook (none exists)
    };

    // There is deliberately no other value and no setter.
    enum class ClientStage : std::uint8_t
    {
        NotServerObservable
    };

    constexpr ClientStage ClientStageOf() { return ClientStage::NotServerObservable; }

    enum class FailReason : std::uint8_t
    {
        ConfigOff,
        UnverifiedActor,       // humans, Altbots, unverified RandomBots
        NoEntitlement,
        RightsNotProven,
        StaleViewerSession,
        RecycledIncarnation,
        CreatureNotAlive,
        WrongMapThread,
        InvalidKey,
        LedgerRefused,         // frozen, pending restores, full or recycled
        RecordCapacity,
        Count
    };

    enum class Outcome : std::uint8_t
    {
        SuppressionRecorded,
        DuplicateSuppression,
        FailVisibleGlobalRestore,
        IgnoredOrdinary,
        IgnoredStale,
        IgnoredRecycled,
        IgnoredWrongMap,
        RestoreServerEvidence,
        BatchFlushObserved
    };

    // Facts a future caller must positively establish at decision time.
    // Every default is the fail-visible value.
    struct DecisionFacts
    {
        bool configEnabled = false;
        bool actorPositivelyVerifiedRandomBot = false;
        bool actorEntitled = false;
        bool pairRightsProven = false;  // F14CombatVisibilityPolicy stays default false
        bool creatureAlive = false;
        std::uint64_t currentViewerEpoch = 0;
        std::uint64_t currentCreatureIncarnation = 0;
        std::uint32_t eventMapId = 0;
        std::uint32_t eventInstanceId = 0;
    };

    // Facts for a generic (#76) link transition of an already-recorded pair.
    struct TransitionFacts
    {
        std::uint64_t currentViewerEpoch = 0;
        std::uint64_t currentCreatureIncarnation = 0;
        std::uint32_t eventMapId = 0;
        std::uint32_t eventInstanceId = 0;
    };

    struct Evidence
    {
        bool found = false;
        ViewerKey viewer{};
        CreatureKey creature{};
        std::uint64_t incarnation = 0;
        bool suppressIntent = false;
        ServerStage suppressServer = ServerStage::None;
        PacketStage suppressPacket = PacketStage::None;
        ServerStage restoreServer = ServerStage::None;
        PacketStage restorePacket = PacketStage::None;
    };

    template<std::size_t Capacity = Registry::DefaultMaxCreatures>
    class Protocol
    {
        static_assert(Capacity > 0, "F14 viewer restore protocol capacity must be positive");

    public:
        // Read-only: a positive suppression query may only be answered when
        // every fact is proven AND nothing is owed. This grants nothing.
        bool MayAnswerPositiveSuppression(ViewerKey viewer, CreatureKey creature,
                                          std::uint64_t incarnation,
                                          DecisionFacts const& facts) const
        {
            return !_unattributedHideRisk && _ledger.MayConsiderNewConcealment() &&
                CheckDecision(viewer, creature, incarnation, facts) == FailReason::Count;
        }

        // #86 OnF14CreatureSuppressionDecision mapped to values. The decision
        // already happened in Core, so a refusal here is NOT "do nothing":
        // an unrecordable F14 hide forces a sticky global restore.
        Outcome ObserveF14SuppressionDecision(ViewerKey viewer, CreatureKey creature,
                                              std::uint64_t incarnation, bool wasLinked,
                                              VisibilityPath path, DecisionFacts const& facts)
        {
            FailReason const reason = CheckDecision(viewer, creature, incarnation, facts);
            if (reason != FailReason::Count)
                return FailVisible(reason);

            if (!_ledger.RecordConfirmedF14Suppression(viewer, creature, incarnation, true))
                return FailVisible(FailReason::LedgerRefused);

            std::size_t const index = FindExact(viewer, creature);
            if (index != Capacity)
            {
                // Repeated visibility pass for the same proven suppression:
                // never downgrade or rewrite the first evidence.
                ++_duplicates;
                return Outcome::DuplicateSuppression;
            }
            if (_size == Capacity)
                return FailVisible(FailReason::RecordCapacity);

            Record& r = _records[_size++];
            r = Record{};
            r.viewer = viewer;
            r.creature = creature;
            r.incarnation = incarnation;
            r.suppressIntent = true; // the decision only fires after the query said true
            r.suppressServer = wasLinked ? ServerStage::UnlinkedByF14 : ServerStage::CreateWithheldByF14;
            r.suppressPacket = !wasLinked ? PacketStage::None :
                (path == VisibilityPath::Direct ? PacketStage::HandedToSessionDirect
                                                : PacketStage::ImpliedPendingBatchFlush);
            return Outcome::SuppressionRecorded;
        }

        // #76 generic link transition. Never suppression evidence. A re-link
        // for the exact recorded viewer session + incarnation on the owning
        // map is restore stage-2 evidence; anything else keeps the obligation.
        Outcome ObserveLinkTransition(ViewerKey viewer, CreatureKey creature,
                                      std::uint64_t incarnation, bool nowLinked,
                                      VisibilityPath path, TransitionFacts const& facts)
        {
            std::size_t const index = FindByGuid(viewer.guidRaw, creature);
            if (index == Capacity || !nowLinked)
                return Outcome::IgnoredOrdinary;

            Record& r = _records[index];
            if (!viewer.IsValid() || !(r.viewer == viewer) ||
                viewer.loginEpoch != facts.currentViewerEpoch)
                return Outcome::IgnoredStale;
            if (incarnation == 0 || incarnation != r.incarnation ||
                incarnation != facts.currentCreatureIncarnation)
                return Outcome::IgnoredRecycled;
            if (facts.eventMapId != creature.mapId || facts.eventInstanceId != creature.instanceId)
                return Outcome::IgnoredWrongMap;

            // Relinked while F14 still believed it suppressed: make the
            // obligation explicit (fail visible) before recording evidence.
            if (!_ledger.NeedsRestore(r.viewer, r.creature))
                _ledger.RevokeCreature(r.creature);

            r.restoreServer = ServerStage::RelinkedAfterF14;
            r.restorePacket = path == VisibilityPath::Direct ? PacketStage::HandedToSessionDirect
                                                             : PacketStage::ImpliedPendingBatchFlush;
            return Outcome::RestoreServerEvidence;
        }

        // ONLY for the PROPOSED OnF14VisibilityBatchFlushed Core hook, which
        // does not exist at the pinned Core. Upgrades implied -> handed over.
        Outcome ObserveProposedBatchFlush(ViewerKey viewer, CreatureKey creature,
                                          std::uint64_t incarnation, bool restoreDirection)
        {
            std::size_t const index = FindExact(viewer, creature);
            if (index == Capacity || _records[index].incarnation != incarnation)
                return Outcome::IgnoredStale;
            PacketStage& stage = restoreDirection ? _records[index].restorePacket
                                                  : _records[index].suppressPacket;
            if (stage != PacketStage::ImpliedPendingBatchFlush)
                return Outcome::IgnoredOrdinary;
            stage = PacketStage::HandedToSessionBatched;
            return Outcome::BatchFlushObserved;
        }

        // Lifecycle invalidations. They only ever ADD restore obligations.
        void ObserveCreatureRetired(CreatureKey creature) { _ledger.RevokeCreature(creature); }
        void ObserveViewerSessionEnded(std::uint64_t viewerGuid) { _ledger.RevokeViewer(viewerGuid); }
        void ObserveViewerEraOrRightsChanged(std::uint64_t viewerGuid) { _ledger.RevokeViewer(viewerGuid); }
        void ObserveConfigDisabled() { _ledger.RequestGlobalRestore(); }
        void ObserveMapUnloaded(std::uint32_t mapId, std::uint32_t instanceId)
        {
            for (std::size_t i = 0; i < _size; ++i)
                if (_records[i].creature.mapId == mapId && _records[i].creature.instanceId == instanceId)
                    _ledger.RevokeCreature(_records[i].creature);
        }

        Evidence Find(ViewerKey viewer, CreatureKey creature) const
        {
            Evidence e;
            std::size_t const index = FindExact(viewer, creature);
            if (index == Capacity)
                return e;
            Record const& r = _records[index];
            e.found = true;
            e.viewer = r.viewer;
            e.creature = r.creature;
            e.incarnation = r.incarnation;
            e.suppressIntent = r.suppressIntent;
            e.suppressServer = r.suppressServer;
            e.suppressPacket = r.suppressPacket;
            e.restoreServer = r.restoreServer;
            e.restorePacket = r.restorePacket;
            return e;
        }

        bool NeedsRestore(ViewerKey viewer, CreatureKey creature) const
        {
            return _unattributedHideRisk || _ledger.NeedsRestore(viewer, creature);
        }
        bool GlobalRestoreRequired() const { return _unattributedHideRisk || _ledger.IsFrozen(); }
        bool UnattributedHideRisk() const { return _unattributedHideRisk; }
        std::size_t Size() const { return _size; }
        std::uint64_t Duplicates() const { return _duplicates; }
        std::uint64_t Failures(FailReason reason) const
        {
            return reason < FailReason::Count ? _failures[static_cast<std::size_t>(reason)] : 0;
        }
        // Const only: no caller can complete or reset through this protocol.
        Ledger::Ledger<Capacity> const& LedgerView() const { return _ledger; }

    private:
        struct Record
        {
            ViewerKey viewer{};
            CreatureKey creature{};
            std::uint64_t incarnation = 0;
            bool suppressIntent = false;
            ServerStage suppressServer = ServerStage::None;
            PacketStage suppressPacket = PacketStage::None;
            ServerStage restoreServer = ServerStage::None;
            PacketStage restorePacket = PacketStage::None;
        };

        static FailReason CheckDecision(ViewerKey viewer, CreatureKey creature,
                                        std::uint64_t incarnation, DecisionFacts const& facts)
        {
            if (!viewer.IsValid() || !creature.IsValid() || incarnation == 0)
                return FailReason::InvalidKey;
            if (!facts.configEnabled)
                return FailReason::ConfigOff;
            if (!facts.actorPositivelyVerifiedRandomBot)
                return FailReason::UnverifiedActor;
            if (!facts.actorEntitled)
                return FailReason::NoEntitlement;
            if (!facts.pairRightsProven)
                return FailReason::RightsNotProven;
            if (viewer.loginEpoch != facts.currentViewerEpoch)
                return FailReason::StaleViewerSession;
            if (incarnation != facts.currentCreatureIncarnation)
                return FailReason::RecycledIncarnation;
            if (!facts.creatureAlive)
                return FailReason::CreatureNotAlive;
            if (facts.eventMapId != creature.mapId || facts.eventInstanceId != creature.instanceId)
                return FailReason::WrongMapThread;
            return FailReason::Count;
        }

        Outcome FailVisible(FailReason reason)
        {
            ++_failures[static_cast<std::size_t>(reason)];
            _unattributedHideRisk = true;   // sticky: no API here clears it
            _ledger.RequestGlobalRestore(); // keeps every recorded obligation
            return Outcome::FailVisibleGlobalRestore;
        }

        std::size_t FindExact(ViewerKey viewer, CreatureKey creature) const
        {
            for (std::size_t i = 0; i < _size; ++i)
                if (_records[i].viewer == viewer && _records[i].creature == creature)
                    return i;
            return Capacity;
        }

        std::size_t FindByGuid(std::uint64_t viewerGuid, CreatureKey creature) const
        {
            for (std::size_t i = 0; i < _size; ++i)
                if (_records[i].viewer.guidRaw == viewerGuid && _records[i].creature == creature)
                    return i;
            return Capacity;
        }

        Ledger::Ledger<Capacity> _ledger{};
        std::array<Record, Capacity> _records{};
        std::array<std::uint64_t, static_cast<std::size_t>(FailReason::Count)> _failures{};
        std::size_t _size = 0;
        std::uint64_t _duplicates = 0;
        bool _unattributedHideRisk = false;
    };
}
#endif
