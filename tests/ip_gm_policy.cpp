// Standalone tests for IndividualProgressionGmPolicy.h (.ip get access and
// .ip set paid-expansion audit). Section "M" is a labelled MODEL combining it
// with the real paid-expansion rules; it is not the AzerothCore code itself.
#include "../src/IndividualProgressionGmPolicy.h"
#include "../src/IndividualProgressionExpansionPolicy.h"

#include <cstdint>
#include <iostream>
#include <string>

using namespace IndividualProgressionGmPolicy;
namespace EP = IndividualProgressionExpansionPolicy;

namespace
{
    int failures = 0;
    int checks = 0;

    template <typename T>
    void Check(T actual, T expected, std::string const& context)
    {
        ++checks;
        if (!(actual == expected))
        {
            ++failures;
            std::cerr << "FAIL: " << context << " (actual=" << static_cast<int>(actual)
                      << ", expected=" << static_cast<int>(expected) << ")\n";
        }
    }

    EP::CharacterFacts Facts(bool active, bool gated, std::uint8_t raw, bool wrathPaid = false)
    {
        EP::CharacterFacts f;
        f.featureActive = active;
        f.gated = gated;
        f.rawStage = raw;
        f.wrathPaid = wrathPaid;
        return f;
    }

    // MODEL of BeginSetAudit / ForceUpdate / FinishSetAudit (labelled).
    struct Warning { bool tbc; bool wrath; };
    Warning ModelSet(bool active, bool gated, std::uint8_t oldRaw, std::uint8_t newRaw, bool wrathPaid = false)
    {
        if (!active || !gated)
            return {false, false};
        EP::CharacterFacts const before = Facts(active, gated, oldRaw, wrathPaid);
        EP::CharacterFacts const after = Facts(active, gated, newRaw, wrathPaid);
        return {NewlyGranted(EP::Owns(before, EP::Era::Tbc), EP::Owns(after, EP::Era::Tbc)),
                NewlyGranted(EP::Owns(before, EP::Era::Wrath), EP::Owns(after, EP::Era::Wrath))};
    }
}

int main()
{
    // ---- 1. .ip get target matrix ----
    for (int console = 0; console <= 1; ++console)
        for (int gm = 0; gm <= 1; ++gm)
            for (int requested = 0; requested <= 1; ++requested)
                for (int self = 0; self <= 1; ++self)
                {
                    if (self && !requested)
                        continue;
                    GetTarget expected;
                    if (console || gm)
                        expected = GetTarget::RequestedOrSelection;
                    else if (!requested || self)
                        expected = GetTarget::Self;
                    else
                        expected = GetTarget::RefuseOther;
                    Check(DecideGetTarget(console, gm, requested, self), expected,
                          "get console=" + std::to_string(console) + " gm=" + std::to_string(gm) +
                          " requested=" + std::to_string(requested) + " self=" + std::to_string(self));
                }
    // Named acceptance cases.
    Check(DecideGetTarget(false, false, false, false), GetTarget::Self, "player self (no name; selection ignored)");
    Check(DecideGetTarget(false, false, true, true), GetTarget::Self, "player naming themself");
    Check(DecideGetTarget(false, false, true, false), GetTarget::RefuseOther, "player naming someone else refused");
    Check(DecideGetTarget(false, true, true, false), GetTarget::RequestedOrSelection, "GM explicit target");
    Check(DecideGetTarget(false, true, false, false), GetTarget::RequestedOrSelection, "GM selection or self");
    Check(DecideGetTarget(true, false, true, false), GetTarget::RequestedOrSelection, "console named target");
    Check(DecideGetTarget(true, false, false, false), GetTarget::RequestedOrSelection, "console without name (handler refuses)");

    // ---- 2. NewlyGranted truth table ----
    Check(NewlyGranted(false, true), true, "not owned -> owned");
    Check(NewlyGranted(true, true), false, "owned -> owned");
    Check(NewlyGranted(false, false), false, "not owned -> not owned");
    Check(NewlyGranted(true, false), false, "owned -> not owned (removal is not a grant)");

    // ---- M. MODEL: .ip set warnings ----
    Warning w = ModelSet(true, true, 7, 8);
    Check(w.tbc, true, "M: unpaid 7 -> 8 warns TBC");
    Check(w.wrath, false, "M: unpaid 7 -> 8 no Wrath");
    w = ModelSet(true, true, 13, 14);
    Check(w.wrath, true, "M: unpaid 13 -> 14 warns Wrath");
    Check(w.tbc, false, "M: unpaid 13 -> 14 TBC already owned");
    w = ModelSet(true, true, 3, 15);
    Check(w.tbc && w.wrath, true, "M: 3 -> 15 warns both");
    w = ModelSet(true, true, 13, 14, true);
    Check(w.tbc || w.wrath, false, "M: paid Wrath holder 13 -> 14 no warning");
    w = ModelSet(true, true, 8, 8);
    Check(w.tbc || w.wrath, false, "M: repeated set 8 no warning");
    w = ModelSet(true, true, 14, 16);
    Check(w.tbc || w.wrath, false, "M: already Wrath 14 -> 16 no warning");
    w = ModelSet(true, true, 12, 13);
    Check(w.tbc || w.wrath, false, "M: 12 -> 13 grants no Wrath (unpaid 13 is TBC)");
    w = ModelSet(true, true, 9, 3);
    Check(w.tbc || w.wrath, false, "M: lowering never warns as a grant");
    for (int from = 0; from <= 18; ++from)
        for (int to = 0; to <= 18; ++to)
        {
            Warning off = ModelSet(false, true, static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to));
            Warning bot = ModelSet(true, false, static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to));
            std::string const c = std::to_string(from) + " -> " + std::to_string(to);
            Check(off.tbc || off.wrath, false, "M: feature OFF never warns " + c);
            Check(bot.tbc || bot.wrath, false, "M: bot/excluded never warns " + c);
            Warning on = ModelSet(true, true, static_cast<std::uint8_t>(from), static_cast<std::uint8_t>(to));
            Check(on.tbc, from < 8 && to >= 8, "M: TBC warning iff crossing into 8+ " + c);
            Check(on.wrath, from < 14 && to >= 14, "M: Wrath warning iff crossing into 14+ " + c);
        }

    std::cout << "GM command policy: " << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
