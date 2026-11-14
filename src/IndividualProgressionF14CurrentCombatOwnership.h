#ifndef INDIVIDUAL_PROGRESSION_F14_CURRENT_COMBAT_OWNERSHIP_H
#define INDIVIDUAL_PROGRESSION_F14_CURRENT_COMBAT_OWNERSHIP_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace IndividualProgressionF14CurrentCombatOwnership
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

    struct ActorKey
    {
        std::uint64_t guid = 0;
        std::uint64_t loginEpoch = 0;
        std::uint64_t mapGeneration = 0;
        MapKey map{};

        explicit operator bool() const
        {
            return guid && loginEpoch && mapGeneration;
        }

        bool operator==(ActorKey const& other) const
        {
            return guid == other.guid &&
                loginEpoch == other.loginEpoch &&
                mapGeneration == other.mapGeneration &&
                map == other.map;
        }
    };

    struct CreatureKey
    {
        std::uint64_t guid = 0;
        std::uint64_t incarnation = 0;
        MapKey map{};

        explicit operator bool() const
        {
            return guid && incarnation;
        }

        bool operator==(CreatureKey const& other) const
        {
            return guid == other.guid &&
                incarnation == other.incarnation &&
                map == other.map;
        }
    };

    enum class ObserveResult : std::uint8_t
    {
        Recorded,
        Unchanged,
        Replaced,
        Stale,
        Refused,
        Poisoned
    };

    struct ActorState
    {
        bool found = false;
        bool active = false;
        ActorKey actor{};
        CreatureKey creature{};
    };

    struct Snapshot
    {
        std::size_t actors = 0;
        std::size_t activeRelations = 0;
        bool poisoned = false;
    };

    template <std::size_t Capacity = 64>
    class Tracker
    {
    public:
        static_assert(Capacity > 0, "combat ownership tracker requires capacity");

        ObserveResult Observe(ActorKey actor, CreatureKey creature)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned)
                return ObserveResult::Poisoned;
            if (!actor || !creature || !(actor.map == creature.map))
                return ObserveResult::Refused;

            Slot* slot = FindActor(actor.guid);
            if (!slot)
            {
                slot = FindFree();
                if (!slot)
                {
                    _poisoned = true;
                    return ObserveResult::Poisoned;
                }
                slot->used = true;
                slot->actor = actor;
            }
            else
            {
                if (actor.loginEpoch < slot->actor.loginEpoch ||
                    (actor.loginEpoch == slot->actor.loginEpoch &&
                     actor.mapGeneration < slot->actor.mapGeneration))
                    return ObserveResult::Stale;

                if (actor.loginEpoch == slot->actor.loginEpoch &&
                    actor.mapGeneration == slot->actor.mapGeneration &&
                    !(actor.map == slot->actor.map))
                {
                    slot->active = false;
                    slot->creature = {};
                    _poisoned = true;
                    return ObserveResult::Poisoned;
                }

                if (actor.loginEpoch > slot->actor.loginEpoch ||
                    actor.mapGeneration > slot->actor.mapGeneration)
                {
                    slot->active = false;
                    slot->creature = {};
                    slot->actor = actor;
                }
            }

            if (slot->active && slot->creature == creature)
                return ObserveResult::Unchanged;

            bool const replaced = slot->active;
            slot->actor = actor;
            slot->creature = creature;
            slot->active = true;
            return replaced ? ObserveResult::Replaced : ObserveResult::Recorded;
        }

        bool EndActor(ActorKey actor)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* slot = FindActor(actor.guid);
            if (!slot || !(slot->actor == actor))
                return false;
            bool const changed = slot->active;
            slot->active = false;
            slot->creature = {};
            return changed;
        }
        // Session-lifetime release only. The full actor key must still match
        // so a stale callback cannot free a slot already reused by a newer
        // login epoch or map generation. This never clears sticky poison.
        bool ReleaseActor(ActorKey actor)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* slot = FindActor(actor.guid);
            if (!slot || !(slot->actor == actor))
                return false;
            slot->used = false;
            slot->active = false;
            slot->actor = {};
            slot->creature = {};
            return true;
        }

        // Session cleanup may occur after map leave has deliberately invalidated
        // the map-generation key. Release only the slot owned by the exact
        // verified login epoch; a stale prior session cannot free a newer one.
        // This is cleanup only and never clears sticky poison.
        bool ReleaseActorSession(std::uint64_t guid, std::uint64_t loginEpoch)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!guid || !loginEpoch)
                return false;
            Slot* slot = FindActor(guid);
            if (!slot || slot->actor.loginEpoch != loginEpoch)
                return false;
            slot->used = false;
            slot->active = false;
            slot->actor = {};
            slot->creature = {};
            return true;
        }

        std::size_t RetireCreature(CreatureKey creature)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!creature)
                return 0;
            std::size_t retired = 0;
            for (Slot& slot : _slots)
            {
                if (slot.used && slot.active && slot.creature == creature)
                {
                    slot.active = false;
                    slot.creature = {};
                    ++retired;
                }
            }
            return retired;
        }

        std::size_t ActiveAttackers(CreatureKey creature) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned || !creature)
                return 0;
            std::size_t count = 0;
            for (Slot const& slot : _slots)
                if (slot.used && slot.active && slot.creature == creature)
                    ++count;
            return count;
        }

        ActorState InspectActor(std::uint64_t guid) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_poisoned)
                return {};
            ActorState out;
            Slot const* slot = FindActorConst(guid);
            if (!slot)
                return out;
            out.found = true;
            out.active = slot->active;
            out.actor = slot->actor;
            out.creature = slot->creature;
            return out;
        }

        Snapshot Inspect() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Snapshot out;
            out.poisoned = _poisoned;
            for (Slot const& slot : _slots)
            {
                if (!slot.used)
                    continue;
                ++out.actors;
                if (slot.active)
                    ++out.activeRelations;
            }
            return out;
        }

    private:
        struct Slot
        {
            bool used = false;
            bool active = false;
            ActorKey actor{};
            CreatureKey creature{};
        };

        Slot* FindActor(std::uint64_t guid)
        {
            for (Slot& slot : _slots)
                if (slot.used && slot.actor.guid == guid)
                    return &slot;
            return nullptr;
        }

        Slot const* FindActorConst(std::uint64_t guid) const
        {
            for (Slot const& slot : _slots)
                if (slot.used && slot.actor.guid == guid)
                    return &slot;
            return nullptr;
        }

        Slot* FindFree()
        {
            for (Slot& slot : _slots)
                if (!slot.used)
                    return &slot;
            return nullptr;
        }

        mutable std::mutex _mutex;
        std::array<Slot, Capacity> _slots{};
        bool _poisoned = false;
    };
}

#endif