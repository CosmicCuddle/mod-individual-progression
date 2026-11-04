/* Isolated pure model of the two-layer PB earned marker reset proposal.
 * This does NOT run against any server, client, database or bot session.
 */
#include <atomic>
#include <cstdint>
#include <iostream>

namespace
{
    constexpr std::uint32_t First = 66001, Last = 66018;
    enum class QuestStatus { None, Active, Rewarded };

    bool Preserve(std::uint32_t id, QuestStatus status,
                  std::uint32_t keepMask, bool positivelySelectedRandomBot)
    {
        return positivelySelectedRandomBot && keepMask != 0 &&
               id >= First && id <= Last && status == QuestStatus::Rewarded &&
               (keepMask & (std::uint32_t{1} << (id - First))) != 0;
    }
}

int main()
{
    unsigned total = 0, failed = 0;
    auto check = [&](bool got, bool expected)
    {
        ++total;
        if (got != expected)
        {
            ++failed;
            std::cerr << "FAILED check " << total << '\n';
        }
    };
    constexpr std::uint32_t full = (std::uint32_t{1} << 18) - 1;
    constexpr std::uint32_t partial = (std::uint32_t{1} << (66008 - First));
    for (std::uint32_t id = 65990; id <= 66026; ++id)
        for (QuestStatus state : {QuestStatus::None, QuestStatus::Active, QuestStatus::Rewarded})
            for (std::uint32_t mask : {std::uint32_t{0}, full, partial})
                for (bool positiveSelectedRandomBot : {false, true})
                {
                    bool const expected = positiveSelectedRandomBot &&
                        state == QuestStatus::Rewarded && id >= First && id <= Last &&
                        (mask & (std::uint32_t{1} << (id >= First && id <= Last ? id - First : 0))) != 0;
                    check(Preserve(id, state, mask, positiveSelectedRandomBot), expected);
                }
    // Identity comes from the PB selected-bot manager, NOT race or bot
    // account alone. Altbot, normal player and excluded human all fail.
    for (int unselectedCategory = 0; unselectedCategory < 3; ++unselectedCategory)
    {
        (void)unselectedCategory;
        check(Preserve(66008, QuestStatus::Rewarded, full, false), false);
    }
    std::atomic<std::uint32_t> mask{full};
    std::uint32_t const sameResetMask = mask.load(std::memory_order_acquire);
    mask.store(0, std::memory_order_release);
    check(Preserve(66008, QuestStatus::Rewarded, sameResetMask, true), true);
    check(Preserve(66008, QuestStatus::Rewarded, mask.load(std::memory_order_acquire), true), false);
    check(Preserve(66008, QuestStatus::None, full, true), false);
    check(Preserve(66008, QuestStatus::Active, full, true), false);
    check(Preserve(66008, QuestStatus::Rewarded, full, true), true);
    check(Preserve(66018, QuestStatus::Rewarded, full, true), true);
    check(Preserve(10259, QuestStatus::Rewarded, full, true), false);
    std::cout << "F14 selected RandomBot-only marker keep-list pure model: " <<
                 total - failed << "/" << total << " checks passed; NO GAMEPLAY.\n";
    return failed ? 1 : 0;
}
