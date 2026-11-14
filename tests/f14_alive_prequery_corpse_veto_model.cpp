// F14 pure model: NEW positive policy is never consulted on non-Alive NPC.
// The model NEVER clears previously-hidden/restoration debt. NOT a client test.
#include <cstdint>
#include <iostream>

enum class DeathState : std::uint8_t
{
    Alive, JustDied, Corpse, Dead, JustRespawned
};

struct Facts
{
    bool baselineVisible;
    bool isCreature;
    bool isPet;
    bool viewerInWorld;
    bool targetInWorld;
    bool sameMap;
    bool mapUpdateFrame;
    bool humanViewerSession;
    bool directHumanRightsVeto;
    bool hypotheticalPositivePolicy;
    bool previouslyHidden;
    DeathState death;
};

bool DefaultOff(Facts const&, int& calls)
{
    ++calls;
    return false;
}

bool HypotheticalPositive(Facts const& f, int& calls)
{
    ++calls;
    return f.hypotheticalPositivePolicy;
}

template<bool Batched, typename Policy>
bool RequestNEWF14Suppression(Facts const& f, Policy policy, int& calls)
{
    (void)Batched;
    return f.baselineVisible && f.isCreature && !f.isPet &&
        f.viewerInWorld && f.targetInWorld && f.sameMap &&
        f.mapUpdateFrame && f.humanViewerSession &&
        f.death == DeathState::Alive && policy(f, calls) &&
        !f.directHumanRightsVeto;
}

#define CHECK(x) do { \
    if (!(x)) { \
        std::cerr << "FAIL " << #x << " at " << __LINE__ << '\n'; \
        return 1; \
    } \
} while (false)

int main()
{
    std::uint32_t cases = 0;
    for (int state = 0; state != 5; ++state)
    {
        for (std::uint32_t bits = 0; bits < (1u << 11); ++bits)
        {
            auto bit = [bits](int i) { return (bits & (1u << i)) != 0; };
            Facts const f{
                bit(0), bit(1), bit(2), bit(3), bit(4), bit(5),
                bit(6), bit(7), bit(8), bit(9), bit(10),
                static_cast<DeathState>(state)
            };
            bool const prerequisite =
                f.baselineVisible && f.isCreature && !f.isPet &&
                f.viewerInWorld && f.targetInWorld &&
                f.sameMap && f.mapUpdateFrame && f.humanViewerSession &&
                f.death == DeathState::Alive;

            for (int path = 0; path != 2; ++path)
            {
                int defaultCalls = 0;
                bool const actualDefault = path
                    ? RequestNEWF14Suppression<true>(f, DefaultOff, defaultCalls)
                    : RequestNEWF14Suppression<false>(f, DefaultOff, defaultCalls);
                CHECK(!actualDefault);
                CHECK(defaultCalls == (prerequisite ? 1 : 0));

                int hypotheticalCalls = 0;
                bool const hypothetical = path
                    ? RequestNEWF14Suppression<true>(f, HypotheticalPositive, hypotheticalCalls)
                    : RequestNEWF14Suppression<false>(f, HypotheticalPositive, hypotheticalCalls);
                CHECK(hypotheticalCalls == (prerequisite ? 1 : 0));
                CHECK(hypothetical == (prerequisite && f.hypotheticalPositivePolicy &&
                                      !f.directHumanRightsVeto));
                if (f.death != DeathState::Alive)
                {
                    CHECK(!hypothetical);
                    CHECK(hypotheticalCalls == 0);
                }
                // IMPORTANT: denying NEW concealment is not a RESTORE action.
                // If the exact prior viewer-pair was hidden, that obligation
                // remains unchanged through death and corpse lifecycle.
                bool const restoreOwedBefore = f.previouslyHidden;
                bool const restoreOwedAfter = restoreOwedBefore;
                CHECK(restoreOwedAfter == f.previouslyHidden);
                ++cases;
            }
        }
    }
    CHECK(cases == 20480);
    std::cout << "F14 alive-only new-hide gate: "
              << cases << " exhaustive path/death-state cases PASS; "
              << "prior-hidden restoration debt NOT cleared\n";
    return 0;
}
