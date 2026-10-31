// Standalone exhaustive C++17/C++20 tests for the EXACT candidate IP F14 header.
// Synthetic only: no core, no Playerbots identity source or database.
#include "../src/IndividualProgressionRandomBotStagePolicy.h"
#include <cstdint>
#include <initializer_list>
#include <iostream>

using namespace IndividualProgressionRandomBotStagePolicy;

namespace
{
    unsigned checks = 0;
    unsigned failures = 0;
    void Check(bool got, char const* why)
    {
        ++checks;
        if (!got)
        {
            if (failures < 20)
                std::cerr << "FAIL: " << why << "\n";
            ++failures;
        }
    }

    constexpr Request Input(Identity id, int race, TbcAccess access, Entry entry,
                            bool fresh, bool saved, int current,
                            bool requested, int target, int cap)
    {
        return { id, static_cast<std::uint8_t>(race), access, entry, fresh, saved,
                 static_cast<std::uint8_t>(current), requested,
                 static_cast<std::uint8_t>(target), cap };
    }
}

static_assert(Resolve(Input(Identity::VerifiedRandomBot, 10, TbcAccess::Available,
                            Entry::NewSelection, true, false, 0, true, 8, 0)).result ==
                  Result::InitializeNew, "Only eligible proven new rndbot stage8");
static_assert(Resolve(Input(Identity::VerifiedRandomBot, 10, TbcAccess::Available,
                            Entry::SavedEventRestore, false, true, 8, true, 0, 0)).result ==
                  Result::RejectWrite, "Saved stage8 must not downgrade");
static_assert(Resolve(Input(Identity::VerifiedRandomBot, 10, TbcAccess::Available,
                            Entry::NewSelection, false, true, 0, true, 8, 0)).result ==
                  Result::HoldMigration, "Previously saved stage0 is NOT a new bot");

int main()
{
    constexpr Entry entries[] = {
        Entry::NewSelection, Entry::SavedEventRestore, Entry::PlayerLogin,
        Entry::AdminStageWrite, Entry::BotGroupSync, Entry::GroupAdmission,
        Entry::ProgressionReward
    };
    constexpr Identity kinds[] = { Identity::Unknown, Identity::VerifiedOther,
                                   Identity::VerifiedRandomBot };
    constexpr TbcAccess accessStates[] = { TbcAccess::Unverified,
                                            TbcAccess::Unavailable,
                                            TbcAccess::Available };

    for (Identity id : kinds)
        for (int race : { 1, 10, 11 })
            for (TbcAccess access : accessStates)
                for (Entry entry : entries)
                    for (int current : { 0, 7, 8, 12, 13, 18, 19 })
                        for (int target : { 0, 7, 8, 12, 13, 18, 19 })
                            for (int cap : { -1, 0, 7, 8, 12, 13, 18, 19 })
                            {
                                Request const x = Input(id, race, access, entry, false,
                                                        true, current, true, target, cap);
                                Decision const d = Resolve(x);
                                Check(!(d.mayWrite && d.result != Result::PermitWrite),
                                      "Write only with explicit PermitWrite");
                                Check(!(d.mayWrite && (target < 8 || target > 18 || race == 1)),
                                      "Never write F14 invalid stage or race");
                                Check(!(d.mayWrite && (id != Identity::VerifiedRandomBot ||
                                                      access != TbcAccess::Available)),
                                      "No unverified identity or unavailable TBC writes");
                                Check(!(d.mayWrite && cap && target > cap),
                                      "Never bypass cap");
                                Check(!(d.mayWrite && current < 8),
                                      "Never silently migrate a saved stage0..7");
                                if (id == Identity::VerifiedOther || race == 1)
                                    Check(d.result == Result::DeferToExisting && !d.mayWrite &&
                                          !d.mayActivate, "Non-F14 characters defer, not approved");
                                else if (id == Identity::Unknown)
                                    Check(d.result == Result::HoldUnknownIdentity &&
                                          !d.mayWrite, "Unknown race10/11 identity held");
                                else if (access != TbcAccess::Available || cap < 0 ||
                                         cap > 18 || (cap && cap < 8))
                                    Check(d.result == Result::KeepDormant && !d.mayWrite &&
                                          !d.mayActivate, "Ineligible stage remains dormant");
                                else if (current < 8 || current > 18 ||
                                         (cap && current > cap))
                                    Check(d.result == Result::HoldMigration && !d.mayWrite &&
                                          !d.mayActivate, "Saved invalid stage cannot migrate");
                                else if (current == target)
                                    Check(d.result == Result::PreserveExisting &&
                                          d.mayActivate && !d.mayWrite,
                                          "Idempotent request never rewrites rewarded quests");
                                else if (target < 8 || target > 18 || (cap && target > cap))
                                    Check(d.result == Result::RejectWrite &&
                                          d.mayActivate && !d.mayWrite,
                                          "Illegal write rejected before side effects");
                                else
                                    Check(d.result == Result::PermitWrite && d.mayWrite &&
                                          d.targetStage == target,
                                          "Legal stage8..18 only, within cap");
                            }

    // The only permitted initial write is positive, unsaved, eligible, exact stage8.
    for (Identity id : kinds)
        for (int race : { 1, 10, 11 })
            for (TbcAccess a : accessStates)
                for (Entry route : entries)
                    for (int proposed : { 0, 7, 8, 9, 13, 18, 19 })
                        for (int cap : { 0, 7, 8, 12, 18 })
                        {
                            Decision const d = Resolve(Input(id, race, a, route, true,
                                                             false, 0, true, proposed, cap));
                            bool const expected = id == Identity::VerifiedRandomBot &&
                                (race == 10 || race == 11) && a == TbcAccess::Available &&
                                route == Entry::NewSelection && proposed == 8 &&
                                (cap == 0 || cap >= 8);
                            Check((d.result == Result::InitializeNew) == expected,
                                  "Only positively new, eligible, stage8 initializes");
                            Check(d.mayWrite == expected,
                                  "Only legitimate verified first F14 stage can be written");
                        }

    // An unknown current marker at restore is neither new nor default stage0.
    for (Entry e : entries)
        for (int race : { 10, 11 })
        {
            Decision const d = Resolve(Input(Identity::VerifiedRandomBot, race,
                                             TbcAccess::Available, e, false,
                                             false, 0, true, 8, 0));
            Check(d.result == Result::HoldMigration && !d.mayWrite && !d.mayActivate,
                  "Missing persisted stage cannot be interpreted as new");
        }

    // No requested stage: saved stage8+ must not rewrite markers during relog.
    for (Entry e : entries)
        for (int race : { 10, 11 })
            for (int current = 8; current <= 18; ++current)
            {
                Decision const d = Resolve(Input(Identity::VerifiedRandomBot, race,
                                                 TbcAccess::Available, e, false,
                                                 true, current, false, 0, 0));
                Check(d.result == Result::PreserveExisting && d.mayActivate &&
                      !d.mayWrite, "Saved stage8+ preserved on any no-write route");
            }

    std::cout << "F14 verified-rndbot C++ policy: " << checks - failures
              << "/" << checks << " checks passed\n";
    return failures ? 1 : 0;
}
