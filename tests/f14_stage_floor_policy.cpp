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
    bool Blocks(bool on, bool verified, int current, int requested, int limit)
    {
        return F::BlocksVerifiedFloorDowngrade(on, verified, static_cast<std::uint8_t>(current),
                                               static_cast<std::uint8_t>(requested), limit);
    }

    // Exhaustive table: the rule blocks exactly when ON, verified, earned
    // 8..18, requested 0..7, and the current stage is within a positive cap.
    void TestExhaustiveTable()
    {
        int const limits[] = {-1, 0, 1, 5, 7, 8, 9, 10, 12, 13, 17, 18, 19, 255};
        for (int on = 0; on <= 1; ++on)
            for (int verified = 0; verified <= 1; ++verified)
                for (int current = 0; current <= 20; ++current)
                    for (int requested = 0; requested <= 20; ++requested)
                        for (int limit : limits)
                        {
                            bool const expected = on && verified &&
                                current >= 8 && current <= 18 &&
                                requested < 8 &&
                                (limit <= 0 || current <= limit);
                            CHECK(Blocks(on, verified, current, requested, limit) == expected);
                        }
    }

    void TestNamedOwnerCases()
    {
        // Default OFF: historical behaviour on every route.
        CHECK(!Blocks(false, true, 12, 0, 0));
        // Human BE/Draenei, Altbot, GM, excluded or unverified identity.
        CHECK(!Blocks(true, false, 12, 0, 0));
        // Saved stage 0-7 (Vanilla): never fabricated to 8, writes proceed.
        for (int s = 0; s < 8; ++s)
            CHECK(!Blocks(true, true, s, 0, 0));
        // Saved 8-18 with a Vanilla request: kept as is (not lowered to 8).
        for (int s = 8; s <= 18; ++s)
            CHECK(Blocks(true, true, s, 3, 0));
        // Same-era or higher requests are left to the established route
        // (e.g. TBC-band group sync 12 -> 8, or 8 -> 13).
        CHECK(!Blocks(true, true, 12, 8, 0));
        CHECK(!Blocks(true, true, 8, 13, 0));
        // ProgressionLimit 0 = unlimited.
        CHECK(Blocks(true, true, 18, 0, 0));
        // Positive limit below 8 must not be bypassed.
        CHECK(!Blocks(true, true, 8, 0, 7));
        CHECK(!Blocks(true, true, 8, 0, 1));
        // Positive limit >= current: protected.
        CHECK(Blocks(true, true, 8, 0, 8));
        CHECK(Blocks(true, true, 12, 0, 12));
        // Current above a positive limit: never preserve an over-cap stage.
        CHECK(!Blocks(true, true, 13, 0, 12));
        // Corrupt/unsupported stored stage: not protected.
        CHECK(!Blocks(true, true, 19, 0, 0));
        CHECK(!Blocks(true, true, 255, 0, 0));
        // Login level write (<=60 -> 0) for a verified bot at earned 8.
        CHECK(Blocks(true, true, 8, 0, 0));
        // Login level write 61-70 -> 8 and 71+ -> 13 are never refused.
        CHECK(!Blocks(true, true, 12, 8, 0));
        CHECK(!Blocks(true, true, 15, 13, 0));
    }
}

int main()
{
    TestExhaustiveTable();
    TestNamedOwnerCases();
    std::printf("F14 STAGE FLOOR POLICY %u/%u passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
