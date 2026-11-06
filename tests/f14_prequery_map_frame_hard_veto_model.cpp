// Pure negative F14 pre-query Map::Update frame gate MODEL, not live Core.
// No AzerothCore process, positive hide script, client, database or packets.
// Enumerates the same conjunctive order as the disposable Core overlay.
#include <cassert>
#include <cstdint>
#include <iostream>

struct SourceFacts
{
    bool baselineVisible;
    bool creatureNpc;
    bool isPet;
    bool viewerInWorld;
    bool creatureInWorld;
    bool mapKnown;
    bool sameMap;
    bool inThisMapUpdateInvocation;
    bool futurePolicyWouldSayYes;
    bool directHumanRightsVeto;
};

bool DefaultFalsePolicy(SourceFacts const&, int& calls)
{
    ++calls;
    return false;
}

bool HypotheticalPolicyOnly(SourceFacts const& f, int& calls)
{
    ++calls;
    return f.futurePolicyWouldSayYes;
}

template <bool Batched, typename Policy>
bool ProposedF14Decision(SourceFacts const& f, Policy policy, int& calls)
{
    (void)Batched; // both production visibility paths share same hard conditions
    return f.baselineVisible && f.creatureNpc && !f.isPet &&
           f.viewerInWorld && f.creatureInWorld && f.mapKnown &&
           f.sameMap && f.inThisMapUpdateInvocation &&
           policy(f, calls) && !f.directHumanRightsVeto;
}

int main()
{
    int scenarios = 0;
    for (std::uint32_t bits = 0; bits < (1u << 10); ++bits)
    {
        auto bit = [bits](int n) -> bool { return (bits & (1u << n)) != 0; };
        SourceFacts const f{
            bit(0), bit(1), bit(2), bit(3), bit(4),
            bit(5), bit(6), bit(7), bit(8), bit(9)
        };
        bool const prerequisite =
            f.baselineVisible && f.creatureNpc && !f.isPet &&
            f.viewerInWorld && f.creatureInWorld && f.mapKnown &&
            f.sameMap && f.inThisMapUpdateInvocation;
        for (int mode = 0; mode < 2; ++mode)
        {
            int realPolicyCalls = 0;
            bool const actualDefaultFalse =
                mode == 0
                    ? ProposedF14Decision<false>(f, DefaultFalsePolicy, realPolicyCalls)
                    : ProposedF14Decision<true>(f, DefaultFalsePolicy, realPolicyCalls);
            assert(!actualDefaultFalse); // no live positive provider
            assert(realPolicyCalls == (prerequisite ? 1 : 0));

            int hypotheticalCalls = 0;
            bool const hypotheticallyApproved =
                mode == 0
                    ? ProposedF14Decision<false>(f, HypotheticalPolicyOnly, hypotheticalCalls)
                    : ProposedF14Decision<true>(f, HypotheticalPolicyOnly, hypotheticalCalls);
            assert(hypotheticalCalls == (prerequisite ? 1 : 0));
            assert(hypotheticallyApproved ==
                   (prerequisite && f.futurePolicyWouldSayYes &&
                    !f.directHumanRightsVeto));
            if (!f.inThisMapUpdateInvocation || !f.sameMap ||
                !f.mapKnown || !f.viewerInWorld || !f.creatureInWorld)
            {
                assert(!hypotheticallyApproved);
                assert(hypotheticalCalls == 0);
            }
            if (!f.baselineVisible || !f.creatureNpc || f.isPet)
            {
                assert(!hypotheticallyApproved);
                assert(hypotheticalCalls == 0);
            }
            ++scenarios;
        }
    }
    assert(scenarios == 2048);
    std::cout << "F14 pre-query hard frame-veto pure model: "
              << scenarios << " exhaustive path/boolean cases passed\n";
}
