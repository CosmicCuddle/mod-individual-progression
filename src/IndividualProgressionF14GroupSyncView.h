#ifndef INDIVIDUAL_PROGRESSION_F14_GROUP_SYNC_VIEW_H
#define INDIVIDUAL_PROGRESSION_F14_GROUP_SYNC_VIEW_H

#include "IndividualProgressionF14GroupSyncEraFloor.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace IndividualProgressionF14GroupSyncView
{
    namespace Floor = IndividualProgressionF14GroupSyncEraFloor;

    // External locking: values only, never Core objects. Each invalidation
    // precedes refresh; a late refresh cannot republish an older revision.
    template<std::size_t Capacity = 4096>
    class Snapshots
    {
        static_assert(Capacity > 0, "group snapshot capacity must be positive");
    public:
        using Work = std::pair<std::uint64_t, std::uint64_t>;
        explicit Snapshots(std::uint64_t initialRevision = 0) : _revision(initialRevision) { }

        void Request(std::uint64_t group)
        {
            if (!group || _failed || _mutationPoisoned)
                return;
            if ((_groups.count(group) == 0 && _groups.size() == Capacity) ||
                _revision == std::numeric_limits<std::uint64_t>::max())
            {
                _failed = true;
                _groups.clear();
                _pending.clear();
                return;
            }
            _groups[group] = Snapshot{++_revision, 0, 0, false};
            _pending.insert(group);
        }

        void EnsureRequested(std::uint64_t group)
        {
            if (_groups.count(group) == 0)
                Request(group);
        }

        std::vector<Work> TakeWork()
        {
            std::vector<Work> work;
            work.reserve(_pending.size());
            for (auto group : _pending)
                work.emplace_back(group, _groups.at(group).revision);
            _pending.clear();
            return work;
        }

        bool Publish(Work const& work, std::uint64_t leader, std::uint8_t stage)
        {
            auto it = _groups.find(work.first);
            if (_failed || _mutationPoisoned || !leader || stage > Floor::GroupPolicy::LastSupportedStage ||
                it == _groups.end() || it->second.revision != work.second)
                return false;
            if (_activeMutations)
            {
                _pending.insert(work.first);
                return false;
            }
            it->second.leader = leader;
            it->second.stage = stage;
            it->second.valid = true;
            return true;
        }

        bool Read(std::uint64_t group, std::uint64_t leader, std::uint8_t& stage) const
        {
            auto const it = _groups.find(group);
            if (_failed || _mutationPoisoned || _activeMutations || !leader ||
                it == _groups.end() || !it->second.valid || it->second.leader != leader)
                return false;
            stage = it->second.stage;
            return true;
        }

        bool ObservationBlocked() const
        {
            return _failed || _mutationPoisoned || _activeMutations != 0;
        }

        void Forget(std::uint64_t group)
        {
            _pending.erase(group);
            _groups.erase(group);
        }

        void Clear()
        {
            _groups.clear();
            _pending.clear();
            _failed = false;
            // Keep revision monotonic: work captured before a reset must not
            // accidentally match a newly requested group.
            // Active mutation pairs and poison survive reload: clearing them
            // could publish an intermediate stage or hide an unbalanced pair.
        }

        void BeginMutation(std::uint64_t group)
        {
            if (_activeMutations == std::numeric_limits<std::uint64_t>::max())
            {
                PoisonMutations();
                return;
            }
            ++_activeMutations;
            Request(group);
        }

        void EndMutation(std::uint64_t group)
        {
            if (!_activeMutations)
            {
                PoisonMutations();
                return;
            }
            --_activeMutations;
            Request(group);
        }

    private:
        void PoisonMutations()
        {
            _mutationPoisoned = true;
            _groups.clear();
            _pending.clear();
        }
        struct Snapshot
        {
            std::uint64_t revision;
            std::uint64_t leader;
            std::uint8_t stage;
            bool valid;
        };
        std::map<std::uint64_t, Snapshot> _groups;
        std::set<std::uint64_t> _pending;
        std::uint64_t _revision;
        bool _failed = false;
        std::uint64_t _activeMutations = 0;
        bool _mutationPoisoned = false;
    };

    // Caller must already prove current racial RandomBot identity and group
    // membership. Pending leader facts cannot confer Wrath, but the approved
    // TBC group floor still applies. This never writes an earned marker.
    constexpr std::uint8_t UnavailableStage(std::uint8_t earned)
    {
        return earned <= Floor::GroupPolicy::LastSupportedStage ? Floor::GroupPolicy::FirstTbcStage : 0;
    }

    inline std::uint8_t EffectiveStage(std::uint8_t race, std::uint8_t earned,
                                      std::uint8_t leaderStage, std::int32_t limit)
    {
        auto const decision = Floor::WhileGrouped(true, race, earned, leaderStage);
        if (!decision || limit < 0 || limit > Floor::GroupPolicy::LastSupportedStage ||
            (limit && limit < Floor::GroupPolicy::FirstTbcStage))
            return UnavailableStage(earned);
        std::uint8_t const desired = decision.temporaryEra == Floor::Era::BurningCrusade &&
            leaderStage < Floor::GroupPolicy::FirstTbcStage ? Floor::GroupPolicy::FirstTbcStage : leaderStage;
        return limit ? std::min(desired, static_cast<std::uint8_t>(limit)) : desired;
    }
}
#endif
