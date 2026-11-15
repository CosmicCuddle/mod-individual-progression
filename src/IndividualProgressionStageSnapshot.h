/*
 * CosmicCuddle Individual Progression: cache-only stage snapshot (API v1).
 *
 * For callers on threads that must not touch the database or a foreign
 * Player (Playerbots AutoGear, Coordination #59). IP publishes one immutable
 * value per online character from the thread that owns that character; a
 * reader copies it under a shared lock. Anything but Status::Known means
 * "refuse": IP never returns a guessed or default stage as Known.
 *
 * Pure: no Player, DB or Core dependency (the registry below is the storage
 * IP wraps with its own lock).
 */

#ifndef INDIVIDUAL_PROGRESSION_STAGE_SNAPSHOT_H
#define INDIVIDUAL_PROGRESSION_STAGE_SNAPSHOT_H

#include <cstdint>
#include <unordered_map>

#define COSMIC_IP_STAGE_SNAPSHOT_API_VERSION 1

namespace IndividualProgressionStageSnapshot
{
    constexpr std::uint8_t LastSupportedStage = 18;

    enum class Status : std::uint8_t { Known, Unknown };

    enum class UnknownReason : std::uint8_t
    {
        None,
        IpDisabled,        // Individual Progression is disabled
        NotPublished,      // never published, or logged out
        Invalidated,       // config reload / account-rule change; awaiting republish
        AccountUnresolved, // account class not in IP's cache when publishing
        CorruptStage       // raw marker outside 0..18
    };

    struct Snapshot
    {
        Status status = Status::Unknown;
        UnknownReason reason = UnknownReason::NotPublished;
        std::uint8_t rawStage = 0;       // highest rewarded IP marker (completed content)
        std::uint8_t effectiveStage = 0; // after the paid cap; == raw when paid OFF or ungated
        bool gatedHuman = false;         // normal (non-bot, non-excluded) account
        std::uint32_t generation = 0;    // registry generation at publication (diagnostics)
    };

    // What the publisher knows about the character on its owning thread.
    struct PublishInput
    {
        bool ipEnabled = false;
        bool accountResolved = false; // account class found in IP's cache (or resolved on the world thread)
        bool gatedHuman = false;
        std::uint8_t rawStage = 0;
        std::uint8_t effectiveStage = 0;
    };

    inline Snapshot Make(PublishInput const& in, std::uint32_t generation)
    {
        Snapshot s;
        s.generation = generation;
        if (!in.ipEnabled)
        {
            s.reason = UnknownReason::IpDisabled;
            return s;
        }
        if (!in.accountResolved)
        {
            s.reason = UnknownReason::AccountUnresolved;
            return s;
        }
        if (in.rawStage > LastSupportedStage || in.effectiveStage > in.rawStage)
        {
            s.reason = UnknownReason::CorruptStage;
            return s;
        }
        s.status = Status::Known;
        s.reason = UnknownReason::None;
        s.rawStage = in.rawStage;
        s.effectiveStage = in.effectiveStage;
        s.gatedHuman = in.gatedHuman;
        return s;
    }

    // Storage semantics. IP holds one Registry under its own shared_mutex:
    // Get under a shared lock, every mutation under a unique lock.
    class Registry
    {
    public:
        // Read: copy only; a miss is NotPublished; never inserts.
        Snapshot Get(std::uint32_t lowGuid) const
        {
            auto const it = _entries.find(lowGuid);
            if (it == _entries.end())
                return Snapshot{};
            return it->second;
        }

        void Publish(std::uint32_t lowGuid, PublishInput const& in)
        {
            _entries[lowGuid] = Make(in, _generation);
        }

        void Erase(std::uint32_t lowGuid) { _entries.erase(lowGuid); }

        // Config reload / account-rule change: every entry becomes Unknown
        // until its owner republishes it.
        void InvalidateAll()
        {
            ++_generation;
            for (auto& [guid, snapshot] : _entries)
            {
                snapshot.status = Status::Unknown;
                snapshot.reason = UnknownReason::Invalidated;
                snapshot.generation = _generation;
            }
        }

        std::uint32_t Generation() const { return _generation; }
        std::size_t Size() const { return _entries.size(); }

    private:
        std::unordered_map<std::uint32_t, Snapshot> _entries;
        std::uint32_t _generation = 1;
    };
}

#endif
