#ifndef INDIVIDUAL_PROGRESSION_F14_OWNED_RESTORE_SHARDS_H
#define INDIVIDUAL_PROGRESSION_F14_OWNED_RESTORE_SHARDS_H

#include "IndividualProgressionF14ReservationGate.h"
#include "IndividualProgressionF14ViewerRestoreProtocol.h"
#include <array>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>

namespace IndividualProgressionF14OwnedRestoreShards
{
    namespace VRP = IndividualProgressionF14ViewerRestoreProtocol;
    // Value-only experimental composition. One process-lifetime owner is
    // required by a future caller. No Core pointers, callbacks or packet I/O.
    // All model access is serialized; no payload reference escapes. No R2
    // debt retirement, runtime registration or authorization to conceal.
    template<std::size_t Shards, std::size_t Pairs>
    class Store
    {
        using Accounting = IndividualProgressionF14ReservationGate::Gate<Shards>;
        using Protocol = VRP::Protocol<Pairs>;
        static_assert(std::is_nothrow_default_constructible<Protocol>::value,
                      "whole-shard preallocation must not throw on construction");
    public:
        using Handle = typename Accounting::Handle;
        using Reservation = typename Accounting::Reservation;
        using SealResult = typename Accounting::SealResult;
        using Snapshot = typename Accounting::Snapshot;
        enum class CommitResult { Recorded, Uncertain, Rejected };
        enum class EvidenceState { Missing, Known, Uncertain };
        struct EvidenceRead
        {
            EvidenceState state = EvidenceState::Missing;
            VRP::Evidence evidence{};
        };
        struct Status
        {
            bool found = false, retained = false, pending = false, uncertain = false;
            bool frozen = false, unattributed = false;
            std::size_t records = 0, ledgerPairs = 0, pendingRestores = 0;
            VRP::ViewerKey reservedViewer{};
            VRP::CreatureKey reservedCreature{};
            std::uint64_t reservedIncarnation = 0;
        };

        Store() = default;
        Store(Store const&) = delete;
        Store& operator=(Store const&) = delete;

        // The fixed slot count bounds actual whole Protocol allocations,
        // including active/retained payloads and the next provisional one.
        // Allocator overhead and Store metadata are separate from payload bytes.
        static constexpr std::size_t MaxPayloadBytes = sizeof(Protocol) * Shards;
        Handle Open(std::uint32_t mapId, std::uint32_t instanceId)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            auto const state = _gate.Inspect();
            if (state.fenced || state.occupied == Shards)
                return {};
            for (Slot const& s : _slots)
                if ((s.active || s.retained) && s.mapId == mapId && s.instanceId == instanceId)
                    return {};
            // Keep allocation serialized until a provisional-slot budget is
            // implemented: allocating outside this lock could exceed Shards.
            std::unique_ptr<Protocol> payload(new (std::nothrow) Protocol{});
            if (!payload)
                return {};
            Handle h = _gate.Open(true);
            if (!h)
                return {};
            Slot& s = _slots[h.slot];
            s.handle = h;
            s.mapId = mapId;
            s.instanceId = instanceId;
            s.active = std::move(payload);
            return h;
        }

        // Initial suppression only. Existing-hide queries require a separate
        // idempotent read path; do not allocate a second reservation for them.
        Reservation Reserve(Handle h, VRP::ViewerKey viewer, VRP::CreatureKey creature,
                            std::uint64_t incarnation, VRP::DecisionFacts const& facts)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(h);
            if (!s || s->pending || !SameMap(*s, creature) ||
                s->active->Find(viewer, creature).found ||
                !s->active->MayAnswerPositiveSuppression(viewer, creature, incarnation, facts))
                return {};
            Reservation r = _gate.Reserve(h);
            if (r)
                s->pending = {r.token, viewer, creature, incarnation, false};
            return r;
        }

        // Only the genuine source-attributed decision may call Commit, with
        // freshly authenticated identity/facts. A reported positive effect
        // that cannot be matched must close the process-wide model fence.
        CommitResult Commit(Reservation r, VRP::ViewerKey viewer, VRP::CreatureKey creature,
                            std::uint64_t incarnation, bool wasLinked, VRP::VisibilityPath path,
                            VRP::DecisionFacts const& facts)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(r.shard);
            if (!s || !Matches(*s, r))
            {
                _unattributedCommitRisk = true;
                _gate.CloseFence();
                return CommitResult::Rejected;
            }
            bool const same = viewer == s->pending.viewer && creature == s->pending.creature &&
                incarnation == s->pending.incarnation && !s->pending.invalidated;
            if (!same)
                return FailCommit(*s, r);
            auto const outcome = s->active->ObserveF14SuppressionDecision(
                viewer, creature, incarnation, wasLinked, path, facts);
            if (outcome != VRP::Outcome::SuppressionRecorded)
                return FailCommit(*s, r);
            if (!_gate.Settle(r, Accounting::Settlement::Recorded))
                return FailCommit(*s, r);
            s->pending = {};
            return CommitResult::Recorded;
        }

        // Cancellation is legal ONLY when caller proves no positive effect.
        // It cannot erase a recorded or uncertain decision.
        bool CancelBeforeEffect(Reservation r)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(r.shard);
            if (!s || !Matches(*s, r) || s->uncertain ||
                !_gate.Settle(r, Accounting::Settlement::Unused))
                return false;
            s->pending = {};
            return true;
        }

        bool RevokeCreature(Handle h, VRP::CreatureKey creature)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(h);
            if (!s || !SameMap(*s, creature))
                return false;
            if (s->pending && s->pending.creature == creature)
                s->pending.invalidated = true;
            s->active->ObserveCreatureRetired(creature);
            return true;
        }

        bool RevokeViewer(Handle h, std::uint64_t viewerGuid)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(h);
            if (!s || !viewerGuid)
                return false;
            if (s->pending && s->pending.viewer.guidRaw == viewerGuid)
                s->pending.invalidated = true;
            s->active->ObserveViewerEraOrRightsChanged(viewerGuid);
            return true;
        }

        VRP::Outcome ObserveLink(Handle h, VRP::ViewerKey viewer, VRP::CreatureKey creature,
                                 std::uint64_t incarnation, bool nowLinked,
                                 VRP::VisibilityPath path, VRP::TransitionFacts const& facts)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(h);
            if (!s || !SameMap(*s, creature))
                return VRP::Outcome::IgnoredWrongMap;
            return s->active->ObserveLinkTransition(viewer, creature, incarnation, nowLinked, path, facts);
        }

        // Seal the whole original Protocol, not a pending-pair export.
        // Storage already exists; unique_ptr<const Protocol> acquires sole
        // ownership without allocating or copying. No mutable aliases escape.
        // Only an empty, clean, quiescent shard can return its slot.
        SealResult Seal(Handle h, bool quiescent)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(h);
            if (!s)
                return SealResult::Rejected;
            s->active->ObserveMapUnloaded(s->mapId, s->instanceId);
            if (s->pending || !quiescent)
            {
                // A possibly applied effect may exist only in pending metadata.
                _unattributedCommitRisk = true;
                s->uncertain = true;
                s->active->ObserveConfigDisabled();
            }
            auto const& ledger = s->active->LedgerView();
            bool const clean = !s->pending && !s->uncertain && !s->active->Size() &&
                !ledger.Size() && !ledger.IsPoisoned() && !ledger.IsFrozen() &&
                !s->active->UnattributedHideRisk();
            SealResult const result = _gate.Seal(h, quiescent, clean);
            if (result == SealResult::Retained)
                s->retained = std::move(s->active);
            else if (result == SealResult::CleanReleased)
                *s = Slot{};
            return result;
        }

        Snapshot Inspect() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _gate.Inspect();
        }
        Status Inspect(Handle h) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot const* s = Find(h);
            if (!s)
                return {};
            Protocol const& p = s->active ? *s->active : *s->retained;
            auto const& ledger = p.LedgerView();
            return {true, bool(s->retained), bool(s->pending), s->uncertain,
                ledger.IsFrozen(), p.UnattributedHideRisk(), p.Size(), ledger.Size(),
                ledger.PendingRestores(), s->pending.viewer, s->pending.creature,
                s->pending.incarnation};
        }
        // Retained evidence is the whole tuple: original Protocol, pending
        // identity, uncertain flag and process-wide unknown-effect risk.
        // A missing Protocol record alone cannot prove a hide did not occur.
        // Known evidence remains available even if its classification is uncertain.
        EvidenceRead ReadEvidence(Handle h, VRP::ViewerKey viewer, VRP::CreatureKey creature) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot const* s = Find(h);
            if (!s)
                return {_unattributedCommitRisk ? EvidenceState::Uncertain : EvidenceState::Missing, {}};
            Protocol const& p = s->active ? *s->active : *s->retained;
            auto const evidence = p.Find(viewer, creature);
            bool const uncertain = _unattributedCommitRisk || s->uncertain || p.UnattributedHideRisk() ||
                (s->pending && s->pending.viewer == viewer && s->pending.creature == creature);
            return {uncertain ? EvidenceState::Uncertain :
                evidence.found ? EvidenceState::Known : EvidenceState::Missing, evidence};
        }

        bool HasUnattributedCommitRisk() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _unattributedCommitRisk;
        }

    private:
        struct Pending
        {
            std::uint64_t token = 0;
            VRP::ViewerKey viewer{};
            VRP::CreatureKey creature{};
            std::uint64_t incarnation = 0;
            bool invalidated = false;
            explicit operator bool() const { return token != 0; }
        };
        struct Slot
        {
            Handle handle{};
            std::uint32_t mapId = 0, instanceId = 0;
            std::unique_ptr<Protocol> active{};
            std::unique_ptr<Protocol const> retained{};
            Pending pending{};
            bool uncertain = false;
        };
        static bool SameMap(Slot const& s, VRP::CreatureKey creature)
        {
            return creature.mapId == s.mapId && creature.instanceId == s.instanceId;
        }
        static bool Matches(Slot const& s, Reservation r)
        {
            return r.token != 0 && s.pending.token == r.token && !s.uncertain;
        }
        Slot const* Find(Handle h) const
        {
            if (!h) return nullptr;
            Slot const& s = _slots[h.slot];
            return s.handle.generation == h.generation && (s.active || s.retained) ? &s : nullptr;
        }
        Slot* Active(Handle h)
        {
            if (!h) return nullptr;
            Slot& s = _slots[h.slot];
            return s.handle.generation == h.generation && s.active ? &s : nullptr;
        }
        CommitResult FailCommit(Slot& s, Reservation r)
        {
            _unattributedCommitRisk = true;
            s.uncertain = true;
            s.active->ObserveConfigDisabled();
            _gate.Settle(r, Accounting::Settlement::Uncertain);
            _gate.CloseFence();
            // Keep the originally reserved identity for diagnostics/retention.
            return CommitResult::Uncertain;
        }

        mutable std::mutex _mutex;
        Accounting _gate{};
        std::array<Slot, Shards> _slots{};
        bool _unattributedCommitRisk = false;
    };
}
#endif
