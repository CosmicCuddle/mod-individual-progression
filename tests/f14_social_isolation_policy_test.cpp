// Native tests for the pure F14 social isolation policy.
// GCC/Clang, C++17/C++20, with and without NDEBUG. Inputs are synthetic;
// nothing touches a Core object, packet or client.
#include "IndividualProgressionF14SocialIsolationPolicy.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace P = IndividualProgressionF14SocialIsolationPolicy;

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

static_assert(P::ClampCooldown(0) == P::MinReplyCooldownSeconds, "cooldown floor");
static_assert(P::ClampCooldown(999999) == P::MaxReplyCooldownSeconds, "cooldown ceiling");
static_assert(P::ClampCooldown(-5) == P::MinReplyCooldownSeconds, "negative cooldown");

namespace
{
    P::Human VanillaHuman(std::uint8_t stage = 3)
    {
        P::Human h;
        h.present = h.inWorld = true;
        h.nonHeadlessSession = true;
        h.playerAccountSecurity = true;
        h.gameMasterMode = false;
        h.verifiedRandomBot = false;
        h.rawStage = stage;
        return h;
    }

    P::Bot VerifiedBot(std::uint8_t race = P::BloodElfRace)
    {
        P::Bot b;
        b.present = b.inWorld = true;
        b.verifiedRandomBotIdentity = true;
        b.headlessSession = true;
        b.gameMasterMode = false;
        b.race = race;
        return b;
    }

    P::Surface const kSurfaces[] = {P::Surface::Whisper, P::Surface::Trade,
                                    P::Surface::GroupInvite, P::Surface::GroupAccept};

    void TestDefaultsNeverRestrict()
    {
        CHECK(!P::IsRestrictedPair(true, P::Human{}, P::Bot{}));
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), P::Bot{}));
        CHECK(!P::IsRestrictedPair(true, P::Human{}, VerifiedBot()));
        CHECK(!P::ExcludeFromWho(true, P::Human{}, true, P::BloodElfRace));
        // Option OFF: nothing changes anywhere.
        CHECK(!P::IsRestrictedPair(false, VanillaHuman(), VerifiedBot()));
        CHECK(!P::ExcludeFromWho(false, VanillaHuman(), true, P::DraeneiRace));
        for (P::Surface s : kSurfaces)
        {
            P::Decision const d = P::Decide(false, s, P::Direction::HumanToBot, VanillaHuman(), VerifiedBot(), false);
            CHECK(!d.block && !d.reply);
        }
    }

    void TestRestrictedPairAndReplies()
    {
        for (std::uint8_t race : {P::BloodElfRace, P::DraeneiRace})
        {
            CHECK(P::IsRestrictedPair(true, VanillaHuman(), VerifiedBot(race)));
            for (P::Surface s : kSurfaces)
            {
                P::Decision const h2b = P::Decide(true, s, P::Direction::HumanToBot, VanillaHuman(), VerifiedBot(race), false);
                CHECK(h2b.block && h2b.reply);
                P::Decision const b2h = P::Decide(true, s, P::Direction::BotToHuman, VanillaHuman(), VerifiedBot(race), false);
                CHECK(b2h.block && !b2h.reply); // bots never answer their own attempt
            }
        }
        // Addon whispers are blocked silently.
        P::Decision const addon = P::Decide(true, P::Surface::Whisper, P::Direction::HumanToBot,
                                            VanillaHuman(), VerifiedBot(), true);
        CHECK(addon.block && !addon.reply);
        // The addon flag only matters for whispers.
        P::Decision const trade = P::Decide(true, P::Surface::Trade, P::Direction::HumanToBot,
                                            VanillaHuman(), VerifiedBot(), true);
        CHECK(trade.block && trade.reply);
    }

    // Race alone is never enough, and every bot fact is required.
    void TestBotIdentityIsNeverRaceAlone()
    {
        unsigned restricted = 0;
        for (unsigned mask = 0; mask < (1u << 5); ++mask)
            for (int race = 0; race <= 12; ++race)
            {
                P::Bot b;
                b.present = mask & 1u;
                b.inWorld = mask & 2u;
                b.verifiedRandomBotIdentity = mask & 4u;
                b.headlessSession = mask & 8u;
                b.gameMasterMode = !(mask & 16u);
                b.race = static_cast<std::uint8_t>(race);
                bool const expected = mask == 31u && (race == 10 || race == 11);
                bool const got = P::IsRestrictedPair(true, VanillaHuman(), b);
                CHECK(got == expected);
                restricted += got;
            }
        CHECK(restricted == 2);

        // Named witnesses.
        P::Bot altbot = VerifiedBot();
        altbot.verifiedRandomBotIdentity = false; // Altbot: not IsRandomBot
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), altbot));
        P::Bot humanBloodElf = VerifiedBot();
        humanBloodElf.verifiedRandomBotIdentity = false;
        humanBloodElf.headlessSession = false;   // a real client
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), humanBloodElf));
        P::Bot selfBot = VerifiedBot();
        selfBot.headlessSession = false;         // human session with bot AI
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), selfBot));
        P::Bot gmBot = VerifiedBot();
        gmBot.gameMasterMode = true;
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), gmBot));
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), VerifiedBot(1))); // Human race bot
    }

    // Every human fact is required; GMs, bots and unknown stages never count.
    void TestHumanFacts()
    {
        unsigned restricted = 0;
        for (unsigned mask = 0; mask < (1u << 6); ++mask)
        {
            P::Human h = VanillaHuman();
            h.present = mask & 1u;
            h.inWorld = mask & 2u;
            h.nonHeadlessSession = mask & 4u;
            h.playerAccountSecurity = mask & 8u;
            h.gameMasterMode = !(mask & 16u);
            h.verifiedRandomBot = !(mask & 32u);
            bool const got = P::IsRestrictedPair(true, h, VerifiedBot());
            CHECK(got == (mask == 63u));
            CHECK(P::ExcludeFromWho(true, h, true, P::BloodElfRace) == (mask == 63u));
            restricted += got;
        }
        CHECK(restricted == 1);
    }

    // Stage boundary and immediate resume.
    void TestStageBoundaryAndResume()
    {
        for (int stage = 0; stage <= 255; ++stage)
        {
            P::Human const h = VanillaHuman(static_cast<std::uint8_t>(stage));
            bool const expected = stage < P::FirstTbcStage;
            CHECK(P::IsRestrictedPair(true, h, VerifiedBot()) == expected);
            CHECK(P::ExcludeFromWho(true, h, true, P::DraeneiRace) == expected);
            for (P::Surface s : kSurfaces)
                CHECK(P::Decide(true, s, P::Direction::HumanToBot, h, VerifiedBot(), false).block == expected);
        }
        // The same pair, recomputed after the human earns stage 8: allowed.
        CHECK(P::IsRestrictedPair(true, VanillaHuman(7), VerifiedBot()));
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(8), VerifiedBot()));
        // Unknown stage (out of world reader) never restricts.
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(255), VerifiedBot()));
    }

    void TestWho()
    {
        CHECK(P::ExcludeFromWho(true, VanillaHuman(), true, P::BloodElfRace));
        CHECK(P::ExcludeFromWho(true, VanillaHuman(), true, P::DraeneiRace));
        CHECK(!P::ExcludeFromWho(true, VanillaHuman(), false, P::BloodElfRace)); // race alone
        for (std::uint32_t race = 0; race < 20; ++race)
            CHECK(P::ExcludeFromWho(true, VanillaHuman(), true, race) == (race == 10 || race == 11));
        CHECK(!P::ExcludeFromWho(true, VanillaHuman(8), true, P::BloodElfRace));
    }

    // /who exclusion and the social block agree for every equivalent fact
    // set: a bot is left out of /who exactly when its social surfaces are
    // declined, so the list never advertises a bot the human cannot reach.
    void TestWhoMatchesSocialRestriction()
    {
        unsigned agree = 0;
        for (unsigned mask = 0; mask < (1u << 6); ++mask)
            for (int stage : {0, 7, 8, 18, 255})
                for (std::uint32_t race : {1u, 10u, 11u})
                    for (int verified = 0; verified <= 1; ++verified)
                    {
                        P::Human h = VanillaHuman(static_cast<std::uint8_t>(stage));
                        h.present = mask & 1u;
                        h.inWorld = mask & 2u;
                        h.nonHeadlessSession = mask & 4u;
                        h.playerAccountSecurity = mask & 8u;
                        h.gameMasterMode = !(mask & 16u);
                        h.verifiedRandomBot = !(mask & 32u);
                        P::Bot b = VerifiedBot(static_cast<std::uint8_t>(race));
                        b.verifiedRandomBotIdentity = verified != 0;
                        CHECK(P::ExcludeFromWho(true, h, verified != 0, race) == P::IsRestrictedPair(true, h, b));
                        ++agree;
                    }
        CHECK(agree == 64u * 5u * 3u * 2u);
    }

    // A human hammering a bot gets exactly one answer per cooldown, and the
    // bot's own answer can never trigger another (replies are never fed
    // back into the decision; a bot-initiated attempt never replies).
    void TestNoReplyLoops()
    {
        P::ReplyLimiter<4> l;
        unsigned replies = 0;
        for (std::uint64_t t = 1000; t < 1000 + 600; ++t)
        {
            P::Decision const d = P::Decide(true, P::Surface::Whisper, P::Direction::HumanToBot,
                                            VanillaHuman(), VerifiedBot(), false);
            if (d.reply && l.MayReply(7, 9, t, 60))
                ++replies;
            P::Decision const back = P::Decide(true, P::Surface::Whisper, P::Direction::BotToHuman,
                                               VanillaHuman(), VerifiedBot(), false);
            CHECK(back.block && !back.reply);
        }
        CHECK(replies == 10); // 600 s / 60 s
    }

    void TestReplyLimiter()
    {
        P::ReplyLimiter<3> l;
        // First reply per pair, then cooldown.
        CHECK(l.MayReply(1, 100, 1000, 60));
        CHECK(!l.MayReply(1, 100, 1030, 60));
        CHECK(!l.MayReply(1, 100, 1059, 60));
        CHECK(l.MayReply(1, 100, 1060, 60));
        // Other pairs are independent.
        CHECK(l.MayReply(2, 100, 1060, 60));
        CHECK(l.MayReply(1, 101, 1060, 60));
        CHECK(l.Suppressed() == 2);
        // Full table of still-cooling pairs: no reply for a new pair.
        CHECK(!l.MayReply(3, 102, 1061, 60));
        CHECK(l.FullRefusals() == 1);
        // Once an entry has cooled down, it is reused.
        CHECK(l.MayReply(3, 102, 1121, 60));
        // Clock moving backwards never replies; it restarts the cooldown.
        CHECK(!l.MayReply(3, 102, 1100, 60));
        CHECK(!l.MayReply(3, 102, 1159, 60));
        CHECK(l.MayReply(3, 102, 1160, 60));
        // Invalid keys never reply.
        CHECK(!l.MayReply(0, 102, 5000, 60));
        CHECK(!l.MayReply(3, 0, 5000, 60));
        // Cooldown is clamped to the minimum.
        P::ReplyLimiter<2> m;
        CHECK(m.MayReply(1, 1, 10, 0));
        CHECK(!m.MayReply(1, 1, 14, 0));
        CHECK(m.MayReply(1, 1, 15, 0));
    }

    // A long sequence: per pair, replies are never closer than the cooldown.
    void TestLimiterSpacingRandomised()
    {
        P::ReplyLimiter<8> l;
        std::uint64_t last[4][4] = {};
        bool seen[4][4] = {};
        std::uint64_t now = 100;
        std::uint64_t s = 0x9e3779b97f4a7c15ULL;
        for (int i = 0; i < 100000; ++i)
        {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            now += s % 7;
            unsigned const h = static_cast<unsigned>(s >> 8) % 4, b = static_cast<unsigned>(s >> 16) % 4;
            if (l.MayReply(h + 1, b + 1, now, 30))
            {
                if (seen[h][b])
                    CHECK(now - last[h][b] >= 30);
                seen[h][b] = true;
                last[h][b] = now;
            }
        }
        CHECK(seen[0][0] && seen[3][3]);
    }

    bool ValidUtf8(std::string const& t)
    {
        std::size_t i = 0;
        while (i < t.size())
        {
            unsigned char const c = static_cast<unsigned char>(t[i]);
            std::size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
            if (!n || i + n > t.size())
                return false;
            for (std::size_t k = 1; k < n; ++k)
                if ((static_cast<unsigned char>(t[i + k]) & 0xC0) != 0x80)
                    return false;
            i += n;
        }
        return true;
    }

    bool OneLineNoControl(std::string const& t)
    {
        for (char c : t)
        {
            unsigned char const u = static_cast<unsigned char>(c);
            if (u < 0x20 || u == 0x7F)
                return false;
        }
        return true;
    }

    void TestReplyNormalization()
    {
        // Default stays exactly as configured.
        CHECK(P::NormalizeReply(P::DefaultReply) == std::string(P::DefaultReply));
        // Empty / whitespace / control-only => decline silently.
        CHECK(P::NormalizeReply("").empty());
        CHECK(P::NormalizeReply("   ").empty());
        CHECK(P::NormalizeReply("\r\n\t").empty());
        CHECK(P::NormalizeReply(std::string("\0\0", 2)).empty());
        // CR/LF/NUL/tab/DEL/other C0 become spaces; one line; trimmed.
        CHECK(P::NormalizeReply("Line one\r\nLine two") == "Line one  Line two");
        CHECK(P::NormalizeReply(std::string("a\0b", 3)) == "a b");
        CHECK(P::NormalizeReply("\n  hello\t") == "hello");
        CHECK(P::NormalizeReply("x\x7Fy\x1Bz") == "x y z");
        for (int c = 0; c < 256; ++c)
        {
            std::string const out = P::NormalizeReply(std::string("a") + static_cast<char>(c) + "b");
            CHECK(OneLineNoControl(out));
            CHECK(out.size() == 3);
        }
        // Oversized ASCII is cut to exactly the cap.
        std::string const big(5000, 'w');
        CHECK(P::NormalizeReply(big).size() == P::MaxReplyBytes);
        CHECK(P::NormalizeReply(std::string(P::MaxReplyBytes, 'w')).size() == P::MaxReplyBytes);
        CHECK(P::NormalizeReply(std::string(P::MaxReplyBytes + 1, 'w')).size() == P::MaxReplyBytes);
        // Multi-byte text is never cut mid-character, at every offset.
        char const* glyphs[] = {"\xC3\xA9", "\xE2\x82\xAC", "\xF0\x9F\x8C\x99"}; // 2, 3, 4 bytes
        for (char const* g : glyphs)
            for (std::size_t pad = 0; pad < 4; ++pad)
            {
                std::string t(pad, 'p');
                while (t.size() < 1200)
                    t += g;
                std::string const out = P::NormalizeReply(t);
                CHECK(out.size() <= P::MaxReplyBytes);
                CHECK(out.size() + 4 > P::MaxReplyBytes);
                CHECK(ValidUtf8(out));
                CHECK(t.compare(0, out.size(), out) == 0); // a true prefix
            }
        // Randomised: always one line, bounded, valid UTF-8 for UTF-8 input.
        std::uint64_t s = 0x2545F4914F6CDD1DULL;
        for (int i = 0; i < 20000; ++i)
        {
            std::string t;
            int const len = static_cast<int>(s % 900);
            for (int k = 0; k < len; ++k)
            {
                s ^= s << 13; s ^= s >> 7; s ^= s << 17;
                switch (s % 6)
                {
                    case 0: t += '\n'; break;
                    case 1: t += '\r'; break;
                    case 2: t.push_back('\0'); break;
                    case 3: t += "\xE2\x82\xAC"; break;
                    default: t += static_cast<char>('a' + s % 26); break;
                }
            }
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            std::string const out = P::NormalizeReply(t);
            CHECK(out.size() <= P::MaxReplyBytes);
            CHECK(OneLineNoControl(out));
            CHECK(ValidUtf8(out));
            CHECK(out.empty() || (out.front() != ' ' && out.back() != ' '));
        }
    }

    // Backlog item 4: lifecycle scenarios that reduce to pure decisions.
    // Runtime ordering (login hooks, map transfer, Core threads) is NOT
    // exercised here; see docs/F14_SOCIAL_ISOLATION_2026-11-13.md.
    void TestLifecycleScenarios()
    {
        // Identity appearing late (verified only after a later login): the
        // pair is unrestricted until the verified fact exists, then restricted.
        P::Bot late = VerifiedBot();
        late.verifiedRandomBotIdentity = false;
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), late));
        late.verifiedRandomBotIdentity = true;
        CHECK(P::IsRestrictedPair(true, VanillaHuman(), late));
        // Identity disappearing (logout/revoke): unknown => normal interaction.
        late.verifiedRandomBotIdentity = false;
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), late));
        CHECK(!P::ExcludeFromWho(true, VanillaHuman(), false, P::BloodElfRace));

        // Bot leaves the world mid-attempt (map transfer/logout): not restricted.
        P::Bot transferring = VerifiedBot();
        transferring.inWorld = false;
        CHECK(!P::IsRestrictedPair(true, VanillaHuman(), transferring));
        P::Human relogging = VanillaHuman();
        relogging.inWorld = false; // the stage reader also reports 255 here
        relogging.rawStage = 255;
        CHECK(!P::IsRestrictedPair(true, relogging, VerifiedBot()));

        // Progression changing while online is re-read on every attempt,
        // including an unusual downward correction.
        int const path[] = {0, 7, 8, 12, 13, 7, 3, 8};
        for (int stage : path)
            CHECK(P::IsRestrictedPair(true, VanillaHuman(static_cast<std::uint8_t>(stage)), VerifiedBot()) ==
                  (stage < P::FirstTbcStage));

        // Ordinary-race verified bots are never isolated, at any human stage.
        for (int race = 1; race <= 8; ++race)
            for (int stage = 0; stage <= 18; ++stage)
                CHECK(!P::IsRestrictedPair(true, VanillaHuman(static_cast<std::uint8_t>(stage)),
                                           VerifiedBot(static_cast<std::uint8_t>(race))));
    }

    // Many humans and many bots: cooldowns are strictly per (human, bot).
    void TestMultiViewerCooldowns()
    {
        P::ReplyLimiter<16> l;
        for (std::uint64_t human = 1; human <= 3; ++human)
            for (std::uint64_t bot = 100; bot <= 102; ++bot)
                CHECK(l.MayReply(human, bot, 1000, 60)); // 9 independent first replies
        for (std::uint64_t human = 1; human <= 3; ++human)
            for (std::uint64_t bot = 100; bot <= 102; ++bot)
                CHECK(!l.MayReply(human, bot, 1001, 60)); // each pair cools down alone
        CHECK(l.MayReply(4, 100, 1001, 60)); // a new human is unaffected

        // Logout/relog keeps the raw-GUID pair cooldown: a quick relog does
        // not reset it, and it expires normally.
        CHECK(!l.MayReply(1, 100, 1030, 60));
        CHECK(l.MayReply(1, 100, 1060, 60));

        // Saturation affects only new pairs, and recovers as entries cool.
        P::ReplyLimiter<2> tiny;
        CHECK(tiny.MayReply(1, 100, 10, 60));
        CHECK(tiny.MayReply(2, 100, 10, 60));
        CHECK(!tiny.MayReply(3, 100, 11, 60));
        CHECK(!tiny.MayReply(1, 100, 12, 60)); // existing pair still cooling
        CHECK(tiny.MayReply(3, 100, 70, 60));  // a cooled slot is reused
        CHECK(tiny.FullRefusals() == 1);
    }

    void TestDefaultReplyIsInCharacter()
    {
        char const* text = P::DefaultReply;
        CHECK(std::strlen(text) > 20 && std::strlen(text) < 255);
        char const* banned[] = {"tier", "Tier", "stage", "Stage", "server", "Server", "expansion",
                                "Expansion", "TBC", "Vanilla", "level", "Level", "progression"};
        for (char const* word : banned)
            CHECK(std::strstr(text, word) == nullptr);
    }
}

int main()
{
    TestDefaultsNeverRestrict();
    TestRestrictedPairAndReplies();
    TestBotIdentityIsNeverRaceAlone();
    TestHumanFacts();
    TestStageBoundaryAndResume();
    TestWho();
    TestWhoMatchesSocialRestriction();
    TestNoReplyLoops();
    TestReplyLimiter();
    TestLimiterSpacingRandomised();
    TestDefaultReplyIsInCharacter();
    TestReplyNormalization();
    TestLifecycleScenarios();
    TestMultiViewerCooldowns();
    std::printf("F14 SOCIAL ISOLATION POLICY %u/%u passed; NO WORLD HIDING\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
