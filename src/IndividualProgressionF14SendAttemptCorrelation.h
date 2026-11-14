/*
 * CosmicCuddle F14: viewer login-epoch + creature-incarnation CORRELATION for
 * GUID-only tagged send attempts. PURE, NOT WIRED, value-only, default OFF.
 *
 * The #93 OUT and #95 CREATE Core callbacks report only (viewer, creature
 * GUID) after SendDirectMessage. After an OUT unlink they cannot recover the
 * viewer's login epoch or the creature's incarnation. Both ARE known earlier
 * in the same synchronous chain: at the #86 suppression decision (OUT) and
 * at the #76 re-link of an owed restore (CREATE). This table records a
 * candidate there ("Nominate") and hands the recorded identity back to a
 * later tagged send attempt ("Correlate") ONLY when exactly one candidate
 * matches the same viewer session, creature GUID, direction and pass token.
 *
 * The pass token is a caller-supplied, opaque, nonzero, strictly increasing
 * per-viewer-session value. NO Core producer exists at the pinned Core: it
 * is an UNMET dependency, and a synthetic accepted token proves no live
 * provenance. A token is never inferred from GUIDs, maps, clocks, callback
 * order or the packet itself.
 *
 * Every refusal means only "no evidence upgrade". This header never reads a
 * Core object, never calls the #103 adapter or any mutating #94 API, never
 * clears or adds a restore obligation, and never treats a send CALL as
 * socket transmission, client presentation or restoration.
 *
 * Not thread-safe: one owner (or one mutex) per table; never hold it across
 * a Core visibility or ScriptMgr callback.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_SEND_ATTEMPT_CORRELATION_H
#define INDIVIDUAL_PROGRESSION_F14_SEND_ATTEMPT_CORRELATION_H

#include "IndividualProgressionF14ViewerRestoreProtocol.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14SendAttemptCorrelation
{
    namespace Restore = IndividualProgressionF14ViewerRestoreProtocol;
    using ViewerKey = Restore::ViewerKey;
    using CreatureKey = Restore::CreatureKey;

    constexpr std::size_t DefaultMaxCandidates = 1024;
    constexpr std::size_t DefaultMaxViewers = 256;

    enum class Direction : std::uint8_t
    {
        OutOfRange, // nominated at the #86 F14 decision (wasLinked)
        Create      // nominated at the #76 re-link of an owed restore
    };

    enum class Result : std::uint8_t
    {
        Nominated,
        DuplicateIgnored,
        Matched,
        PassAccepted,
        PassClosed, // observed active pass has ended; late send proves nothing
        // Refusals (all fail closed: no evidence upgrade).
        InvalidKey,
        ZeroToken,
        Unattributed,
        StaleEpoch,
        UnobservedSessionChange,
        RecycledIncarnation,
        CrossMap,
        GlobalFreeze,
        NoPriorF14Suppression,
        RestoreOwedForOut,
        RestoreNotOwedForCreate,
        TokenReorderedOrReused,
        ViewerPoisoned,
        TablePoisoned,
        Capacity,
        Ambiguous,
        NoCandidate,
        StaleToken,
        WrongDirection,
        Count
    };

    // Facts a future caller establishes at nomination time. Every default
    // is the failing value (frozen, no prior suppression, zero epoch/ids).
    struct NominationFacts
    {
        std::uint64_t currentViewerEpoch = 0;
        std::uint64_t currentCreatureIncarnation = 0;
        std::uint32_t eventMapId = 0;
        std::uint32_t eventInstanceId = 0;
        bool globalRestoreRequired = true;
        bool priorF14SuppressionRecorded = false;
        bool restoreOwed = false;
    };

    // Read-only helper: the protocol-derived part of NominationFacts. It
    // takes the protocol by const reference and cannot change it.
    template<std::size_t ProtocolCapacity>
    void ReadProtocolFacts(Restore::Protocol<ProtocolCapacity> const& protocol,
                           ViewerKey viewer, CreatureKey creature,
                           std::uint64_t incarnation, NominationFacts& facts)
    {
        Restore::Evidence const e = protocol.Find(viewer, creature);
        facts.globalRestoreRequired = protocol.GlobalRestoreRequired();
        facts.priorF14SuppressionRecorded = e.found && e.suppressIntent && e.incarnation == incarnation;
        facts.restoreOwed = protocol.NeedsRestore(viewer, creature);
    }

    // Facts of one tagged GUID-only send attempt. Defaults fail closed.
    struct SendFacts
    {
        bool sourceAttributedByExactCoreF14Hook = false;
        std::uint64_t viewerGuidRaw = 0;
        std::uint64_t currentViewerEpoch = 0;
        std::uint64_t creatureGuidRaw = 0;
        std::uint32_t eventMapId = 0;
        std::uint32_t eventInstanceId = 0;
        Direction direction = Direction::OutOfRange;
        std::uint64_t passToken = 0;
        bool globalRestoreRequired = true;
    };

    // The recorded identity, returned only for a unique exact match. It is
    // an INPUT a future caller could give the #103 adapter; it is not proof
    // of transmission, presentation or restoration.
    struct Correlation
    {
        Result result = Result::NoCandidate;
        ViewerKey viewer{};
        CreatureKey creature{};
        std::uint64_t incarnation = 0;
        Direction direction = Direction::OutOfRange;
        std::uint64_t passToken = 0;
        bool Matched() const { return result == Result::Matched; }
    };

    constexpr bool IsSendCallClientPresentation() { return false; }
    constexpr bool HasCorePassTokenProducer() { return false; }

    template<std::size_t MaxCandidates = DefaultMaxCandidates,
             std::size_t MaxViewers = DefaultMaxViewers>
    class Table
    {
        static_assert(MaxCandidates > 0 && MaxViewers > 0, "F14 correlation bounds must be positive");

    public:
        Result Nominate(ViewerKey viewer, CreatureKey creature, std::uint64_t incarnation,
                        Direction direction, std::uint64_t passToken, NominationFacts const& facts)
        {
            if (_poisoned)
                return Refuse(Result::TablePoisoned);
            if (!viewer.IsValid() || !creature.IsValid() || incarnation == 0 || !ValidDirection(direction))
                return Refuse(Result::InvalidKey);
            if (passToken == 0)
                return Refuse(Result::ZeroToken);
            if (viewer.loginEpoch != facts.currentViewerEpoch)
                return Refuse(Result::StaleEpoch);
            if (incarnation != facts.currentCreatureIncarnation)
                return Refuse(Result::RecycledIncarnation);
            if (facts.eventMapId != creature.mapId || facts.eventInstanceId != creature.instanceId)
                return Refuse(Result::CrossMap);
            if (facts.globalRestoreRequired)
                return Refuse(Result::GlobalFreeze);
            if (!facts.priorF14SuppressionRecorded)
                return Refuse(Result::NoPriorF14Suppression);
            if (direction == Direction::OutOfRange && facts.restoreOwed)
                return Refuse(Result::RestoreOwedForOut);
            if (direction == Direction::Create && !facts.restoreOwed)
                return Refuse(Result::RestoreNotOwedForCreate);

            Result const pass = AdvancePass(viewer, passToken);
            if (pass != Result::PassAccepted)
                return Refuse(pass);

            // Same viewer session, GUID, direction and pass: identical is a
            // repeated visit (idempotent); anything else competes and fails
            // closed for every competitor.
            bool competing = false;
            for (std::size_t i = 0; i < _size; ++i)
            {
                Candidate& c = _candidates[i];
                if (!SameSlot(c, viewer, creature.guidRaw, direction, passToken))
                    continue;
                if (c.creature == creature && c.incarnation == incarnation && !c.ambiguous)
                    return Accept(Result::DuplicateIgnored);
                c.ambiguous = true;
                competing = true;
            }
            if (competing)
                return Refuse(Result::Ambiguous);

            if (_size == MaxCandidates)
            {
                PoisonTable();
                return Refuse(Result::Capacity);
            }
            _candidates[_size++] = Candidate{viewer, creature, incarnation, direction, passToken, false};
            return Accept(Result::Nominated);
        }

        Correlation Correlate(SendFacts const& facts)
        {
            Correlation out;
            out.direction = facts.direction;
            out.passToken = facts.passToken;
            out.result = CorrelateInner(facts, out);
            ++_counts[static_cast<std::size_t>(out.result)];
            return out;
        }

        // Explicit successor pass for the same viewer session. Older pass
        // candidates expire; a non-increasing token poisons that viewer.
        Result ObserveSuccessorPass(ViewerKey viewer, std::uint64_t passToken)
        {
            if (_poisoned)
                return Refuse(Result::TablePoisoned);
            if (!viewer.IsValid())
                return Refuse(Result::InvalidKey);
            if (passToken == 0)
                return Refuse(Result::ZeroToken);
            Result const r = AdvancePass(viewer, passToken);
            return r == Result::PassAccepted ? Accept(r) : Refuse(r);
        }

        // Externally supplied pass-end events. A token closed by End/Cancel
        // can never nominate or correlate again. A strictly newer end closes
        // through that token and does not poison the viewer: an empty or
        // never-nominated pass is ordinary, not proof the session is corrupt.
        // Unknown-viewer ends are remembered; a stale end must not close a
        // newer active pass. A fully closed, candidate-free, unpoisoned
        // tombstone may be replaced by a strictly newer login epoch. Live
        // evidence may not. Dropping candidates NEVER changes the #94 ledger.
        Result ObservePassEnded(ViewerKey viewer, std::uint64_t passToken)
        {
            if (_poisoned)
                return Refuse(Result::TablePoisoned);
            if (!viewer.IsValid())
                return Refuse(Result::InvalidKey);
            if (passToken == 0)
                return Refuse(Result::ZeroToken);

            std::size_t const vp = FindViewerByGuid(viewer.guidRaw);
            if (vp == MaxViewers)
            {
                if (_viewerCount == MaxViewers)
                {
                    PoisonTable();
                    return Refuse(Result::Capacity);
                }
                // A late nomination for this previously unknown token must
                // fail, rather than opening a pass after its own end event.
                _viewers[_viewerCount++] = ViewerPass{viewer, passToken, passToken, false};
                return Accept(Result::PassAccepted);
            }

            ViewerPass& pass = _viewers[vp];
            if (!(pass.viewer == viewer))
            {
                if (!MayReplaceWithNewerEpoch(pass, viewer))
                    return Refuse(Result::UnobservedSessionChange);
                // The previous epoch left only a closed tombstone. Remember
                // this epoch's end in the same slot; do not keep the old one.
                _viewers[vp] = ViewerPass{viewer, passToken, passToken, false};
                return Accept(Result::PassAccepted);
            }
            if (pass.poisoned)
                return Refuse(Result::ViewerPoisoned);
            if (passToken < pass.lastToken)
                return Refuse(Result::StaleToken);
            if (passToken > pass.lastToken)
            {
                // Close through the newer token. Superseded candidates for
                // this viewer cannot match afterwards. Other viewers stay.
                pass.lastToken = passToken;
                pass.closedThrough = passToken;
                DropIf([&](Candidate const& c) { return c.viewer == viewer; });
                return Accept(Result::PassAccepted);
            }
            if (passToken <= pass.closedThrough)
                return Refuse(Result::PassClosed); // duplicate end, no state change

            pass.closedThrough = passToken;
            DropIf([&](Candidate const& c) { return c.viewer == viewer && c.passToken == passToken; });
            return Accept(Result::PassAccepted);
        }
        Result CancelPass(ViewerKey viewer, std::uint64_t passToken)
        {
            return ObservePassEnded(viewer, passToken);
        }
        void ObserveViewerSessionEnded(std::uint64_t viewerGuid) { ForgetViewer(viewerGuid); }
        // Rights/era changes do not end the authenticated login epoch.
        // Close through the last observed pass, retaining its replay fence
        // and any poison. Never forget a same-epoch viewer to free capacity.
        // Unknown viewers have no local pass to close; the future Core
        // consumer must independently fence its actual pass/generation.
        void ObserveViewerRightsOrEraChanged(std::uint64_t viewerGuid)
        {
            if (!viewerGuid)
                return;
            std::size_t const vp = FindViewerByGuid(viewerGuid);
            if (vp == MaxViewers)
                return;
            ViewerPass& pass = _viewers[vp];
            pass.closedThrough = pass.lastToken;
            DropIf([&](Candidate const& c) { return c.viewer == pass.viewer; });
        }
        void ObserveCreatureRetired(CreatureKey creature)
        {
            DropIf([&](Candidate const& c) { return c.creature == creature; });
        }
        void ObserveMapUnloaded(std::uint32_t mapId, std::uint32_t instanceId)
        {
            DropIf([&](Candidate const& c) {
                return c.creature.mapId == mapId && c.creature.instanceId == instanceId; });
        }

        std::size_t Candidates() const { return _size; }
        std::size_t Viewers() const { return _viewerCount; }
        bool IsPoisoned() const { return _poisoned; }
        std::uint64_t Count(Result r) const
        {
            return r < Result::Count ? _counts[static_cast<std::size_t>(r)] : 0;
        }

    private:
        struct Candidate
        {
            ViewerKey viewer{};
            CreatureKey creature{};
            std::uint64_t incarnation = 0;
            Direction direction = Direction::OutOfRange;
            std::uint64_t passToken = 0;
            bool ambiguous = false;
        };

        struct ViewerPass
        {
            ViewerKey viewer{};
            std::uint64_t lastToken = 0;
            std::uint64_t closedThrough = 0;
            bool poisoned = false;
        };

        static bool ValidDirection(Direction d)
        {
            return d == Direction::OutOfRange || d == Direction::Create;
        }

        static bool SameSlot(Candidate const& c, ViewerKey viewer, std::uint64_t guidRaw,
                             Direction direction, std::uint64_t passToken)
        {
            return c.viewer == viewer && c.creature.guidRaw == guidRaw &&
                c.direction == direction && c.passToken == passToken;
        }

        // Replacement is allowed only for a strictly newer login epoch and
        // only when the stored record is a fully closed tombstone: the pass
        // was ended, nothing is poisoned, and no candidate remains for that
        // viewer GUID. An open pass, a poison flag or any candidate is live
        // evidence and stays until ObserveViewerSessionEnded.
        bool MayReplaceWithNewerEpoch(ViewerPass const& pass, ViewerKey incoming) const
        {
            if (!(incoming.loginEpoch > pass.viewer.loginEpoch) || pass.poisoned ||
                pass.lastToken == 0 || pass.closedThrough < pass.lastToken)
                return false;
            for (std::size_t i = 0; i < _size; ++i)
                if (_candidates[i].viewer.guidRaw == pass.viewer.guidRaw)
                    return false;
            return true;
        }

        Result CorrelateInner(SendFacts const& f, Correlation& out)
        {
            if (!f.sourceAttributedByExactCoreF14Hook)
                return Result::Unattributed;
            if (_poisoned)
                return Result::TablePoisoned;
            if (f.viewerGuidRaw == 0 || f.currentViewerEpoch == 0 || f.creatureGuidRaw == 0 ||
                !ValidDirection(f.direction))
                return Result::InvalidKey;
            if (f.passToken == 0)
                return Result::ZeroToken;
            if (f.globalRestoreRequired)
                return Result::GlobalFreeze;

            std::size_t const vp = FindViewerByGuid(f.viewerGuidRaw);
            if (vp == MaxViewers)
                return Result::NoCandidate;
            ViewerPass const& pass = _viewers[vp];
            if (pass.viewer.loginEpoch != f.currentViewerEpoch)
                return Result::StaleEpoch;
            if (pass.poisoned)
                return Result::ViewerPoisoned;
            if (f.passToken < pass.lastToken)
                return Result::StaleToken;
            if (f.passToken == pass.lastToken && f.passToken <= pass.closedThrough)
                return Result::PassClosed;
            if (f.passToken > pass.lastToken)
                return Result::NoCandidate; // never nominated; does NOT advance the pass

            std::size_t matches = 0, index = MaxCandidates;
            bool otherDirection = false, ambiguous = false;
            for (std::size_t i = 0; i < _size; ++i)
            {
                Candidate const& c = _candidates[i];
                if (c.viewer == pass.viewer && c.creature.guidRaw == f.creatureGuidRaw &&
                    c.passToken == f.passToken)
                {
                    if (c.direction != f.direction)
                    {
                        otherDirection = true;
                        continue;
                    }
                    ++matches;
                    index = i;
                    ambiguous = ambiguous || c.ambiguous;
                }
            }
            if (matches == 0)
                return otherDirection ? Result::WrongDirection : Result::NoCandidate;
            if (matches > 1 || ambiguous)
            {
                // Fail closed and drop every competitor for this exact slot.
                ViewerKey const v = pass.viewer;
                DropIf([&](Candidate const& c) {
                    return SameSlot(c, v, f.creatureGuidRaw, f.direction, f.passToken); });
                return Result::Ambiguous;
            }

            Candidate const c = _candidates[index];
            RemoveAt(index); // consumed either way: one send attempt, one use
            if (c.creature.mapId != f.eventMapId || c.creature.instanceId != f.eventInstanceId)
                return Result::CrossMap;

            out.viewer = c.viewer;
            out.creature = c.creature;
            out.incarnation = c.incarnation;
            return Result::Matched;
        }

        // PassAccepted when the token is the current or a newer pass.
        Result AdvancePass(ViewerKey viewer, std::uint64_t passToken)
        {
            std::size_t const vp = FindViewerByGuid(viewer.guidRaw);
            if (vp == MaxViewers)
            {
                if (_viewerCount == MaxViewers)
                {
                    PoisonTable();
                    return Result::Capacity;
                }
                _viewers[_viewerCount++] = ViewerPass{viewer, passToken, 0, false};
                return Result::PassAccepted;
            }
            ViewerPass& pass = _viewers[vp];
            if (!(pass.viewer == viewer))
            {
                if (!MayReplaceWithNewerEpoch(pass, viewer))
                    return Result::UnobservedSessionChange; // live evidence stays until session end
                pass = ViewerPass{viewer, passToken, 0, false};
                return Result::PassAccepted;
            }
            if (pass.poisoned)
                return Result::ViewerPoisoned;
            if (passToken == pass.lastToken)
            {
                if (passToken <= pass.closedThrough)
                {
                    // Reopening an ended or cancelled token is a replay.
                    pass.poisoned = true;
                    DropIf([&](Candidate const& c) { return c.viewer == viewer; });
                    return Result::TokenReorderedOrReused;
                }
                return Result::PassAccepted;
            }
            if (passToken < pass.lastToken)
            {
                // Reused, reordered or wrapped token: fail closed for this
                // viewer session only, and drop its candidates.
                pass.poisoned = true;
                DropIf([&](Candidate const& c) { return c.viewer == viewer; });
                return Result::TokenReorderedOrReused;
            }
            pass.lastToken = passToken; // explicitly observed successor pass
            DropIf([&](Candidate const& c) { return c.viewer == viewer && c.passToken < passToken; });
            return Result::PassAccepted;
        }

        void ForgetViewer(std::uint64_t viewerGuid)
        {
            if (!viewerGuid)
                return;
            DropIf([&](Candidate const& c) { return c.viewer.guidRaw == viewerGuid; });
            for (std::size_t i = 0; i < _viewerCount;)
            {
                if (_viewers[i].viewer.guidRaw == viewerGuid)
                {
                    _viewers[i] = _viewers[_viewerCount - 1];
                    _viewers[--_viewerCount] = ViewerPass{};
                }
                else
                    ++i;
            }
        }

        std::size_t FindViewerByGuid(std::uint64_t guidRaw) const
        {
            for (std::size_t i = 0; i < _viewerCount; ++i)
                if (_viewers[i].viewer.guidRaw == guidRaw)
                    return i;
            return MaxViewers;
        }

        template<typename Pred>
        void DropIf(Pred pred)
        {
            for (std::size_t i = 0; i < _size;)
            {
                if (pred(_candidates[i]))
                    RemoveAt(i);
                else
                    ++i;
            }
        }

        void RemoveAt(std::size_t i)
        {
            _candidates[i] = _candidates[_size - 1];
            _candidates[--_size] = Candidate{};
        }

        Result Accept(Result r)
        {
            ++_counts[static_cast<std::size_t>(r)];
            return r;
        }

        Result Refuse(Result r)
        {
            ++_counts[static_cast<std::size_t>(r)];
            return r;
        }

        // Overflow of candidates or of viewer slots (default 256): never
        // evict to make room or guess a partial match. Every candidate is
        // dropped and every later nomination, correlation, successor, end
        // and cancel is refused. Viewer records are not reused. Obligations
        // live only in #94's ledger; this poison does not clear them.
        void PoisonTable()
        {
            _poisoned = true;
            _size = 0;
            _candidates = {};
        }

        std::array<Candidate, MaxCandidates> _candidates{};
        std::array<ViewerPass, MaxViewers> _viewers{};
        std::array<std::uint64_t, static_cast<std::size_t>(Result::Count)> _counts{};
        std::size_t _size = 0;
        std::size_t _viewerCount = 0;
        bool _poisoned = false;
    };
}
#endif
