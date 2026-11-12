#ifndef INDIVIDUAL_PROGRESSION_F14_CURRENT_VIEWER_SESSION_H
#define INDIVIDUAL_PROGRESSION_F14_CURRENT_VIEWER_SESSION_H
#include <cstdint>
#include <mutex>

namespace IndividualProgressionF14CurrentViewerSession
{
    struct MapKey
    {
        std::uint32_t mapId = 0, instanceId = 0;
        bool operator==(MapKey const& o) const { return mapId == o.mapId && instanceId == o.instanceId; }
    };
    struct Identity
    {
        std::uint64_t guid = 0, epoch = 0, generation = 0;
        MapKey map{};
        explicit operator bool() const { return guid && epoch && generation; }
    };
    // Pure session metadata, no Core pointers or visibility/debt authority.
    // One object belongs to one current Player CustomData entry. Issuers
    // are external, process-monotonic, non-reused and nonzero.
    class State
    {
    public:
        void BindLogin(std::uint64_t guid, std::uint64_t epoch)
        {
            std::lock_guard<std::mutex> lock(_lock);
            _identity = guid && epoch ? Identity{guid,epoch,0,{}} : Identity{};
            _lastGeneration = 0;
            _blocked = false;
        }
        bool Enter(std::uint64_t guid, MapKey map, std::uint64_t generation)
        {
            std::lock_guard<std::mutex> lock(_lock);
            if (!_identity.epoch || _blocked)
                return false;
            if (guid != _identity.guid || !generation || generation <= _lastGeneration ||
                _identity.generation)
            {
                InvalidateLocked();
                return false;
            }
            _identity.map = map;
            _identity.generation = generation;
            _lastGeneration = generation;
            return true;
        }
        void Leave(std::uint64_t guid, MapKey map)
        {
            std::lock_guard<std::mutex> lock(_lock);
            if (!_identity.generation)
                return;
            if (guid != _identity.guid || !(_identity.map == map))
                InvalidateLocked();
            else
            {
                _identity.generation = 0;
                _identity.map = {};
            }
        }
        void InvalidateMap()
        {
            std::lock_guard<std::mutex> lock(_lock);
            InvalidateLocked();
        }
        void Logout()
        {
            std::lock_guard<std::mutex> lock(_lock);
            _identity = {};
            _lastGeneration = 0;
            _blocked = true;
        }
        Identity Read(std::uint64_t guid, MapKey map) const
        {
            std::lock_guard<std::mutex> lock(_lock);
            if (_blocked || !_identity || _identity.guid != guid || !(_identity.map == map))
                return {};
            return _identity;
        }
    private:
        void InvalidateLocked()
        {
            _identity.generation = 0;
            _identity.map = {};
            _blocked = true;
        }
        mutable std::mutex _lock;
        Identity _identity{};
        std::uint64_t _lastGeneration = 0;
        bool _blocked = true;
    };
}
#endif
