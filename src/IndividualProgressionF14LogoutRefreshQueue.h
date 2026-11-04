/*
 * CosmicCuddle IP F14 — bounded, value-only logout -> map-thread refresh handoff.
 *
 * PURE helper: no Unit*, Map*, packets, playerbots, SQL, or visibility rights.
 * Queue capacity exhaustion poisons the request channel: a future real
 * hide implementation MUST additionally globally fail visible and restore
 * prior viewer links before enabling concealment. Current hide is OFF.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_LOGOUT_REFRESH_QUEUE_H
#define INDIVIDUAL_PROGRESSION_F14_LOGOUT_REFRESH_QUEUE_H

#include "IndividualProgressionF14LifecycleRegistry.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14LogoutRefreshQueue
{
    namespace Registry = IndividualProgressionF14LifecycleRegistry;

    struct Request
    {
        Registry::CreatureKey key{};
        std::uint64_t stamp = 0;
    };

    template<std::size_t Capacity = Registry::DefaultMaxCreatures>
    class Queue
    {
        static_assert(Capacity > 0, "F14 pending logout queue must be bounded and nonempty");

    public:
        void Reset()
        {
            _entries = {};
            _count = 0;
            _poisoned = false;
        }

        bool IsPoisoned() const { return _poisoned; }
        std::size_t Size() const { return _count; }
        bool HasPending() const { return _count != 0 && !_poisoned; }

        // Called only while owning the SAME lifecycle mutex as the registry.
        // Never store a Creature/Map/Player pointer or a recyclable spawn ID.
        bool Remember(Registry::CreatureKey const& key, std::uint64_t stamp)
        {
            if (_poisoned || !key.IsValid() || stamp == 0)
                return false;

            for (std::size_t i = 0; i < _count; ++i)
                if (_entries[i].key == key)
                {
                    // A fresh creature incarnation supersedes a stale request.
                    _entries[i].stamp = stamp;
                    return true;
                }

            if (_count == Capacity)
            {
                // Dropping one request is NOT approval to conceal anything.
                // Permanent poison requires an explicit owner-reviewed reset.
                _poisoned = true;
                _count = 0;
                return false;
            }
            _entries[_count++] = {key, stamp};
            return true;
        }

        // Called only from the target's own map-thread Unit::Update, with
        // same lifecycle mutex. A recycled GUID or retired stamp cannot match.
        bool Take(Registry::CreatureKey const& key, std::uint64_t currentStamp)
        {
            if (_poisoned || !key.IsValid())
                return false;
            for (std::size_t i = 0; i < _count; ++i)
                if (_entries[i].key == key)
                {
                    bool const sameIncarnation =
                        currentStamp != 0 && _entries[i].stamp == currentStamp;
                    _entries[i] = _entries[_count - 1];
                    _entries[--_count] = {};
                    return sameIncarnation;
                }
            return false;
        }

        // Creature removal/map unload retires requests without dereferencing
        // any stale Unit. Existing clients must eventually receive removal
        // through Core; this queue cannot prove it.
        void Discard(Registry::CreatureKey const& key)
        {
            if (!key.IsValid())
                return;
            for (std::size_t i = 0; i < _count; ++i)
                if (_entries[i].key == key)
                {
                    _entries[i] = _entries[_count - 1];
                    _entries[--_count] = {};
                    return;
                }
        }

    private:
        std::array<Request, Capacity> _entries{};
        std::size_t _count = 0;
        bool _poisoned = false;
    };
}

#endif
