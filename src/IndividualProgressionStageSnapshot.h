/*
 * CosmicCuddle Individual Progression: cache-only stage snapshot (API v1).
 *
 * For callers on threads that must not touch the database or a foreign
 * Player (Playerbots AutoGear, Coordination #59). Anything but Status::Known
 * means "refuse": IP never returns a guessed or default stage as Known.
 *
 * Lifecycle (owner amendments, #59 6892638392):
 *  - Key: the FULL ObjectGuid raw value (uint64).
 *  - Login registers the entry with a fresh process-unique session epoch and
 *    leaves it Unknown(NotPublished) + dirty.
 *  - Every change source INVALIDATES synchronously (Unknown + dirty) before
 *    the change becomes observable; it never reads a Player.
 *  - Only the thread that owns the player (PlayerScript::OnPlayerUpdate)
 *    publishes, and a publish is accepted only for the same epoch and the
 *    same registry generation the publisher started from, so a late writer
 *    from an old session or from before a reload can never resurrect an
 *    erased or invalidated entry.
 *  - Logout erases. Config reload bumps the generation and invalidates all.
 *  - Readers copy a value under a shared lock; a miss never inserts.
 *  - PB re-reads immediately before a destructive operation and proceeds only
 *    if SameKnownSnapshot(first, second).
 *
 * Pure: no Player, DB or Core dependency; IP wraps Registry with its lock.
 */

#ifndef INDIVIDUAL_PROGRESSION_STAGE_SNAPSHOT_H
#define INDIVIDUAL_PROGRESSION_STAGE_SNAPSHOT_H

#include <cstdint>
#include <string_view>
#include <unordered_map>

#define COSMIC_IP_STAGE_SNAPSHOT_API_VERSION 1

namespace IndividualProgressionStageSnapshot
{
    constexpr std::uint8_t LastSupportedStage = 18;
    constexpr std::uint32_t FirstMarkerQuest = 66001;
    constexpr std::uint32_t LastMarkerQuest = 66018;

    constexpr bool IsStageMarkerQuest(std::uint32_t questId)
    {
        return questId >= FirstMarkerQuest && questId <= LastMarkerQuest;
    }

    // True when any decimal number in a command text is a stage marker quest
    // (".quest remove 66003", ".q rem 66003", quest links): the command must
    // invalidate snapshots BEFORE Core executes it. Over-matching is safe.
    constexpr bool MentionsStageMarker(std::string_view text)
    {
        std::uint64_t value = 0;
        bool inNumber = false;
        for (std::size_t i = 0; i <= text.size(); ++i)
        {
            bool const digit = i < text.size() && text[i] >= '0' && text[i] <= '9';
            if (digit)
            {
                std::uint64_t const d = static_cast<std::uint64_t>(text[i] - '0');
                value = inNumber ? value * 10 + d : d;
                if (value > 0xFFFFFFFFull)
                    value = 0xFFFFFFFFull;
                inNumber = true;
                continue;
            }
            if (inNumber && IsStageMarkerQuest(static_cast<std::uint32_t>(value)))
                return true;
            inNumber = false;
        }
        return false;
    }

    enum class Status : std::uint8_t { Known, Unknown };

    enum class UnknownReason : std::uint8_t
    {
        None,
        IpDisabled,        // Individual Progression is disabled
        NotPublished,      // not registered (offline/logged out) or not yet published this session
        Invalidated,       // a change is in progress or happened; awaiting owner-thread republish
        AccountUnresolved, // account class not in IP's cache when publishing
        CorruptStage       // raw marker outside 0..18 or effective > raw
    };

    struct Snapshot
    {
        Status status = Status::Unknown;
        UnknownReason reason = UnknownReason::NotPublished;
        std::uint8_t rawStage = 0;       // highest rewarded IP marker (completed content)
        std::uint8_t effectiveStage = 0; // after the paid cap; == raw when paid OFF or ungated
        bool gatedHuman = false;         // normal (non-bot, non-excluded) account
        std::uint64_t sessionEpoch = 0;  // login session that owns this value (0 = none)
        std::uint32_t generation = 0;    // registry generation the value was published under
        std::uint64_t revision = 0;      // process-unique: changes on EVERY publish and invalidation
    };

    // True only if both reads are Known and describe the SAME publication
    // (same revision): a republish of identical values, an invalidation, a
    // reload or a new session all change the revision.
    constexpr bool SameKnownSnapshot(Snapshot const& a, Snapshot const& b)
    {
        return a.status == Status::Known && b.status == Status::Known &&
            a.revision != 0 && a.revision == b.revision &&
            a.sessionEpoch == b.sessionEpoch && a.generation == b.generation &&
            a.rawStage == b.rawStage && a.effectiveStage == b.effectiveStage &&
            a.gatedHuman == b.gatedHuman;
    }

    // What the owner thread computed for the character.
    struct PublishInput
    {
        bool ipEnabled = false;
        bool accountResolved = false;
        bool gatedHuman = false;
        std::uint8_t rawStage = 0;
        std::uint8_t effectiveStage = 0;
    };

    inline Snapshot Make(PublishInput const& in, std::uint64_t epoch, std::uint32_t generation)
    {
        Snapshot s;
        s.sessionEpoch = epoch;
        s.generation = generation;
        if (!in.ipEnabled)
            s.reason = UnknownReason::IpDisabled;
        else if (!in.accountResolved)
            s.reason = UnknownReason::AccountUnresolved;
        else if (in.rawStage > LastSupportedStage || in.effectiveStage > in.rawStage)
            s.reason = UnknownReason::CorruptStage;
        else
        {
            s.status = Status::Known;
            s.reason = UnknownReason::None;
            s.rawStage = in.rawStage;
            s.effectiveStage = in.effectiveStage;
            s.gatedHuman = in.gatedHuman;
        }
        return s;
    }

    enum class PublishResult : std::uint8_t
    {
        Published,
        RejectedNotRegistered,   // logged out (or never registered)
        RejectedStaleEpoch,      // an older session's writer
        RejectedStaleGeneration, // computed before a reload/invalidate-all
        RejectedInvalidatedSince // this character was invalidated while computing
    };

    // Taken (shared lock) by the owner thread BEFORE it computes the input.
    struct Ticket
    {
        bool valid = false;
        std::uint64_t epoch = 0;
        std::uint32_t generation = 0;
        std::uint64_t invalidations = 0;
    };

    // Storage semantics. IP holds one Registry under its own shared_mutex:
    // Get/Peek under a shared lock, every mutation under a unique lock.
    class Registry
    {
    public:
        struct Entry
        {
            Snapshot snapshot;
            bool dirty = true;
            std::uint64_t invalidations = 0;
        };

        // Read: copy only; a miss is NotPublished; never inserts.
        Snapshot Get(std::uint64_t guid) const
        {
            auto const it = _entries.find(guid);
            return it == _entries.end() ? Snapshot{} : it->second.snapshot;
        }

        std::uint64_t EpochOf(std::uint64_t guid) const
        {
            auto const it = _entries.find(guid);
            return it == _entries.end() ? 0 : it->second.snapshot.sessionEpoch;
        }

        // Login: a fresh epoch replaces whatever was there.
        std::uint64_t Register(std::uint64_t guid)
        {
            std::uint64_t const epoch = ++_nextEpoch;
            Entry e;
            e.snapshot.sessionEpoch = epoch;
            e.snapshot.generation = _generation;
            e.snapshot.revision = ++_nextRevision;
            _entries[guid] = e;
            return epoch;
        }

        // Owner thread, before computing: only for its own dirty entry.
        Ticket BeginPublish(std::uint64_t guid, std::uint64_t epoch) const
        {
            Ticket t;
            auto const it = _entries.find(guid);
            if (it == _entries.end() || it->second.snapshot.sessionEpoch != epoch || !it->second.dirty)
                return t;
            t.valid = true;
            t.epoch = epoch;
            t.generation = _generation;
            t.invalidations = it->second.invalidations;
            return t;
        }

        PublishResult Publish(std::uint64_t guid, Ticket const& ticket, PublishInput const& in)
        {
            auto const it = _entries.find(guid);
            if (!ticket.valid || it == _entries.end())
                return PublishResult::RejectedNotRegistered;
            if (it->second.snapshot.sessionEpoch != ticket.epoch)
                return PublishResult::RejectedStaleEpoch;
            if (ticket.generation != _generation)
                return PublishResult::RejectedStaleGeneration;
            if (ticket.invalidations != it->second.invalidations)
                return PublishResult::RejectedInvalidatedSince;
            it->second.snapshot = Make(in, ticket.epoch, _generation);
            it->second.snapshot.revision = ++_nextRevision;
            // An unresolved account is retried by the owner on a later update
            // (the account cache is re-warmed outside this registry).
            it->second.dirty = it->second.snapshot.reason == UnknownReason::AccountUnresolved;
            return PublishResult::Published;
        }

        // A change for one character: Unknown + dirty, before it is observable.
        void Invalidate(std::uint64_t guid)
        {
            auto const it = _entries.find(guid);
            if (it == _entries.end())
                return;
            Snapshot& s = it->second.snapshot;
            s.status = Status::Unknown;
            s.reason = UnknownReason::Invalidated;
            s.rawStage = 0;
            s.effectiveStage = 0;
            s.gatedHuman = false;
            s.generation = _generation;
            s.revision = ++_nextRevision;
            ++it->second.invalidations;
            it->second.dirty = true;
        }

        // Config reload / external command: everything Unknown; any publisher
        // that started earlier is rejected by the generation check.
        void InvalidateAll()
        {
            ++_generation;
            for (auto& [guid, entry] : _entries)
            {
                entry.snapshot.status = Status::Unknown;
                entry.snapshot.reason = UnknownReason::Invalidated;
                entry.snapshot.rawStage = 0;
                entry.snapshot.effectiveStage = 0;
                entry.snapshot.gatedHuman = false;
                entry.snapshot.generation = _generation;
                entry.snapshot.revision = ++_nextRevision;
                ++entry.invalidations;
                entry.dirty = true;
            }
        }

        void Erase(std::uint64_t guid) { _entries.erase(guid); }

        std::uint32_t Generation() const { return _generation; }
        std::size_t Size() const { return _entries.size(); }

    private:
        std::unordered_map<std::uint64_t, Entry> _entries;
        std::uint64_t _nextEpoch = 0;
        std::uint64_t _nextRevision = 0;
        std::uint32_t _generation = 1;
    };
}

#endif
