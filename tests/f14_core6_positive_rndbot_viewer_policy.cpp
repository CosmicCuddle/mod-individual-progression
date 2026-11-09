// Actual new F14 Core6 positive-only IP pure policy, not a server/client test.
// Both actual Core6 callbacks remain default FALSE without an EXTRA macro.
#include "../src/IndividualProgressionF14Core6PositiveRndbotViewerPolicy.h"
#include <cstdint>
#include <iostream>
#include <initializer_list>

namespace P = CosmicIPF14Core6PositiveRndbot;

int main()
{
    std::uint32_t checks = 0;
    std::uint32_t failures = 0;
    auto test = [&](bool good) { ++checks; if (!good) ++failures; };

    for (std::uint32_t vm = 0; vm < 64; ++vm)
    for (std::uint8_t stage : {std::uint8_t(0), std::uint8_t(7),
                              std::uint8_t(8), std::uint8_t(18),
                              std::uint8_t(19), std::uint8_t(255)})
    for (std::uint32_t pm = 0; pm < 32; ++pm)
    for (std::uint8_t race : {std::uint8_t(1), std::uint8_t(10),
                             std::uint8_t(11), std::uint8_t(12)})
    for (std::uint32_t am = 0; am < 8; ++am)
    {
        P::Viewer v;
        v.confirmedNormalHumanAccount = (vm & 1) != 0;
        v.confirmedNonGM = (vm & 2) != 0;
        v.activeNonHeadlessSession = (vm & 4) != 0;
        v.inWorld = (vm & 8) != 0;
        v.confirmedNotSelf = (vm & 16) != 0;
        v.effectiveStageKnown = (vm & 32) != 0;
        v.effectiveStage = stage;
        P::Publication pub;
        pub.verifiedSelectedRandomBot = (pm & 1) != 0;
        pub.fullGuidMatches = (pm & 2) != 0;
        pub.currentAccountProven = (pm & 4) != 0;
        pub.raceMatches = (pm & 8) != 0;
        pub.nonzeroCurrentLoginEpoch = (pm & 16) != 0;
        pub.publishedRace = race;
        bool const actorWorld = (am & 1) != 0;
        bool const actorHeadless = (am & 2) != 0;
        bool const actorNonGM = (am & 4) != 0;
        bool const viewerExpected = vm == 63 && stage < 8;
        bool const sourceExpected = pm == 31 && (race == 10 || race == 11);
        bool const expectedWho = viewerExpected && sourceExpected;
        bool const expectedActor = expectedWho && am == 7;
        test(P::ViewerIsVanilla(v) == viewerExpected);
        test(P::HasPositiveTargetProof(pub) == sourceExpected);
        test(P::ExcludeWho(v, pub) == expectedWho);
        test(P::HideActor(v, pub, actorWorld, actorHeadless,
                          actorNonGM) == expectedActor);
    }

    P::Viewer unknown;
    P::Publication absent;
    test(!P::ExcludeWho(unknown, absent));
    test(!P::HideActor(unknown, absent, true, true, true));

    P::Viewer vanilla{true,true,true,true,true,true,7};
    P::Publication verified{true,true,true,true,true,10};
    test(P::ExcludeWho(vanilla, verified));
    test(P::HideActor(vanilla, verified, true, true, true));

    // Direct negative witnesses for human, Altbot and stale/unselected source:
    for (std::uint32_t mask = 1; mask <= 16; mask <<= 1)
    {
        P::Publication candidate = verified;
        if (mask == 1) candidate.verifiedSelectedRandomBot = false;
        if (mask == 2) candidate.fullGuidMatches = false;
        if (mask == 4) candidate.currentAccountProven = false;
        if (mask == 8) candidate.raceMatches = false;
        if (mask == 16) candidate.nonzeroCurrentLoginEpoch = false;
        test(!P::ExcludeWho(vanilla, candidate));
        test(!P::HideActor(vanilla, candidate, true, true, true));
    }

    std::cout << "IP F14 CORE6 VERIFIED RANDBOT POLICY: " << checks
              << " synthetic checks, " << failures << " failures; no packets or client"
              << '\n';
    return failures ? 1 : 0;
}
