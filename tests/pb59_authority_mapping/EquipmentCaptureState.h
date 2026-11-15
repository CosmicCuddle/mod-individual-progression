#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>

namespace PB59
{
// Owner-context only: never read this object from a foreign worker/thread.
// Session is Core's own incarnation, not the IP registry epoch.
struct Stamp
{
    std::uint64_t session = 0;
    std::uint64_t revision = 0;
    bool operator==(Stamp const&) const = default;
};

class EquipmentCaptureState
{
public:
    void Bind()
    {
        auto value = _lastSession.load(std::memory_order_relaxed);
        while (value != std::numeric_limits<std::uint64_t>::max())
        {
            if (_lastSession.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
            {
                _session = value + 1;
                Invalidate();
                return;
            }
        }
        Detach();
    }

    void Detach()
    {
        _session = 0;
        Invalidate();
    }

    class Mutation
    {
    public:
        explicit Mutation(EquipmentCaptureState* state) : _state(state)
        {
            if (_state && _state->_depth++ == 0)
                _state->Invalidate();
        }
        explicit Mutation(EquipmentCaptureState& state) : Mutation(&state) { }
        ~Mutation() { if (_state) --_state->_depth; }
        Mutation(Mutation const&) = delete;
        Mutation& operator=(Mutation const&) = delete;
    private:
        EquipmentCaptureState* _state;
    };

    std::optional<Stamp> Capture() const
    {
        if (!_session || _depth || _exhausted)
            return std::nullopt;
        return Stamp{_session, _revision};
    }

    bool Current(Stamp const& stamp) const
    {
        auto now = Capture();
        return now && *now == stamp;
    }

private:
    void Invalidate()
    {
        if (_revision == std::numeric_limits<std::uint64_t>::max())
            _exhausted = true;
        else
            ++_revision;
    }

    inline static std::atomic<std::uint64_t> _lastSession{0};
    std::uint64_t _session = 0;
    std::uint64_t _revision = 0;
    std::uint64_t _depth = 0;
    bool _exhausted = false;
};

// Raw facts only. No item-level weighting or eligibility policy here.
struct SlotFacts
{
    std::uint64_t itemGuid = 0;
    std::uint32_t entry = 0;
    std::uint32_t itemLevel = 0;
    std::uint32_t quality = 0;
    std::uint32_t inventoryType = 0;
    std::uint32_t itemClass = 0;
    std::uint32_t itemSubClass = 0;
    std::uint32_t scalingDistribution = 0;
    std::uint32_t scalingValue = 0;
    bool operator==(SlotFacts const&) const = default;
};

struct EquipmentFacts
{
    Stamp stamp;
    std::uint64_t guid = 0;
    std::uint32_t level = 0;
    std::uint32_t playerClass = 0;
    std::uint32_t race = 0;
    std::uint32_t spec = 0;
    bool dualWield = false;
    bool titanGrip = false;
    std::array<SlotFacts, 19> slots{};
    bool operator==(EquipmentFacts const&) const = default;
};
}
