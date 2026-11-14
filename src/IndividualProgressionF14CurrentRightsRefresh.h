#ifndef INDIVIDUAL_PROGRESSION_F14_CURRENT_RIGHTS_REFRESH_H
#define INDIVIDUAL_PROGRESSION_F14_CURRENT_RIGHTS_REFRESH_H

#include <atomic>
#include <cstdint>

namespace IndividualProgressionF14CurrentRightsRefresh
{
    enum class Reason : std::uint32_t
    {
        None = 0,
        AttackOrTargetChanged = 1u << 0,
        DirectDamage = 1u << 1,
        PeriodicOrSpellDamage = 1u << 2,
        HealOrAssist = 1u << 3,
        AuraChanged = 1u << 4,
        CombatStateChanged = 1u << 5,
        CreatureRetired = 1u << 6
    };

    constexpr std::uint32_t Bit(Reason reason)
    {
        return static_cast<std::uint32_t>(reason);
    }

    class Pending
    {
    public:
        void Request(Reason reason)
        {
            std::uint32_t const bit = Bit(reason);
            if (bit)
                _bits.fetch_or(bit, std::memory_order_release);
        }

        [[nodiscard]] bool HasPending() const
        {
            return _bits.load(std::memory_order_acquire) != 0;
        }

        // One-shot map-thread consumer. A concurrent request that linearizes
        // after this exchange remains pending for the next Unit::Update.
        [[nodiscard]] std::uint32_t Take()
        {
            return _bits.exchange(0, std::memory_order_acq_rel);
        }

        void Clear()
        {
            _bits.store(0, std::memory_order_release);
        }

    private:
        std::atomic<std::uint32_t> _bits{0};
    };
}

#endif