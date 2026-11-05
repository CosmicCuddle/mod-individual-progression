// Native tests for the F14 verified RandomBot stage-8 floor durability rule
// (pure header). GCC/Clang, C++17/C++20, with and without NDEBUG.
// Source/model proof only, not in-game behaviour.
#include "IndividualProgressionF14StageFloorPolicy.h"

#include <cstdint>
#include <cstdio>

namespace F = IndividualProgressionF14StageFloorPolicy;

static unsigned g_pass = 0;
static unsigned g_fail = 0;

#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        if (cond)                                                                \
            ++g_pass;                                                            \
        else                                                                     \
        {                                                                        \
            ++g_fail;                                                            \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

namespace
{
    bool Blocks(bool on, bool verified, int current, int requested)
    {
        return F::BlocksVerifiedFloorDowngrade(on, verified, static_cast<std::uint8_t>(current),
                                               static_cast<std::uint8_t>(requested));
    }

    // Exhaustive table: the rule blocks exactly when ON, verified, earned
    // 8..18 and requested 0..7. The progression cap is NOT an input.
    void TestExhaustiveTable()
    {
        for (int on = 0; on <= 1; ++on)
            for (int verified = 0; verified <= 1; ++verified)
                for (int current = 0; current <= 255; ++current)
                    for (int requested = 0; requested <= 255; ++requested)
                    {
                        bool const expected = on && verified &&
                            current >= 8 && current <= 18 && requested < 8;
                        CHECK(Blocks(on, verified, current, requested) == expected);
                    }
    }

    // CS01 review witnesses (6644343493, 6644617347): an earned stage above a
    // positive cap must still be preserved, never destroyed.
    void TestCapWitnesses()
    {
        struct W { int current, requested, cap; };
        W const witnesses[] = {{12, 3, 8}, {8, 0, 7}, {18, 7, 12}};
        for (W const& w : witnesses)
        {
            CHECK(Blocks(true, true, w.current, w.requested));      // preserved
            CHECK(!Blocks(false, true, w.current, w.requested));    // OFF: historical
            CHECK(!Blocks(true, false, w.current, w.requested));    // unverified: historical
            CHECK(!Blocks(false, false, w.current, w.requested));
            CHECK(F::IsHistoricOverCapStage(static_cast<std::uint8_t>(w.current), w.cap));
        }
    }

    void TestNamedOwnerCases()
    {
        // Saved stage 0-7 (Vanilla): never fabricated to 8, writes proceed.
        for (int s = 0; s < 8; ++s)
            CHECK(!Blocks(true, true, s, 0));
        // Saved 8-18 with any Vanilla request: kept as is (not lowered to 8).
        for (int s = 8; s <= 18; ++s)
            for (int r = 0; r < 8; ++r)
                CHECK(Blocks(true, true, s, r));
        // Same-era or higher requests are left to the established route
        // (e.g. TBC-band group sync 12 -> 8, login 71+ -> 13, 8 -> 13).
        CHECK(!Blocks(true, true, 12, 8));
        CHECK(!Blocks(true, true, 15, 13));
        CHECK(!Blocks(true, true, 8, 13));
        // Corrupt/unsupported stored stage: not protected.
        CHECK(!Blocks(true, true, 19, 0));
        CHECK(!Blocks(true, true, 255, 0));
        // Login level write (<=60 -> 0) for a verified bot at earned 8.
        CHECK(Blocks(true, true, 8, 0));
    }

    void TestOverCapDiagnosticOnly()
    {
        CHECK(!F::IsHistoricOverCapStage(12, 0));   // 0 = unlimited
        CHECK(!F::IsHistoricOverCapStage(12, -1));
        CHECK(!F::IsHistoricOverCapStage(12, 12));
        CHECK(!F::IsHistoricOverCapStage(8, 18));
        CHECK(F::IsHistoricOverCapStage(13, 12));
        CHECK(F::IsHistoricOverCapStage(8, 1));
    }
}

int main()
{
    TestExhaustiveTable();
    TestCapWitnesses();
    TestNamedOwnerCases();
    TestOverCapDiagnosticOnly();
    std::printf("F14 STAGE FLOOR POLICY %u/%u passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
