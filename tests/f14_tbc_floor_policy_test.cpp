// Native tests for IndividualProgressionF14TbcFloorPolicy (F14 verified
// RandomBot TBC floor). Pure header; no AzerothCore needed.
#include "IndividualProgressionF14TbcFloorPolicy.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace P = IndividualProgressionF14TbcFloorPolicy;

namespace
{
    unsigned g_pass = 0;
    unsigned g_fail = 0;

#define CHECK(expr)                                                              \
    do                                                                           \
    {                                                                            \
        if (expr)                                                                \
            ++g_pass;                                                            \
        else                                                                     \
        {                                                                        \
            ++g_fail;                                                            \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);          \
        }                                                                        \
    } while (0)

    // A model of the module's write path for one bot: the login write, the
    // clamp inside ForceUpdateProgressionState, the Playerbots re-roll and
    // the world-update repair. `floored` is FloorApplies(...) for that bot.
    struct Bot
    {
        bool floored;
        std::uint32_t level;
        std::uint8_t stage;
    };

    std::uint8_t LegacyLevelWrite(std::uint32_t level) { return P::LevelStage(level); }

    void Force(Bot& b, std::uint8_t requested)
    {
        b.stage = b.floored ? P::ClampWrite(requested) : requested;
    }

    void Login(Bot& b, std::int32_t limit)
    {
        if (b.floored)
        {
            std::uint8_t const target = P::TargetStage(b.stage, b.level, limit);
            if (P::NeedsRepair(b.stage, target))
                Force(b, target);
            return;
        }
        Force(b, LegacyLevelWrite(b.level)); // historical non-normal login write
    }

    void PlayerbotsReroll(Bot& b) { b.stage = 0; } // ResetQuests removes 66001-66018

    void WorldRepair(Bot& b, std::int32_t limit)
    {
        if (!b.floored)
            return;
        std::uint8_t const target = P::TargetStage(b.stage, b.level, limit);
        if (P::NeedsRepair(b.stage, target))
            Force(b, target);
    }

    void TestLevelStageAndLimits()
    {
        CHECK(P::LevelStage(1) == 0 && P::LevelStage(60) == 0);
        CHECK(P::LevelStage(61) == 8 && P::LevelStage(70) == 8);
        CHECK(P::LevelStage(71) == 13 && P::LevelStage(80) == 13);
        for (std::int32_t limit = -3; limit <= 25; ++limit)
            CHECK(P::LimitAllowsFloor(limit) == (limit == 0 || (limit >= 8 && limit <= 18)));
        for (int o = 0; o < 2; ++o)
            for (int i = 0; i < 2; ++i)
                for (std::int32_t limit = -1; limit <= 19; ++limit)
                    CHECK(P::FloorApplies(o, i, limit) == (o && i && P::LimitAllowsFloor(limit)));
    }

    void TestTargetStageExhaustive()
    {
        for (std::uint32_t level = 1; level <= 83; ++level)
            for (int saved = 0; saved <= 25; ++saved)
                for (std::int32_t limit = 0; limit <= 18; ++limit)
                {
                    if (!P::LimitAllowsFloor(limit))
                        continue;
                    std::uint8_t const s = static_cast<std::uint8_t>(saved);
                    std::uint8_t const t = P::TargetStage(s, level, limit);
                    if (saved > 18)
                    {
                        CHECK(t == s); // corrupt: unchanged, no write
                        CHECK(!P::NeedsRepair(s, t));
                        continue;
                    }
                    CHECK(t >= s);                                   // never lowers
                    CHECK(t >= 8);                                   // never Vanilla
                    std::uint8_t required = P::LevelStage(level) >= 8 ? P::LevelStage(level) : 8;
                    if (limit > 0 && required > limit)
                        required = static_cast<std::uint8_t>(limit);
                    CHECK(t == (s >= required ? s : required));
                    if (limit > 0 && s <= limit)
                        CHECK(t <= limit);                           // a new grant never exceeds a positive limit
                    CHECK(P::NeedsRepair(s, t) == (s < t));
                }
    }

    void TestClampWrite()
    {
        for (int r = 0; r <= 18; ++r)
            CHECK(P::ClampWrite(static_cast<std::uint8_t>(r)) == (r < 8 ? 8 : r));
    }

    void TestScenarios()
    {
        // Stage 0 -> 8 repair at login (level 60, the default population).
        {
            Bot b{true, 60, 0};
            Login(b, 0);
            CHECK(b.stage == 8);
        }
        // Saved TBC/Wrath preserved at login.
        for (std::uint8_t saved : {8, 10, 12, 13, 16, 18})
        {
            Bot b{true, 60, saved};
            Login(b, 0);
            CHECK(b.stage == saved);
        }
        // Level 71+ -> Wrath required.
        {
            Bot b{true, 75, 0};
            Login(b, 0);
            CHECK(b.stage == 13);
            Bot c{true, 75, 9};
            Login(c, 0);
            CHECK(c.stage == 13);
        }
        // Wrath -> TBC fallback via a Vanilla-leader group sync, never Vanilla.
        {
            Bot b{true, 60, 13};
            Force(b, 0);
            CHECK(b.stage == 8);
            Force(b, 5);
            CHECK(b.stage == 8);
            Force(b, 13);       // upward sync allowed
            CHECK(b.stage == 13);
        }
        // Playerbots re-roll removes the markers; the world repair restores.
        {
            Bot b{true, 60, 13};
            PlayerbotsReroll(b);
            CHECK(b.stage == 0);
            WorldRepair(b, 0);
            CHECK(b.stage == 8);     // markers lost: back to the floor, not Vanilla
            WorldRepair(b, 0);
            CHECK(b.stage == 8);     // idempotent
        }
        // Negatives: Altbot / human / SelfBot / unknown are never floored.
        for (std::uint32_t level : {20u, 60u, 65u, 75u})
        {
            Bot b{false, level, 0};
            Login(b, 0);
            CHECK(b.stage == P::LevelStage(level)); // historical behaviour
            Force(b, 0);
            CHECK(b.stage == 0);
            PlayerbotsReroll(b);
            WorldRepair(b, 0);
            CHECK(b.stage == 0);
        }
        // ProgressionLimit: a positive limit >= 8 caps the target.
        {
            Bot b{true, 75, 0};
            Login(b, 10);
            CHECK(b.stage == 10);
            Bot c{true, 60, 0};
            Login(c, 8);
            CHECK(c.stage == 8);
            Bot d{true, 60, 15};    // historic over-cap stage kept untouched
            Login(d, 10);
            CHECK(d.stage == 15);
        }
        // A Vanilla-only limit (< 8) makes FloorApplies false: the caller
        // treats the bot as not floored, i.e. historical behaviour.
        for (std::int32_t limit = 1; limit < 8; ++limit)
        {
            CHECK(!P::FloorApplies(true, true, limit));
            Bot b{P::FloorApplies(true, true, limit), 60, 0};
            Login(b, limit);
            CHECK(b.stage == 0);
        }
        // OFF: nobody is floored.
        {
            Bot b{P::FloorApplies(false, true, 0), 60, 0};
            Login(b, 0);
            CHECK(b.stage == 0);
        }
    }
}

int main()
{
    TestLevelStageAndLimits();
    TestTargetStageExhaustive();
    TestClampWrite();
    TestScenarios();
    std::printf("F14 TBC FLOOR POLICY %u/%u passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
