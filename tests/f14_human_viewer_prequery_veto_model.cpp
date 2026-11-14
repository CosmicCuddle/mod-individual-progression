// Pure future F14 HUMAN VIEWER negative session/GM pre-query gate model.
// Never permits live hiding: actual F14 policy remains UNREGISTERED/default FALSE.
// This checks conjunctive short-circuit, not independently verified normal
// account, Vanilla-era entitlement, complete combat rights, or client ACK.
#include <cstdint>
#include <iostream>

struct Facts
{
    bool baselineVisible;
    bool worldNpc;
    bool pet;
    bool viewerLive;
    bool targetLive;
    bool sameMap;
    bool withinMapUpdate;
    bool viewerSessionPresent;
    bool viewerHeadless;
    bool viewerIsActiveGM;
    bool futurePolicyWouldSayYes;
    bool existingHumanRightsVeto;
};

bool DefaultFalse(Facts const&, int& calls)
{
    ++calls;
    return false;
}

bool Hypothetical(Facts const& f, int& calls)
{
    ++calls;
    return f.futurePolicyWouldSayYes;
}

template<bool Batched, typename Policy>
bool ProposedF14Suppression(Facts const& f, Policy policy, int& calls)
{
    (void)Batched;
    return f.baselineVisible && f.worldNpc && !f.pet &&
           f.viewerLive && f.targetLive && f.sameMap && f.withinMapUpdate &&
           f.viewerSessionPresent && !f.viewerHeadless &&
           !f.viewerIsActiveGM &&
           policy(f, calls) && !f.existingHumanRightsVeto;
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL: " << #condition << " at line " << __LINE__ << '\n'; \
        return 1; \
    } \
} while (0)

int main()
{
    int cases = 0;
    for (std::uint32_t bits = 0; bits < (1u << 12); ++bits)
    {
        auto bit = [bits](int i) { return (bits & (1u << i)) != 0; };
        Facts const f{
            bit(0), bit(1), bit(2), bit(3), bit(4), bit(5),
            bit(6), bit(7), bit(8), bit(9), bit(10), bit(11)
        };
        bool const allowedToCallPositivePolicy =
            f.baselineVisible && f.worldNpc && !f.pet &&
            f.viewerLive && f.targetLive && f.sameMap && f.withinMapUpdate &&
            f.viewerSessionPresent && !f.viewerHeadless &&
            !f.viewerIsActiveGM;

        for (int path = 0; path < 2; ++path)
        {
            int actualCalls = 0;
            bool const currentBehaviour = path
                ? ProposedF14Suppression<true>(f, DefaultFalse, actualCalls)
                : ProposedF14Suppression<false>(f, DefaultFalse, actualCalls);
            CHECK(!currentBehaviour);
            CHECK(actualCalls == (allowedToCallPositivePolicy ? 1 : 0));

            int hypotheticalCalls = 0;
            bool const futureBehaviour = path
                ? ProposedF14Suppression<true>(f, Hypothetical, hypotheticalCalls)
                : ProposedF14Suppression<false>(f, Hypothetical, hypotheticalCalls);
            CHECK(hypotheticalCalls == (allowedToCallPositivePolicy ? 1 : 0));
            CHECK(futureBehaviour == (allowedToCallPositivePolicy &&
                                      f.futurePolicyWouldSayYes &&
                                      !f.existingHumanRightsVeto));
            if (!f.viewerSessionPresent || f.viewerHeadless || f.viewerIsActiveGM)
            {
                CHECK(!futureBehaviour);
                CHECK(hypotheticalCalls == 0);
            }
            ++cases;
        }
    }
    CHECK(cases == 8192);
    std::cout << "F14 human-viewer prequery exclusion: "
              << cases << " exhaustive path-state cases PASS\n";
    return 0;
}
