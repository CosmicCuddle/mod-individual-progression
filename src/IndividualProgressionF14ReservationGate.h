#ifndef INDIVIDUAL_PROGRESSION_F14_RESERVATION_GATE_H
#define INDIVIDUAL_PROGRESSION_F14_RESERVATION_GATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>

namespace IndividualProgressionF14ReservationGate
{
    // Pure accounting model, NOT a runtime listener or a permission to hide.
    // Fixed slots account for active AND retained shards. Actual protocol
    // payload ownership/allocation and authenticated map context are external
    // obligations; a Slot here is not a complete retained Protocol.
    template<std::size_t Capacity>
    class Gate
    {
        static_assert(Capacity > 0, "positive shard capacity required");
    public:
        struct Handle
        {
            std::size_t slot = Capacity;
            std::uint64_t generation = 0;
            explicit operator bool() const { return slot < Capacity && generation != 0; }
        };
        struct Reservation
        {
            Handle shard{};
            std::uint64_t token = 0;
            explicit operator bool() const { return bool(shard) && token != 0; }
        };
        enum class Settlement { Unused, Recorded, Uncertain };
        enum class SealResult { Rejected, CleanReleased, Retained };
        struct Snapshot
        {
            bool fenced = false;
            std::size_t active = 0, retained = 0, inFlight = 0, occupied = 0;
        };

        Gate() = default;
        Gate(Gate const&) = delete;
        Gate& operator=(Gate const&) = delete;

        // Caller must already own/preallocate complete payload storage.
        // No allocation, Core callback or packet operation occurs under lock.
        Handle Open(bool payloadPreallocated)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_fenced || !payloadPreallocated)
                return {};
            for (std::size_t i = 0; i < Capacity; ++i)
            {
                Slot& s = _slots[i];
                if (s.state != State::Free)
                    continue;
                if (_generation == std::numeric_limits<std::uint64_t>::max())
                {
                    _fenced = true;
                    return {};
                }
                s = Slot{};
                s.state = State::Active;
                s.generation = ++_generation;
                return {i, s.generation};
            }
            return {}; // no eviction, no admission beyond budget
        }

        Reservation Reserve(Handle h)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(h);
            if (_fenced || !s || s->pending != 0)
                return {};
            if (s->lastToken == std::numeric_limits<std::uint64_t>::max())
            {
                _fenced = true;
                return {};
            }
            s->pending = ++s->lastToken;
            return {h, s->pending};
        }

        // Must be called before leaving the same synchronous visibility call.
        // Recorded means a server-side debt record exists, NOT client proof.
        bool Settle(Reservation r, Settlement outcome)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(r.shard);
            if (!s || r.token == 0 || s->pending != r.token)
                return false;
            s->pending = 0;
            if (outcome == Settlement::Recorded)
                s->debt = true;
            else if (outcome == Settlement::Uncertain)
            {
                s->debt = true;
                _fenced = true;
            }
            return true;
        }

        void CloseFence()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _fenced = true;
        }

        // Caller proves exclusive payload ownership and callback quiescence.
        // Uncertain/in-flight/debt state never becomes reusable. No R2 clear.
        SealResult Seal(Handle h, bool quiescent, bool payloadClean)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Slot* s = Active(h);
            if (!s)
                return SealResult::Rejected;
            if (s->debt || s->pending != 0 || !quiescent || !payloadClean)
            {
                _fenced = true;
                s->state = State::Retained;
                return SealResult::Retained;
            }
            *s = Slot{};
            return SealResult::CleanReleased;
        }

        Snapshot Inspect() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            Snapshot out{};
            out.fenced = _fenced;
            for (Slot const& s : _slots)
            {
                if (s.state == State::Active) ++out.active;
                if (s.state == State::Retained) ++out.retained;
                if (s.pending != 0) ++out.inFlight;
            }
            out.occupied = out.active + out.retained;
            return out;
        }
    private:
        enum class State { Free, Active, Retained };
        struct Slot
        {
            State state = State::Free;
            std::uint64_t generation = 0, lastToken = 0, pending = 0;
            bool debt = false;
        };
        Slot* Active(Handle h)
        {
            if (!h) return nullptr;
            Slot& s = _slots[h.slot];
            return s.state == State::Active && s.generation == h.generation ? &s : nullptr;
        }
        mutable std::mutex _mutex;
        std::array<Slot, Capacity> _slots{};
        std::uint64_t _generation = 0;
        bool _fenced = false;
    };
}
#endif
