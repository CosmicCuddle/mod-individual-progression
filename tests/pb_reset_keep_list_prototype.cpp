/*
 * F14 / Playerbots ResetQuests OPTIONAL PROTOTYPE, pure C++ policy model.
 * This DOES NOT patch an installed Playerbots, reward any quest, or launch WoW.
 * The separate unified patch is tested only in a disposable CI checkout.
 */
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
    bool PreserveExistingRewardedMarker(std::uint32_t id, QuestState state,
                                        std::set<std::uint32_t> const& keep)
    {
        return id >= FirstIPMarker && id <= LastIPMarker &&
               state == QuestState::Rewarded && keep.count(id) != 0;
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

    for (std::uint32_t id = 65990; id <= 66025; ++id)
    {
        for (QuestState state : {QuestState::None, QuestState::Active, QuestState::Rewarded})
        {
            Check("default EMPTY keeps stock reset", PreserveExistingRewardedMarker(id, state, empty), false);
            Check("explicit all-markers preserve only earned IDs",
                  PreserveExistingRewardedMarker(id, state, allMarkers),
                  id >= FirstIPMarker && id <= LastIPMarker && state == QuestState::Rewarded);
            Check("invalid/non-marker IDs in list still do not bypass reset",
                  PreserveExistingRewardedMarker(id, state, partial),
                  (id == 66001 || id == 66008 || id == 66018) &&
                  state == QuestState::Rewarded);
        }
    }

    // An unearned stage must not be created; an active quest must not be
    // converted into a rewarded quest. Existing status is never rewritten.
    Check("new bot no stage 8", PreserveExistingRewardedMarker(66008, QuestState::None, allMarkers), false);
    Check("active quest not marked rewarded", PreserveExistingRewardedMarker(66008, QuestState::Active, allMarkers), false);
    Check("already rewarded stage 8 is retained", PreserveExistingRewardedMarker(66008, QuestState::Rewarded, allMarkers), true);
    Check("already rewarded stage 18 is retained", PreserveExistingRewardedMarker(66018, QuestState::Rewarded, allMarkers), true);
    Check("ordinary rewarded quest still reset", PreserveExistingRewardedMarker(10259, QuestState::Rewarded, allMarkers), false);

    std::cout << "F14 Playerbots keep-list candidate model: " << (checks - failures)
              << "/" << checks << " checks passed; PROTOTYPE ONLY, no gameplay.\n";
    return failures ? 1 : 0;
}
