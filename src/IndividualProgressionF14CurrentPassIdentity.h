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
        std::uint32_t mapId = 0;
        std::uint32_t instanceId = 0;
        bool operator==(MapKey const& other) const
        {
            return mapId == other.mapId && instanceId == other.instanceId;
        }
    };

    struct ViewerIdentity
    {
        std::uint64_t guid = 0;
        std::uint64_t loginEpoch = 0;
        std::uint64_t mapGeneration = 0;
        MapKey map{};
        explicit operator bool() const
        {
            return guid && loginEpoch && mapGeneration;
        }
        bool operator==(ViewerIdentity const& other) const
        {
            return guid == other.guid && loginEpoch == other.loginEpoch &&
                mapGeneration == other.mapGeneration && map == other.map;
        }
    };

    enum class Result : std::uint8_t
    {
        Recorded,
        BatchObserved,
        Ended,
        Refused,
        Mismatch,
        Poisoned
    };

    struct Snapshot
    {
        std::size_t active = 0;
        std::uint64_t begins = 0;
        std::uint64_t batches = 0;
        std::uint64_t ends = 0;
        std::uint64_t revocations = 0;
        std::uint64_t mismatches = 0;
        bool poisoned = false;
    };

    template <std::size_t Capacity = 64>
    class Tracker
    {
    public:
        static_assert(Capacity > 0, "pass identity tracker requires capacity");

        Result Begin(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned)
                return Result::Poisoned;
            if (!token || !identity)
                return Result::Refused;

            for (Slot const& slot : _slots)
            {
                if (!slot.used)
                    continue;
                if (slot.token == token || slot.identity.guid == identity.guid)
                    return PoisonLocked();
            }

            Slot* free = nullptr;
            for (Slot& slot : _slots)
                if (!slot.used)
                {
                    free = &slot;
                    break;
                }
            if (!free)
                return PoisonLocked();

            free->used = true;
            free->token = token;
            free->identity = identity;
            free->batchObserved = false;
            ++_begins;
            return Result::Recorded;
        }

        Result ObserveBatch(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned)
                return Result::Poisoned;
            if (!token || !identity)
                return Result::Refused;
            Slot* slot = FindToken(token);
            if (!slot)
                return Result::Refused;
            if (!(slot->identity == identity))
                return MismatchLocked();
            slot->batchObserved = true;
            ++_batches;
            return Result::BatchObserved;
        }

        bool IsActive(std::uint64_t token, ViewerIdentity identity) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned || !token || !identity)
                return false;
            Slot const* slot = FindTokenConst(token);
            return slot && slot->identity == identity;
        }

        bool BatchWasObserved(std::uint64_t token, ViewerIdentity identity) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned || !token || !identity)
                return false;
            Slot const* slot = FindTokenConst(token);
            return slot && slot->identity == identity && slot->batchObserved;
        }

        Result End(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!token || !identity)
                return Result::Refused;
            Slot* slot = FindToken(token);
            if (!slot)
                return _poisoned ? Result::Poisoned : Result::Refused;
            if (!(slot->identity == identity))
            {
                *slot = {};
                return MismatchLocked();
            }
            *slot = {};
            ++_ends;
            return _poisoned ? Result::Poisoned : Result::Ended;
        }

        std::size_t RevokeViewer(std::uint64_t guid)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!guid)
                return 0;
            std::size_t count = 0;
            for (Slot& slot : _slots)
                if (slot.used && slot.identity.guid == guid)
                {
                    slot = {};
                    ++count;
                }
            _revocations += count;
            return count;
        }

        Snapshot Inspect() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Snapshot out;
            for (Slot const& slot : _slots)
                if (slot.used)
                    ++out.active;
            out.begins = _begins;
            out.batches = _batches;
            out.ends = _ends;
            out.revocations = _revocations;
            out.mismatches = _mismatches;
            out.poisoned = _poisoned;
            return out;
        }

    private:
        struct Slot
        {
            bool used = false;
            bool batchObserved = false;
            std::uint64_t token = 0;
            ViewerIdentity identity{};
        };

        Slot* FindToken(std::uint64_t token)
        {
            for (Slot& slot : _slots)
                if (slot.used && slot.token == token)
                    return &slot;
            return nullptr;
        }

        Slot const* FindTokenConst(std::uint64_t token) const
        {
            for (Slot const& slot : _slots)
                if (slot.used && slot.token == token)
                    return &slot;
            return nullptr;
        }

        Result PoisonLocked()
        {
            _poisoned = true;
            return Result::Poisoned;
        }

        Result MismatchLocked()
        {
            ++_mismatches;
            _poisoned = true;
            return Result::Mismatch;
        }

        mutable std::mutex _mutex;
        std::array<Slot, Capacity> _slots{};
        std::uint64_t _begins = 0;
        std::uint64_t _batches = 0;
        std::uint64_t _ends = 0;
        std::uint64_t _revocations = 0;
        std::uint64_t _mismatches = 0;
        bool _poisoned = false;
    };
}

#endif