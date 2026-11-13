#ifndef INDIVIDUAL_PROGRESSION_F14_CURRENT_PASS_IDENTITY_H
#define INDIVIDUAL_PROGRESSION_F14_CURRENT_PASS_IDENTITY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace IndividualProgressionF14CurrentPassIdentity
{
    struct MapKey
    {
        std::uint32_t mapId = 0, instanceId = 0;
        bool operator==(MapKey const& other) const
        {
            return mapId == other.mapId && instanceId == other.instanceId;
        }
    };

    struct ViewerIdentity
    {
        std::uint64_t guid = 0, loginEpoch = 0, mapGeneration = 0;
        MapKey map{};
        explicit operator bool() const { return guid && loginEpoch && mapGeneration; }
        bool operator==(ViewerIdentity const& other) const
        {
            return guid == other.guid && loginEpoch == other.loginEpoch &&
                mapGeneration == other.mapGeneration && map == other.map;
        }
    };

    enum class Result : std::uint8_t
    {
        Recorded, BatchObserved, Ended, Refused, Mismatch, Quarantined, Poisoned
    };

    struct Snapshot
    {
        std::size_t active = 0, closed = 0;
        std::uint64_t begins = 0, batches = 0, ends = 0, revocations = 0;
        std::uint64_t mismatches = 0, refusals = 0;
        bool admissionsClosed = false, poisoned = false;
    };

    // Closed tokens and local quarantine remain for this tracker lifetime.
    // Numerical token order is not callback order across concurrent map workers.
    // Exhausted bounded evidence closes new admission without evicting live slots.
    template <std::size_t Capacity = 64, std::size_t ClosedCapacity = Capacity * 4>
    class Tracker
    {
    public:
        static_assert(Capacity > 0 && ClosedCapacity > 0, "pass tracker needs capacity");

        Result Begin(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned)
                return Result::Poisoned;
            if (!token || !identity)
                return RefuseLocked();
            if (Slot* prior = FindToken(token))
            {
                if (prior->identity.guid != identity.guid)
                    return PoisonLocked(); // Globally unique token has two owners.
                prior->quarantined = true;
                ++_mismatches;
                return Result::Mismatch;
            }
            if (Closed const* prior = FindClosed(token))
            {
                if (prior->identity && prior->identity == identity)
                    QuarantineLocked(identity);
                return RefuseLocked();
            }
            if (IsQuarantinedLocked(identity))
            {
                RememberClosedLocked(token, identity, true);
                return Result::Quarantined;
            }
            for (Slot& slot : _slots)
                if (slot.used && slot.identity.guid == identity.guid)
                {
                    slot.quarantined = true;
                    RememberClosedLocked(token, identity, true);
                    ++_mismatches;
                    return Result::Mismatch; // Conservative nested-pass refusal.
                }
            if (_admissionsClosed)
                return RefuseLocked();
            for (Slot& slot : _slots)
                if (!slot.used)
                {
                    slot = {true, false, false, token, identity};
                    ++_begins;
                    return Result::Recorded;
                }
            RememberClosedLocked(token, identity, false);
            return RefuseLocked();
        }

        Result ObserveBatch(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned)
                return Result::Poisoned;
            if (!token)
                return RefuseLocked();
            Slot* slot = FindToken(token);
            if (!slot)
                return UnknownLocked(token, identity);
            if (!identity || !(slot->identity == identity))
            {
                slot->quarantined = true;
                ++_mismatches;
                return Result::Mismatch;
            }
            if (slot->quarantined || IsQuarantinedLocked(identity))
                return Result::Quarantined;
            slot->batchObserved = true;
            ++_batches;
            return Result::BatchObserved;
        }

        bool IsActive(std::uint64_t token, ViewerIdentity identity) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned || !token || !identity)
                return false;
            Slot const* slot = FindToken(token);
            return slot && !slot->quarantined && slot->identity == identity &&
                !IsQuarantinedLocked(identity);
        }

        bool BatchWasObserved(std::uint64_t token, ViewerIdentity identity) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned || !token || !identity)
                return false;
            Slot const* slot = FindToken(token);
            return slot && !slot->quarantined && slot->identity == identity &&
                !IsQuarantinedLocked(identity) && slot->batchObserved;
        }

        Result End(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!token)
                return RefuseLocked();
            Slot* slot = FindToken(token);
            if (!slot)
                return UnknownLocked(token, identity);
            ViewerIdentity const owner = slot->identity;
            bool const mismatch = !identity || !(owner == identity);
            bool const quarantined = slot->quarantined || IsQuarantinedLocked(owner);
            RememberClosedLocked(token, owner, mismatch || quarantined);
            *slot = {};
            if (_poisoned)
                return Result::Poisoned;
            if (mismatch)
            {
                ++_mismatches;
                return Result::Mismatch;
            }
            if (quarantined)
                return Result::Quarantined;
            ++_ends;
            return Result::Ended;
        }

        // An event-time identity is required. GUID-only revocation is unsafe
        // because a delayed old-session event can erase a newer pass.
        std::size_t RevokeViewer(ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!identity)
                return 0;
            std::size_t count = 0;
            for (Slot& slot : _slots)
                if (slot.used && slot.identity == identity)
                {
                    RememberClosedLocked(slot.token, identity, false);
                    slot = {};
                    ++count;
                }
            _revocations += count;
            return count;
        }

        // Compatibility with the existing runtime observer until CS01 can
        // supply a captured event-time identity. Refusing is safer than
        // deleting a pass from an unrelated login epoch or map generation.
        std::size_t RevokeViewer(std::uint64_t)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            ++_refusals;
            return 0;
        }

        // Reserved for independently proven whole-table corruption.
        void PoisonForGlobalCorruption()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _poisoned = true;
        }

        Snapshot Inspect() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Snapshot out;
            for (Slot const& slot : _slots)
                if (slot.used)
                    ++out.active;
            out.closed = _closedCount;
            out.begins = _begins;
            out.batches = _batches;
            out.ends = _ends;
            out.revocations = _revocations;
            out.mismatches = _mismatches;
            out.refusals = _refusals;
            out.admissionsClosed = _admissionsClosed;
            out.poisoned = _poisoned;
            return out;
        }

    private:
        struct Slot
        {
            bool used = false, batchObserved = false, quarantined = false;
            std::uint64_t token = 0;
            ViewerIdentity identity{};
        };
        struct Closed
        {
            std::uint64_t token = 0;
            ViewerIdentity identity{};
            bool quarantined = false;
        };
        Slot* FindToken(std::uint64_t token)
        {
            for (Slot& slot : _slots)
                if (slot.used && slot.token == token)
                    return &slot;
            return nullptr;
        }
        Slot const* FindToken(std::uint64_t token) const
        {
            for (Slot const& slot : _slots)
                if (slot.used && slot.token == token)
                    return &slot;
            return nullptr;
        }
        Closed const* FindClosed(std::uint64_t token) const
        {
            for (std::size_t i = 0; i < _closedCount; ++i)
                if (_closed[i].token == token)
                    return &_closed[i];
            return nullptr;
        }
        bool IsQuarantinedLocked(ViewerIdentity identity) const
        {
            for (Slot const& slot : _slots)
                if (slot.used && slot.quarantined && slot.identity == identity)
                    return true;
            for (std::size_t i = 0; i < _closedCount; ++i)
                if (_closed[i].quarantined && _closed[i].identity == identity)
                    return true;
            return false;
        }
        void QuarantineLocked(ViewerIdentity identity)
        {
            for (Slot& slot : _slots)
                if (slot.used && slot.identity == identity)
                    slot.quarantined = true;
            for (std::size_t i = 0; i < _closedCount; ++i)
                if (_closed[i].identity == identity)
                    _closed[i].quarantined = true;
        }
        void RememberClosedLocked(std::uint64_t token, ViewerIdentity identity, bool quarantined)
        {
            for (std::size_t i = 0; i < _closedCount; ++i)
                if (_closed[i].token == token)
                {
                    _closed[i].quarantined |= quarantined;
                    return;
                }
            if (_closedCount == ClosedCapacity)
            {
                _admissionsClosed = true;
                return;
            }
            _closed[_closedCount++] = {token, identity, quarantined};
            if (_closedCount == ClosedCapacity)
                _admissionsClosed = true;
        }
        Result UnknownLocked(std::uint64_t token, ViewerIdentity identity)
        {
            if (Closed const* prior = FindClosed(token))
            {
                if (identity && prior->identity == identity)
                    QuarantineLocked(identity);
            }
            else
            {
                RememberClosedLocked(token, identity, bool(identity));
                if (identity)
                    QuarantineLocked(identity);
            }
            return _poisoned ? Result::Poisoned : RefuseLocked();
        }
        Result RefuseLocked()
        {
            ++_refusals;
            return Result::Refused;
        }
        Result PoisonLocked()
        {
            _poisoned = true;
            return Result::Poisoned;
        }

        mutable std::mutex _mutex;
        std::array<Slot, Capacity> _slots{};
        std::array<Closed, ClosedCapacity> _closed{};
        std::size_t _closedCount = 0;
        std::uint64_t _begins = 0, _batches = 0, _ends = 0, _revocations = 0;
        std::uint64_t _mismatches = 0, _refusals = 0;
        bool _admissionsClosed = false, _poisoned = false;
    };
}

#endif
