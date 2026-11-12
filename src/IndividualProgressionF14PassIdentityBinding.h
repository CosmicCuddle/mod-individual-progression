#ifndef INDIVIDUAL_PROGRESSION_F14_PASS_IDENTITY_BINDING_H
#define INDIVIDUAL_PROGRESSION_F14_PASS_IDENTITY_BINDING_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace IndividualProgressionF14PassIdentityBinding
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
        Begun,
        Current,
        Ended,
        Missing,
        Replayed,
        Mismatch,
        Invalidated,
        Refused,
        Poisoned
    };

    struct Snapshot
    {
        std::uint64_t highwater = 0;
        std::size_t active = 0;
        std::size_t invalidated = 0;
        bool poisoned = false;
    };

    template <std::size_t Capacity = 64>
    class Registry
    {
    public:
        static_assert(Capacity > 0, "pass identity registry requires capacity");

        Result Begin(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> guard(_mutex);
            if (_poisoned)
                return Result::Poisoned;
            if (!token || !identity)
                return Result::Refused;
            if (token <= _highwater)
                return Result::Replayed;

            Slot* slot = FindFree();
            if (!slot)
            {
                _poisoned = true;
                return Result::Poisoned;
            }

            _highwater = token;
            slot->used = true;
            slot->active = true;
            slot->invalidated = false;
            slot->token = token;
            slot->identity = identity;
            return Result::Begun;
        }

        Result Validate(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> guard(_mutex);
            if (_poisoned)
                return Result::Poisoned;
            Slot* slot = Find(token);
            if (!slot)
                return token && token <= _highwater ? Result::Replayed : Result::Missing;
            if (!slot->active)
                return Result::Replayed;
            if (slot->invalidated)
                return Result::Invalidated;
            if (!identity || !(slot->identity == identity))
            {
                slot->invalidated = true;
                return Result::Mismatch;
            }
            return Result::Current;
        }

        Result End(std::uint64_t token, ViewerIdentity identity)
        {
            std::lock_guard<std::mutex> guard(_mutex);
            if (_poisoned)
                return Result::Poisoned;
            Slot* slot = Find(token);
            if (!slot)
                return token && token <= _highwater ? Result::Replayed : Result::Missing;
            if (!slot->active)
                return Result::Replayed;

            Result result = Result::Ended;
            if (slot->invalidated)
                result = Result::Invalidated;
            else if (!identity || !(slot->identity == identity))
                result = Result::Mismatch;

            slot->active = false;
            slot->invalidated = false;
            slot->identity = {};
            return result;
        }

        std::size_t RevokeViewer(std::uint64_t guid, std::uint64_t loginEpoch)
        {
            std::lock_guard<std::mutex> guard(_mutex);
            if (!guid || !loginEpoch)
                return 0;
            std::size_t count = 0;
            for (Slot& slot : _slots)
            {
                if (slot.used && slot.active &&
                    slot.identity.guid == guid && slot.identity.loginEpoch == loginEpoch)
                {
                    slot.invalidated = true;
                    ++count;
                }
            }
            return count;
        }

        Snapshot Inspect() const
        {
            std::lock_guard<std::mutex> guard(_mutex);
            Snapshot out;
            out.highwater = _highwater;
            out.poisoned = _poisoned;
            for (Slot const& slot : _slots)
            {
                if (!slot.used || !slot.active)
                    continue;
                ++out.active;
                if (slot.invalidated)
                    ++out.invalidated;
            }
            return out;
        }

    private:
        struct Slot
        {
            bool used = false;
            bool active = false;
            bool invalidated = false;
            std::uint64_t token = 0;
            ViewerIdentity identity{};
        };

        Slot* Find(std::uint64_t token)
        {
            for (Slot& slot : _slots)
                if (slot.used && slot.token == token)
                    return &slot;
            return nullptr;
        }

        Slot* FindFree()
        {
            for (Slot& slot : _slots)
                if (!slot.used || !slot.active)
                    return &slot;
            return nullptr;
        }

        mutable std::mutex _mutex;
        std::array<Slot, Capacity> _slots{};
        std::uint64_t _highwater = 0;
        bool _poisoned = false;
    };
}

#endif