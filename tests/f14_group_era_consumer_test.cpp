#include "IndividualProgressionF14GroupEraConsumer.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace C = IndividualProgressionF14GroupEraConsumer;
using Era = C::Era;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; return EXIT_FAILURE; } } while (false)

int main()
{
    C::Identity const first{100, 1, 1, 0, 200, 1, 300, 1};
    C::Input elf{true, first, 10, 15, 0};
    C::Tracker<3> tracker;
    CHECK(tracker.ObserveGrouped(elf).effectiveEra == Era::BurningCrusade);
    CHECK(tracker.Current(elf).temporary);
    CHECK(tracker.ObserveGrouped(elf).status == C::Status::Temporary);

    // The leader's changing era requires a new revision; earned Wrath remains 15.
    elf.identity.leaderRevision = 2;
    elf.groupStage = 13;
    CHECK(tracker.ObserveGrouped(elf).effectiveEra == Era::Wrath);
    elf.identity.leaderRevision = 3;
    elf.groupStage = 8;
    CHECK(tracker.ObserveGrouped(elf).effectiveEra == Era::BurningCrusade);
    CHECK(elf.earnedStage == 15);

    // Every departure kind drops the temporary era and exposes exact earned.
    std::array<C::Departure, 7> const departures{
        C::Departure::Leave, C::Departure::Disband, C::Departure::Kick,
        C::Departure::LeaderChange, C::Departure::Relog, C::Departure::MapChange,
        C::Departure::SessionInvalidation};
    for (C::Departure reason : departures)
    {
        ++elf.identity.membershipGeneration;
        CHECK(tracker.ObserveGrouped(elf).temporary);
        C::View const gone = tracker.Revoke(elf.identity, true, elf.race, elf.earnedStage, reason);
        CHECK(gone.status == C::Status::Revoked);
        CHECK(gone.effectiveEra == Era::Wrath && !gone.temporary);
        CHECK(tracker.Current(elf).effectiveEra == Era::Wrath);
        CHECK(!tracker.Current(elf).temporary);
        CHECK(tracker.ObserveGrouped(elf).status == C::Status::Ambiguous);
    }

    // A delayed old leave never erases the newer map/session/membership view.
    C::Identity const old = elf.identity;
    ++elf.identity.sessionEpoch;
    elf.identity.mapGeneration = 1;
    elf.identity.membershipGeneration = 1;
    elf.identity.leaderRevision = 1;
    CHECK(tracker.ObserveGrouped(elf).temporary);
    CHECK(tracker.Revoke(old, true, 10, 15, C::Departure::Relog).status == C::Status::Stale);
    CHECK(tracker.Revoke(old, false, 10, 15, C::Departure::SessionInvalidation).status ==
          C::Status::Stale);
    CHECK(tracker.Current(elf).temporary);
    CHECK(tracker.ObserveGrouped(C::Input{true, old, 10, 15, 8}).status == C::Status::Stale);
    CHECK(tracker.Current(elf).temporary);
    C::Identity oldMap = elf.identity;
    ++elf.identity.mapGeneration;
    elf.identity.mapId = 1;
    CHECK(tracker.ObserveGrouped(elf).temporary);
    CHECK(tracker.Revoke(oldMap, true, 10, 15, C::Departure::MapChange).status == C::Status::Stale);
    CHECK(tracker.Current(elf).temporary);

    // Same-version conflicting identity or group facts quarantine only this bot.
    C::Input human{true, {101, 1, 1, 0, 201, 1, 301, 1}, 1, 14, 0};
    CHECK(tracker.ObserveGrouped(human).effectiveEra == Era::Vanilla);
    C::Input conflict = elf;
    conflict.identity.groupGuid = 999;
    CHECK(tracker.ObserveGrouped(conflict).status == C::Status::Ambiguous);
    CHECK(!tracker.Current(elf).temporary);
    CHECK(tracker.Current(elf).effectiveEra == Era::Wrath);
    CHECK(tracker.Current(human).temporary);

    // Eligibility proof may be revoked before session teardown arrives.
    // The captured exact context still clears and tombstones the override.
    C::Tracker<1> lostEligibility;
    CHECK(lostEligibility.ObserveGrouped(human).temporary);
    CHECK(lostEligibility.Revoke(human.identity, false, human.race, human.earnedStage,
                                 C::Departure::SessionInvalidation).status == C::Status::UnverifiedBot);
    CHECK(!lostEligibility.Current(human).temporary);
    CHECK(lostEligibility.Current(human).effectiveEra == Era::Wrath);
    CHECK(lostEligibility.ObserveGrouped(human).status == C::Status::Ambiguous);
    CHECK(tracker.ObserveGrouped(elf).status == C::Status::Ambiguous);
    CHECK(tracker.Revoke(conflict.identity, true, 10, 15, C::Departure::Leave).status ==
          C::Status::Ambiguous);
    CHECK(tracker.Current(human).temporary);
    ++elf.identity.membershipGeneration;
    CHECK(tracker.ObserveGrouped(elf).temporary);
    C::Input changedStage = elf;
    changedStage.groupStage = 0;
    CHECK(tracker.ObserveGrouped(changedStage).status == C::Status::Ambiguous);
    CHECK(tracker.Current(human).temporary);

    // Unknown eligibility/identity never creates or reads a temporary override.
    C::Input unknown = human;
    unknown.eligibleBot = false;
    CHECK(tracker.ObserveGrouped(unknown).status == C::Status::UnverifiedBot);
    CHECK(!tracker.Current(unknown).temporary);
    unknown = human;
    unknown.identity.sessionEpoch = 0;
    CHECK(tracker.ObserveGrouped(unknown).status == C::Status::InvalidIdentity);
    CHECK(!tracker.Current(unknown).temporary);
    CHECK(tracker.Current(human).temporary);

    // Missing TBC-race earned floor is diagnosed on departure, never repaired.
    C::Input draenei{true, {102, 1, 1, 0, 202, 1, 302, 1}, 11, 0, 0};
    CHECK(tracker.ObserveGrouped(draenei).effectiveEra == Era::BurningCrusade);
    C::View const missing = tracker.Revoke(draenei.identity, true, 11, 0, C::Departure::Kick);
    CHECK(missing.status == C::Status::EarnedFloorMissing);
    CHECK(missing.effectiveEra == Era::Vanilla && !missing.temporary);
    CHECK(draenei.earnedStage == 0);

    // Bounded admission fails closed and cannot evict another bot's evidence.
    C::Tracker<1> one;
    CHECK(one.ObserveGrouped(human).temporary);
    CHECK(one.ObserveGrouped(elf).status == C::Status::Capacity);
    CHECK(one.Current(human).temporary);
    CHECK(one.Current(elf).effectiveEra == Era::Wrath);
    CHECK(!one.Current(elf).temporary);
    CHECK(one.Revoke(human.identity, true, human.race, 8, C::Departure::Leave).effectiveEra ==
          Era::BurningCrusade); // Exact caller-supplied earned state, not cached 14.
    C::Input loweredEarned = human;
    loweredEarned.earnedStage = 8;
    CHECK(!one.Current(loweredEarned).temporary);
    CHECK(one.Current(loweredEarned).effectiveEra == Era::BurningCrusade);
    CHECK(one.ObserveGrouped(elf).status == C::Status::Capacity); // Tombstone not evicted.
    ++human.identity.membershipGeneration;
    CHECK(one.ObserveGrouped(human).temporary); // Existing bot can advance in its slot.

    // Numeric wrap to an older nonzero generation is refused, not accepted
    // as a new session. Equal-version ABA needs a caller non-reuse guarantee.
    C::Tracker<1> wrap;
    C::Input nearWrap = human;
    nearWrap.identity.sessionEpoch = std::numeric_limits<std::uint64_t>::max();
    CHECK(wrap.ObserveGrouped(nearWrap).temporary);
    C::Input wrapped = nearWrap;
    wrapped.identity.sessionEpoch = 1;
    CHECK(wrap.ObserveGrouped(wrapped).status == C::Status::Stale);
    CHECK(wrap.Revoke(wrapped.identity, false, wrapped.race, wrapped.earnedStage,
                      C::Departure::Relog).status == C::Status::Stale);
    CHECK(wrap.Current(nearWrap).temporary);

    std::size_t combinations = 0;
    for (std::uint8_t race : std::array<std::uint8_t, 10>{1, 2, 3, 4, 5, 6, 7, 8, 10, 11})
        for (std::uint8_t earned = 0; earned <= 18; ++earned)
            for (std::uint8_t group = 0; group <= 18; ++group)
            {
                C::Tracker<1> each;
                C::Input input{true, first, race, earned, group};
                C::View const view = each.ObserveGrouped(input);
                Era const expected = (race == 10 || race == 11) && group <= 7
                    ? Era::BurningCrusade : C::Floor::GroupPolicy::GetExpansion(group);
                CHECK(view.status == C::Status::Temporary);
                CHECK(view.effectiveEra == expected && view.temporary);
                CHECK(each.Current(input).effectiveEra == expected);
                CHECK(input.earnedStage == earned);
                ++combinations;
            }
    std::cout << "PASS F14 group-era consumer: " << combinations
              << " combinations, all departures, stale/ambiguous/capacity fences\n";
    return EXIT_SUCCESS;
}
