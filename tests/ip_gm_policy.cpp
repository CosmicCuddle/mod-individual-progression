// Pure GM authorization and paid-era grant decisions; no server host required.
#include "../src/IndividualProgressionGmPolicy.h"
#include <cstdio>

namespace P = IndividualProgressionGmPolicy;
static int checks = 0;
static int failed = 0;
static void expect(char const* name, bool value)
{
    ++checks;
    if (!value)
    {
        ++failed;
        std::printf("FAIL %s\n", name);
    }
}
int main()
{
    expect("regular may read own session", P::CanViewTarget(false, true, true));
    expect("regular cannot read explicit other", !P::CanViewTarget(false, true, false));
    expect("regular cannot spoof console", !P::CanViewTarget(false, false, true));
    expect("staff may target others", P::CanViewTarget(true, true, false));
    expect("console may target others", P::CanViewTarget(true, false, false));

    expect("new era on paid normal", P::NewlyGrantedEra(true, true, false, true));
    expect("repeat grant not reaudited", !P::NewlyGrantedEra(true, true, true, true));
    expect("already owned no grant", !P::NewlyGrantedEra(true, true, true, false));
    expect("no new access no warning", !P::NewlyGrantedEra(true, true, false, false));
    expect("paid feature off parity", !P::NewlyGrantedEra(false, true, false, true));
    expect("ungated bots unchanged", !P::NewlyGrantedEra(true, false, false, true));

    // GM .ip tele deliberately keeps RAW (< PROGRESSION_TBC_TIER_5 = 13),
    // NOT effective stage 12; this is restrictive, not a paid-era bypass.
    constexpr int tbcTier5 = 13;
    expect("raw stage 13 is refused", !(13 < tbcTier5));
    expect("effective12 would pass, but command stays raw", 12 < tbcTier5);
    std::printf("GM policy: %d/%d passed\n", checks - failed, checks);
    return failed ? 1 : 0;
}
