/*
 * F14 / Playerbots ResetQuests OPTIONAL PROTOTYPE, pure C++ policy model.
 * This DOES NOT patch an installed Playerbots, reward any quest, or launch WoW.
 * The separate unified patch is tested only in a disposable CI checkout.
 */
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <set>

namespace
{
    constexpr std::uint32_t FirstIPMarker = 66001;
    constexpr std::uint32_t LastIPMarker = 66018;

    enum class QuestState { None, Active, Rewarded };

    // Mirrors the proposed branch at PlayerbotFactory::ResetQuests:
    // status NONE is already filtered by stock PB, and only an existing
    // rewarded IP marker explicitly on the keep list can escape the reset.
    std::uint32_t BuildMask(std::set<std::uint32_t> const& keep)
    {
        std::uint32_t mask = 0;
        for (std::uint32_t id : keep)
            if (id >= FirstIPMarker && id <= LastIPMarker)
                mask |= std::uint32_t(1) << (id - FirstIPMarker);
        return mask;
    }

    bool PreserveExistingRewardedMarker(std::uint32_t id, QuestState state,
                                        std::uint32_t mask)
    {
        return id >= FirstIPMarker && id <= LastIPMarker &&
               state == QuestState::Rewarded &&
               (mask & (std::uint32_t(1) << (id - FirstIPMarker))) != 0;
    }

    unsigned checks = 0;
    unsigned failures = 0;
    void Check(char const* test, bool value, bool expected)
    {
        ++checks;
        if (value != expected)
        {
            ++failures;
            std::cerr << "FAILED " << test << " got=" << value
                      << " expected=" << expected << '\n';
        }
    }
}

int main()
{
    std::set<std::uint32_t> const empty;
    std::set<std::uint32_t> const allMarkers = [] {
        std::set<std::uint32_t> list;
        for (std::uint32_t id = FirstIPMarker; id <= LastIPMarker; ++id)
            list.insert(id);
        return list;
    }();
    std::set<std::uint32_t> const partial = {66001, 66008, 66018, 1, 65999, 66019};
    std::uint32_t const emptyMask = BuildMask(empty);
    std::uint32_t const fullMask = BuildMask(allMarkers);
    std::uint32_t const partialMask = BuildMask(partial);
    Check("empty config mask is zero", emptyMask == 0, true);
    Check("full 18-ID mask exactly set", fullMask == ((std::uint32_t(1) << 18) - 1), true);

    for (std::uint32_t id = 65990; id <= 66025; ++id)
    {
        for (QuestState state : {QuestState::None, QuestState::Active, QuestState::Rewarded})
        {
            Check("default EMPTY keeps stock reset", PreserveExistingRewardedMarker(id, state, emptyMask), false);
            Check("explicit all-markers preserve only earned IDs",
                  PreserveExistingRewardedMarker(id, state, fullMask),
                  id >= FirstIPMarker && id <= LastIPMarker && state == QuestState::Rewarded);
            Check("invalid/non-marker IDs in list still do not bypass reset",
                  PreserveExistingRewardedMarker(id, state, partialMask),
                  (id == 66001 || id == 66008 || id == 66018) &&
                  state == QuestState::Rewarded);
        }
    }

    // An unearned stage must not be created; an active quest must not be
    // converted into a rewarded quest. Existing status is never rewritten.
    Check("new bot no stage 8", PreserveExistingRewardedMarker(66008, QuestState::None, fullMask), false);
    Check("active quest not marked rewarded", PreserveExistingRewardedMarker(66008, QuestState::Active, fullMask), false);
    Check("already rewarded stage 8 is retained", PreserveExistingRewardedMarker(66008, QuestState::Rewarded, fullMask), true);
    Check("already rewarded stage 18 is retained", PreserveExistingRewardedMarker(66018, QuestState::Rewarded, fullMask), true);
    Check("ordinary rewarded quest still reset", PreserveExistingRewardedMarker(10259, QuestState::Rewarded, fullMask), false);

    // Reload updates an atomic snapshot, never a mutable shared std::set.
    std::atomic<std::uint32_t> currentMask{0};
    currentMask.store(fullMask, std::memory_order_release);
    Check("reload to configured list preserves earned stage8",
          PreserveExistingRewardedMarker(66008, QuestState::Rewarded,
                                         currentMask.load(std::memory_order_acquire)), true);
    currentMask.store(emptyMask, std::memory_order_release);
    Check("reload to empty list immediately restores stock reset",
          PreserveExistingRewardedMarker(66008, QuestState::Rewarded,
                                         currentMask.load(std::memory_order_acquire)), false);

    std::cout << "F14 Playerbots keep-list candidate model: " << (checks - failures)
              << "/" << checks << " checks passed; PROTOTYPE ONLY, no gameplay.\n";
    return failures ? 1 : 0;
}
