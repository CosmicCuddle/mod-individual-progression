#ifndef INDIVIDUAL_PROGRESSION_F14_GROUP_ERA_CONSUMER_H
#define INDIVIDUAL_PROGRESSION_F14_GROUP_ERA_CONSUMER_H

#include "IndividualProgressionF14GroupSyncEraFloor.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14GroupEraConsumer
{
    namespace Floor = IndividualProgressionF14GroupSyncEraFloor;
    using Era = Floor::Era;

    // These are event-time values supplied by an independently verified caller.
    // Every generation is nonzero and increases before its corresponding context
    // can be reused. Map ID may legitimately be zero (Azeroth).
    struct Identity
    {
        std::uint64_t botGuid = 0;
        std::uint64_t sessionEpoch = 0;
        std::uint64_t mapGeneration = 0;
        std::uint32_t mapId = 0;
        std::uint64_t groupGuid = 0;
        std::uint64_t membershipGeneration = 0;
        std::uint64_t leaderGuid = 0;
        std::uint64_t leaderRevision = 0;
    };

    constexpr bool Complete(Identity const& id)
    {
        return id.botGuid && id.sessionEpoch && id.mapGeneration && id.groupGuid &&
            id.membershipGeneration && id.leaderGuid && id.leaderRevision;
    }

    constexpr bool Same(Identity const& a, Identity const& b)
    {
        return a.botGuid == b.botGuid && a.sessionEpoch == b.sessionEpoch &&
            a.mapGeneration == b.mapGeneration && a.mapId == b.mapId &&
            a.groupGuid == b.groupGuid &&
            a.membershipGeneration == b.membershipGeneration &&
            a.leaderGuid == b.leaderGuid && a.leaderRevision == b.leaderRevision;
    }

    // Ordering is valid only under the caller's monotonic-generation contract.
    // An equal version with different identity facts is ambiguous, not newer.
    constexpr int CompareVersion(Identity const& a, Identity const& b)
    {
        if (a.sessionEpoch != b.sessionEpoch)
            return a.sessionEpoch > b.sessionEpoch ? 1 : -1;
        if (a.mapGeneration != b.mapGeneration)
            return a.mapGeneration > b.mapGeneration ? 1 : -1;
        if (a.membershipGeneration != b.membershipGeneration)
            return a.membershipGeneration > b.membershipGeneration ? 1 : -1;
        if (a.leaderRevision != b.leaderRevision)
            return a.leaderRevision > b.leaderRevision ? 1 : -1;
        return 0;
    }

    enum class Status : std::uint8_t
    {
        Temporary, Earned, EarnedFloorMissing, UnverifiedBot, InvalidIdentity,
        InvalidRace, InvalidStage, Stale, Ambiguous, Capacity, Revoked
    };

    struct View
    {
        Status status = Status::InvalidIdentity;
        Era effectiveEra = Era::Invalid;
        bool temporary = false;
    };

    struct Input
    {
        bool eligibleBot = false;
        Identity identity{};
        std::uint8_t race = 0;
        std::uint8_t earnedStage = 0;
        std::uint8_t groupStage = 0;
    };

    enum class Departure : std::uint8_t
    {
        Leave, Disband, Kick, LeaderChange, Relog, MapChange, SessionInvalidation
    };

    inline View EarnedView(bool eligibleBot, std::uint8_t race, std::uint8_t earnedStage)
    {
        Floor::DepartureEvidence const evidence = Floor::AfterLeaving(eligibleBot, race, earnedStage);
        switch (evidence.status)
        {
            case Floor::Status::Available: return {Status::Earned, evidence.earnedEra, false};
            case Floor::Status::EarnedFloorMissing:
                return {Status::EarnedFloorMissing, evidence.earnedEra, false};
            case Floor::Status::UnverifiedBot: return {Status::UnverifiedBot, Era::Invalid, false};
            case Floor::Status::InvalidRace: return {Status::InvalidRace, Era::Invalid, false};
            default: return {Status::InvalidStage, Era::Invalid, false};
        }
    }

    template<std::size_t Capacity = 64>
    class Tracker
    {
        static_assert(Capacity > 0, "a consumer needs at least one slot");

        struct Entry
        {
            Identity identity{};
            std::uint8_t race = 0;
            std::uint8_t groupStage = 0;
            Era temporaryEra = Era::Invalid;
            bool occupied = false;
            bool active = false;
            bool ambiguous = false;
        };

    public:
        // A matching replay is idempotent. Reusing an equal generation with
        // changed group/leader/race/stage facts quarantines this bot only.
        View ObserveGrouped(Input const& input)
        {
            if (!input.eligibleBot)
                return {Status::UnverifiedBot, Era::Invalid, false};
            if (!Complete(input.identity))
                return {Status::InvalidIdentity, Era::Invalid, false};

            Entry* entry = Find(input.identity.botGuid);
            if (entry)
            {
                int const order = CompareVersion(input.identity, entry->identity);
                if (order < 0)
                    return {Status::Stale, Era::Invalid, false};
                if (order == 0)
                {
                    if (!Same(input.identity, entry->identity) ||
                        (entry->active && (entry->race != input.race ||
                                           entry->groupStage != input.groupStage)))
                    {
                        entry->active = false;
                        entry->ambiguous = true;
                        return {Status::Ambiguous, Era::Invalid, false};
                    }
                    if (entry->ambiguous || !entry->active)
                        return {Status::Ambiguous, Era::Invalid, false};
                }
            }

            Floor::GroupedDecision const decision = Floor::WhileGrouped(
                input.eligibleBot, input.race, input.earnedStage, input.groupStage);
            if (!entry)
            {
                entry = Free();
                if (!entry)
                    return {Status::Capacity, Era::Invalid, false};
            }
            // A newer invalid context is a tombstone: it cannot reveal an old
            // override or be replayed as a valid context at the same version.
            *entry = {input.identity, input.race, input.groupStage, decision.temporaryEra,
                      true, decision.status == Floor::Status::Available, false};
            if (decision.status == Floor::Status::InvalidRace)
                return {Status::InvalidRace, Era::Invalid, false};
            if (decision.status == Floor::Status::InvalidStage)
                return {Status::InvalidStage, Era::Invalid, false};
            return {Status::Temporary, decision.temporaryEra, true};
        }

        // Read requires the same independently verified event-time context.
        // A stale/unknown context never receives another context's override.
        View Current(Input const& input) const
        {
            if (!input.eligibleBot)
                return {Status::UnverifiedBot, Era::Invalid, false};
            if (!Complete(input.identity))
                return {Status::InvalidIdentity, Era::Invalid, false};
            Entry const* entry = Find(input.identity.botGuid);
            if (entry && entry->active && !entry->ambiguous &&
                Same(input.identity, entry->identity) && entry->race == input.race &&
                entry->groupStage == input.groupStage &&
                Floor::WhileGrouped(true, input.race, input.earnedStage, input.groupStage))
                return {Status::Temporary, entry->temporaryEra, true};
            return EarnedView(true, input.race, input.earnedStage);
        }

        // All departure reasons have identical temporary-state semantics.
        // A captured old identity cannot revoke a newer session/group entry.
        View Revoke(Identity const& identity, bool eligibleBot, std::uint8_t race,
                    std::uint8_t earnedStage, Departure)
        {
            // Teardown can run after the caller loses eligibility proof.
            // Exact captured context is sufficient to remove a temporary
            // override; eligibility is needed only for the returned view.
            if (!Complete(identity))
                return {Status::InvalidIdentity, Era::Invalid, false};
            Entry* entry = Find(identity.botGuid);
            if (!entry)
            {
                entry = Free();
                if (!entry)
                    return {Status::Capacity, Era::Invalid, false};
            }
            else
            {
                int const order = CompareVersion(identity, entry->identity);
                if (order < 0)
                    return {Status::Stale, Era::Invalid, false};
                if (order == 0 && !Same(identity, entry->identity))
                {
                    entry->active = false;
                    entry->ambiguous = true;
                    return {Status::Ambiguous, Era::Invalid, false};
                }
            }
            *entry = {identity, race, 0, Era::Invalid, true, false, false};
            View view = EarnedView(eligibleBot, race, earnedStage);
            if (view.status == Status::Earned)
                view.status = Status::Revoked;
            return view;
        }

    private:
        Entry* Find(std::uint64_t guid)
        {
            for (Entry& entry : _entries)
                if (entry.occupied && entry.identity.botGuid == guid)
                    return &entry;
            return nullptr;
        }
        Entry const* Find(std::uint64_t guid) const
        {
            for (Entry const& entry : _entries)
                if (entry.occupied && entry.identity.botGuid == guid)
                    return &entry;
            return nullptr;
        }
        Entry* Free()
        {
            for (Entry& entry : _entries)
                if (!entry.occupied)
                    return &entry;
            return nullptr;
        }
        std::array<Entry, Capacity> _entries{};
    };
}

#endif
