// Native tests for the pure F14 social isolation policy.
// GCC/Clang, C++17/C++20, with and without NDEBUG. Inputs are synthetic;
// nothing touches a Core object, packet or client.
#include "IndividualProgressionF14SocialIsolationPolicy.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <algorithm>
#include <atomic>
#include <vector>

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
                                    P::Surface::GroupInvite, P::Surface::GroupAccept,
                                    P::Surface::Duel, P::Surface::Mail, P::Surface::GuildInvite};

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

namespace
{
    // Group accept: refused when ANY member is a restricted RandomBot.
    void TestGroupAcceptMemberScan()
    {
        P::Bot const restricted = VerifiedBot(P::BloodElfRace);
        P::Bot const draenei = VerifiedBot(P::DraeneiRace);
        P::Bot const ordinary = VerifiedBot(1);  // verified RandomBot, Human race
        P::Bot humanBe = VerifiedBot();          // real BE player
        humanBe.verifiedRandomBotIdentity = false;
        humanBe.headlessSession = false;
        P::Bot altbot = VerifiedBot();           // not IsRandomBot
        altbot.verifiedRandomBotIdentity = false;
        P::Bot selfBot = VerifiedBot();          // real client with bot AI
        selfBot.headlessSession = false;
        P::Bot gm = VerifiedBot();
        gm.gameMasterMode = true;
        P::Bot const unresolved{};               // offline/unresolved: no facts
        P::Bot raceOnly{};                       // race known, nothing verified
        raceOnly.race = P::BloodElfRace;
        P::Bot offline = VerifiedBot();          // verified once, now out of world
        offline.inWorld = false;

        using Group = std::vector<P::Bot>;
        P::Human const v = VanillaHuman();

        // Unrestricted leader (index 0) + restricted member.
        CHECK(P::FirstRestrictedGroupMember(true, v, Group{ordinary, restricted}) == 1);
        // Restricted leader + unrestricted members.
        CHECK(P::FirstRestrictedGroupMember(true, v, Group{draenei, ordinary, humanBe}) == 0);
        // No restricted member: allow.
        Group const clean{ordinary, humanBe, altbot, selfBot, gm, unresolved, raceOnly, offline};
        CHECK(P::FirstRestrictedGroupMember(true, v, clean) == clean.size());
        CHECK(P::FirstRestrictedGroupMember(true, v, Group{}) == 0); // empty = size 0 = allow
        // Each never-restricted kind on its own.
        for (P::Bot const& b : clean)
            CHECK(P::FirstRestrictedGroupMember(true, v, Group{b}) == 1);
        // Eligible (TBC/Wrath), unknown, GM and bot "humans": allow.
        for (std::uint8_t stage : {8, 12, 13, 18, 255})
            CHECK(P::FirstRestrictedGroupMember(true, VanillaHuman(stage), Group{ordinary, restricted}) == 2);
        P::Human gmHuman = v;
        gmHuman.gameMasterMode = true;
        CHECK(P::FirstRestrictedGroupMember(true, gmHuman, Group{restricted}) == 1);
        P::Human botHuman = v;
        botHuman.verifiedRandomBot = true;
        CHECK(P::FirstRestrictedGroupMember(true, botHuman, Group{restricted}) == 1);
        // Option OFF: allow.
        CHECK(P::FirstRestrictedGroupMember(false, v, Group{restricted, draenei}) == 2);

        // Restricted bot neither first nor last, for every size up to a raid.
        for (std::size_t size = 3; size <= 40; ++size)
            for (std::size_t at = 1; at + 1 < size; ++at)
            {
                Group g(size, ordinary);
                g[at] = (size + at) % 2 ? restricted : draenei;
                g[size - 1] = restricted; // a later one must not win
                CHECK(P::FirstRestrictedGroupMember(true, v, g) == at);
            }

        // Exhaustive: groups of 0-5 members over 8 member kinds, every stage.
        P::Bot const kinds[] = {restricted, draenei, ordinary, humanBe, altbot, selfBot, unresolved, raceOnly};
        bool const kindRestricted[] = {true, true, false, false, false, false, false, false};
        for (unsigned stage = 0; stage <= 19; ++stage)
        {
            P::Human const h = VanillaHuman(static_cast<std::uint8_t>(stage == 19 ? 255 : stage));
            bool const vanilla = stage < 8;
            for (std::size_t size = 0; size <= 5; ++size)
            {
                std::size_t combos = 1;
                for (std::size_t i = 0; i < size; ++i)
                    combos *= 8;
                for (std::size_t code = 0; code < combos; ++code)
                {
                    Group g;
                    std::size_t expected = size;
                    std::size_t c = code;
                    for (std::size_t i = 0; i < size; ++i, c /= 8)
                    {
                        g.push_back(kinds[c % 8]);
                        if (expected == size && vanilla && kindRestricted[c % 8])
                            expected = i;
                    }
                    CHECK(P::FirstRestrictedGroupMember(true, h, g) == expected);
                    CHECK(P::FirstRestrictedGroupMember(false, h, g) == size);
                }
            }
        }

        // The member that blocks gets the same decision and reply rule.
        P::Decision const d = P::Decide(true, P::Surface::GroupAccept, P::Direction::HumanToBot, v, restricted, false);
        CHECK(d.block && d.reply);
    }
}

namespace
{
    // T1: off the world thread a pair is read only when both pointers are in
    // the map this thread owns in the current tick; otherwise nothing is
    // read and the pair is not restricted (owner decision: allow).
    void TestThreadSafeReads()
    {
        // Owned-scope validity: recorded in this very tick only.
        CHECK(P::OwnedScopeValid(true, 7, 7));
        CHECK(!P::OwnedScopeValid(true, 6, 7));   // a previous tick: stale
        CHECK(!P::OwnedScopeValid(true, 8, 7));
        CHECK(!P::OwnedScopeValid(false, 7, 7));  // never recorded

        // Full truth table of the read rule.
        for (int world = 0; world < 2; ++world)
            for (int valid = 0; valid < 2; ++valid)
                for (int first = 0; first < 2; ++first)
                    for (int second = 0; second < 2; ++second)
                    {
                        bool const expected = world || (valid && first && second);
                        CHECK(P::MayReadPair(world != 0, valid != 0, first != 0, second != 0) == expected);
                    }
        CHECK(P::MayReadPair(true, false, false, false));  // client packet: world thread
        CHECK(P::MayReadPair(false, true, true, true));    // bot AI, both on the owned map
        CHECK(!P::MayReadPair(false, true, true, false));  // other player on another map
        CHECK(!P::MayReadPair(false, true, false, true));  // initiator not ours (e.g. LFG invite)
        CHECK(!P::MayReadPair(false, false, true, true));  // stale or missing record

        // Composition model: facts are read and decided only when allowed;
        // otherwise the pair is allowed with no reply.
        for (int world = 0; world < 2; ++world)
            for (int valid = 0; valid < 2; ++valid)
                for (int first = 0; first < 2; ++first)
                    for (int second = 0; second < 2; ++second)
                        for (P::Surface s : kSurfaces)
                            for (int dir = 0; dir < 2; ++dir)
                            {
                                bool const readable = P::MayReadPair(world != 0, valid != 0, first != 0, second != 0);
                                P::Direction const d = dir ? P::Direction::BotToHuman : P::Direction::HumanToBot;
                                P::Decision const decided = readable
                                    ? P::Decide(true, s, d, VanillaHuman(), VerifiedBot(), false)
                                    : P::Decision{};
                                CHECK(decided.block == readable);
                                CHECK(decided.reply == (readable && dir == 0));
                            }

        // The marker: unmarked is never the world thread; only the marking
        // thread is; a re-mark from another thread moves it.
        P::WorldThreadMarker marker;
        CHECK(!marker.IsCurrent());
        bool otherSeesUnmarked = true;
        std::thread([&] { otherSeesUnmarked = !marker.IsCurrent(); }).join();
        CHECK(otherSeesUnmarked);
        marker.Mark();
        CHECK(marker.IsCurrent());
        bool otherSeesWorld = true;
        std::thread([&] { otherSeesWorld = marker.IsCurrent(); }).join();
        CHECK(!otherSeesWorld);
        bool remarkedThere = false;
        std::thread([&] { marker.Mark(); remarkedThere = marker.IsCurrent(); }).join();
        CHECK(remarkedThere);
        CHECK(!marker.IsCurrent());
        marker.Mark();
        CHECK(marker.IsCurrent());

        // Concurrent readers on many threads never see themselves as the
        // world thread while the main thread keeps marking.
        std::atomic<unsigned> falseWorld{0};
        std::vector<std::thread> readers;
        for (int i = 0; i < 4; ++i)
            readers.emplace_back([&] {
                for (int n = 0; n < 20000; ++n)
                    if (marker.IsCurrent())
                        ++falseWorld;
            });
        for (int n = 0; n < 20000; ++n)
            marker.Mark(); // the world thread keeps marking meanwhile
        for (std::thread& r : readers)
            r.join();
        CHECK(falseWorld.load() == 0);
        CHECK(marker.IsCurrent());

        // World ticks: every mark advances the tick by one, from any thread;
        // a record from an earlier tick is never valid again.
        P::WorldThreadMarker ticks;
        CHECK(ticks.Tick() == 0);
        std::uint64_t const recorded = ticks.Tick();
        ticks.Mark();
        CHECK(ticks.Tick() == 1);
        CHECK(!P::OwnedScopeValid(true, recorded, ticks.Tick()));
        std::vector<std::thread> markers;
        for (int i = 0; i < 4; ++i)
            markers.emplace_back([&] { for (int n = 0; n < 1000; ++n) ticks.Mark(); });
        for (std::thread& m : markers)
            m.join();
        CHECK(ticks.Tick() == 4001);
    }
}

namespace
{
    // Fake owned-map types: every accessor call is counted, so the tests
    // can prove which objects the T1 helpers touch.
    struct FakePlayer
    {
        std::uint64_t guid = 0;
        bool inWorld = true;
        mutable unsigned touched = 0;
        std::uint64_t GetGUID() const { ++touched; return guid; }
        bool IsInWorld() const { ++touched; return inWorld; }
    };

    struct FakeRef
    {
        FakePlayer* player = nullptr;
        FakePlayer* GetSource() const { return player; }
    };

    void TestOwnedMapHelpers()
    {
        FakePlayer a{1}, b{2}, c{3}, offWorld{4, false}, foreign{5}, foreignSameGuid{2};
        std::vector<FakeRef> owned{{&a}, {&b}, {nullptr}, {&c}, {&offWorld}};

        // Membership: pointer identity only; NOTHING is touched.
        CHECK(P::OwnedListContains(owned, &a));
        CHECK(P::OwnedListContains(owned, &c));
        CHECK(!P::OwnedListContains(owned, &foreign));
        CHECK(!P::OwnedListContains(owned, &foreignSameGuid)); // same GUID, other object
        CHECK(!P::OwnedListContains(owned, static_cast<FakePlayer const*>(nullptr)));
        CHECK(!P::OwnedListContains(std::vector<FakeRef>{}, &a));
        CHECK(a.touched == 0 && b.touched == 0 && c.touched == 0 && offWorld.touched == 0);
        CHECK(foreign.touched == 0 && foreignSameGuid.touched == 0);

        // Resolve: only owned players are read; foreign objects never.
        CHECK(P::OwnedListResolve<FakePlayer>(owned, std::uint64_t{2}) == &b);
        CHECK(P::OwnedListResolve<FakePlayer>(owned, std::uint64_t{3}) == &c);
        CHECK(P::OwnedListResolve<FakePlayer>(owned, std::uint64_t{5}) == nullptr); // foreign GUID
        CHECK(P::OwnedListResolve<FakePlayer>(owned, std::uint64_t{4}) == nullptr); // out of world
        CHECK(P::OwnedListResolve<FakePlayer>(std::vector<FakeRef>{}, std::uint64_t{1}) == nullptr);
        CHECK(foreign.touched == 0 && foreignSameGuid.touched == 0);
        CHECK(a.touched > 0); // owned players may be read: this thread owns them

        // Composition: a pair is readable off the world thread only when
        // both pointers are owned; a foreign object is never touched.
        unsigned const before = foreign.touched;
        for (FakePlayer* x : {&a, &b, &c, &foreign, &foreignSameGuid})
            for (FakePlayer* y : {&a, &b, &c, &foreign, &foreignSameGuid})
            {
                bool const xOwned = P::OwnedListContains(owned, x);
                bool const yOwned = P::OwnedListContains(owned, y);
                bool const expected = (x == &a || x == &b || x == &c) && (y == &a || y == &b || y == &c);
                CHECK(P::MayReadPair(false, true, xOwned, yOwned) == expected);
                CHECK(!P::MayReadPair(false, false, xOwned, yOwned)); // no valid record
                CHECK(P::MayReadPair(true, false, xOwned, yOwned));   // world thread
            }
        CHECK(foreign.touched == before && foreignSameGuid.touched == 0);
    }
}

namespace
{
    // Dungeon Finder (owner decision): a still-Vanilla human is never
    // matched into one group with a restricted RandomBot; TBC/Wrath humans
    // keep stock matching; unknown GUIDs never restrict.
    void TestDungeonFinderQueue()
    {
        P::Bot const notABot{};
        P::Human const notAHuman{};
        P::QueueFacts const vanilla = P::MakeQueueFacts(VanillaHuman(3), notABot);
        P::QueueFacts const tbc = P::MakeQueueFacts(VanillaHuman(8), notABot);
        P::QueueFacts const wrath = P::MakeQueueFacts(VanillaHuman(13), notABot);
        P::QueueFacts const restricted = P::MakeQueueFacts(notAHuman, VerifiedBot(P::BloodElfRace));
        P::QueueFacts const draenei = P::MakeQueueFacts(notAHuman, VerifiedBot(P::DraeneiRace));
        P::QueueFacts const ordinaryBot = P::MakeQueueFacts(notAHuman, VerifiedBot(1));
        P::Bot altbot = VerifiedBot();
        altbot.verifiedRandomBotIdentity = false;
        P::QueueFacts const altbotFacts = P::MakeQueueFacts(notAHuman, altbot);
        P::Bot selfBot = VerifiedBot();
        selfBot.headlessSession = false;
        P::QueueFacts const selfBotFacts = P::MakeQueueFacts(notAHuman, selfBot);

        CHECK(vanilla.stillVanillaHuman && !vanilla.restrictedBot);
        CHECK(!tbc.stillVanillaHuman && !wrath.stillVanillaHuman);
        CHECK(restricted.restrictedBot && draenei.restrictedBot && !restricted.stillVanillaHuman);
        CHECK(!ordinaryBot.restrictedBot && !altbotFacts.restrictedBot && !selfBotFacts.restrictedBot);

        // The pure rule.
        for (int opt = 0; opt < 2; ++opt)
            for (int v = 0; v < 2; ++v)
                for (int r = 0; r < 2; ++r)
                    CHECK(P::QueueGroupRejected(opt != 0, v != 0, r != 0) == (opt && v && r));

        P::QueueFactsTable table;
        table.Record(1, vanilla);
        table.Record(2, tbc);
        table.Record(3, wrath);
        table.Record(10, restricted);
        table.Record(11, draenei);
        table.Record(20, ordinaryBot);
        table.Record(21, altbotFacts);
        table.Record(22, selfBotFacts);
        table.Record(0, restricted); // a zero GUID is never recorded
        CHECK(table.Size() == 8);

        using G = std::vector<std::uint64_t>;
        CHECK(table.Rejects(true, G{1, 10}));                 // Vanilla human + BE bot
        CHECK(table.Rejects(true, G{20, 21, 1, 22, 11}));     // restricted bot anywhere
        CHECK(!table.Rejects(true, G{2, 10, 11}));            // TBC human: stock
        CHECK(!table.Rejects(true, G{3, 10}));                // Wrath human: stock
        CHECK(!table.Rejects(true, G{1, 20, 21, 22}));        // Vanilla + never-restricted kinds
        CHECK(!table.Rejects(true, G{1, 99}));                // unknown GUID never restricts
        CHECK(!table.Rejects(true, G{99, 98, 10}));
        CHECK(!table.Rejects(false, G{1, 10}));               // option OFF
        CHECK(!table.Rejects(true, G{}));
        CHECK(table.Rejects(true, G{2, 1, 10}));              // mixed humans: one Vanilla is enough

        // Re-queue refreshes facts (reaching TBC lifts it); logout erases.
        table.Record(1, tbc);
        CHECK(!table.Rejects(true, G{1, 10}));
        table.Record(1, vanilla);
        CHECK(table.Rejects(true, G{1, 10}));
        table.Erase(1);
        CHECK(!table.Rejects(true, G{1, 10}));
        table.Erase(12345); // erasing an unknown GUID is harmless
        CHECK(table.Size() == 7);

        // Exhaustive: groups of up to 5 drawn from 8 kinds, every subset.
        std::uint64_t const ids[] = {1, 2, 3, 10, 11, 20, 21, 22};
        table.Record(1, vanilla);
        // Parties: a FULL proposal names a party only by its group GUID (no
        // roles map). Members recorded at join are expanded.
        {
            P::QueueFactsTable parties;
            std::uint64_t const vanillaParty = 1000, tbcParty = 1001, botParty = 1002, unknownParty = 1003;
            parties.Record(501, vanilla);
            parties.Record(502, tbc);
            parties.Record(503, wrath);
            parties.Record(504, restricted);
            parties.Record(505, ordinaryBot);
            parties.RecordGroup(vanillaParty, {501, 502});   // Vanilla human queued with a TBC friend
            parties.RecordGroup(tbcParty, {502, 503});
            parties.RecordGroup(botParty, {505, 504});       // a party carrying a restricted bot
            parties.RecordGroup(0, {501});                   // no group GUID: ignored
            CHECK(parties.Groups() == 3);
            CHECK(parties.Rejects(true, G{vanillaParty, 504}));   // the gap: party GUID + solo bot
            CHECK(parties.Rejects(true, G{501, botParty}));       // solo Vanilla + bot party
            CHECK(parties.Rejects(true, G{vanillaParty, botParty}));
            CHECK(!parties.Rejects(true, G{tbcParty, 504}));      // TBC/Wrath party: stock
            CHECK(!parties.Rejects(true, G{tbcParty, botParty}));
            CHECK(!parties.Rejects(true, G{vanillaParty, 505}));  // ordinary bot: stock
            CHECK(!parties.Rejects(true, G{unknownParty, 504}));  // unrecorded party: no facts
            CHECK(!parties.Rejects(false, G{vanillaParty, 504})); // option OFF
            // Both views of the same proposal agree (roles keys + party GUID).
            CHECK(parties.Rejects(true, G{501, 502, vanillaParty, 504}));
            // A member logs out: their facts and every party listing them go.
            parties.Erase(501);
            CHECK(parties.Groups() == 2);
            CHECK(!parties.Rejects(true, G{vanillaParty, 504}));
            parties.Erase(504);
            CHECK(parties.Groups() == 1);
            CHECK(!parties.Rejects(true, G{501, botParty}));
            // Re-queue as a party refreshes its member list.
            parties.Record(501, vanilla);
            parties.RecordGroup(vanillaParty, {501});
            parties.Record(504, restricted);
            CHECK(parties.Rejects(true, G{vanillaParty, 504}));
        }

        for (unsigned mask = 0; mask < 256; ++mask)
        {
            G group;
            bool vanillaIn = false, botIn = false;
            for (unsigned i = 0; i < 8; ++i)
                if (mask & (1u << i))
                {
                    group.push_back(ids[i]);
                    vanillaIn = vanillaIn || ids[i] == 1;
                    botIn = botIn || ids[i] == 10 || ids[i] == 11;
                }
            CHECK(table.Rejects(true, group) == (vanillaIn && botIn));
            CHECK(!table.Rejects(false, group));
        }
    }
}

namespace
{
    // Battlegrounds, owner option (c): (b) queue-join and (a) enter-battle.
    void TestBattlegrounds()
    {
        std::uint32_t const WSG = 1, AB = 2, RB = 9;

        // Queue/bracket matching: same bracket and same queue, or random BG
        // on either side; never across brackets.
        CHECK(P::BgQueueMatches(WSG, WSG, RB, 5, 5));
        CHECK(!P::BgQueueMatches(WSG, AB, RB, 5, 5));
        CHECK(P::BgQueueMatches(RB, AB, RB, 5, 5));
        CHECK(P::BgQueueMatches(WSG, RB, RB, 5, 5));
        CHECK(!P::BgQueueMatches(WSG, WSG, RB, 5, 4));
        CHECK(!P::BgQueueMatches(RB, RB, RB, 4, 5));

        // The two rules, exhaustively.
        for (int o = 0; o < 2; ++o)
            for (int r = 0; r < 2; ++r)
                for (int s = 0; s < 2; ++s)
                    for (int m = 0; m < 2; ++m)
                    {
                        CHECK(P::BgJoinRefused(o, r, s, m) == (o && r && s && m));
                        CHECK(P::BgPortRefused(o, r, s, m) == (o && r && s && m)); // s = entering
                    }
        CHECK(!P::BgPortRefused(true, true, false, true)); // leaving a queue is never refused

        // Roster: instance -> Vanilla humans inside.
        P::BgVanillaRoster roster;
        roster.Add(100, 1);
        roster.Add(100, 2);
        roster.Add(200, 3);
        roster.Add(0, 4);   // no instance: ignored
        roster.Add(300, 0); // no GUID: ignored
        CHECK(roster.Instances() == 2);
        CHECK(roster.HasVanillaHuman(100) && roster.HasVanillaHuman(200) && !roster.HasVanillaHuman(300));
        auto invitedTo = [](std::uint32_t want) { return [want](std::uint32_t i) { return i == want; }; };
        CHECK(roster.AnyInvitedInstanceHasVanillaHuman(invitedTo(100)));
        CHECK(!roster.AnyInvitedInstanceHasVanillaHuman(invitedTo(999)));
        roster.Remove(100, 1);
        CHECK(roster.HasVanillaHuman(100));     // one human still inside
        roster.Remove(100, 2);
        CHECK(!roster.HasVanillaHuman(100));    // last one left
        CHECK(!roster.AnyInvitedInstanceHasVanillaHuman(invitedTo(100)));
        roster.Remove(100, 2);                  // repeated leave is harmless
        roster.Remove(777, 1);
        roster.Clear(200);                      // instance destroyed
        CHECK(roster.Instances() == 0);

        // Queued humans: re-validated and pruned on every use.
        P::BgQueuedHumans queued;
        queued.Record(1, {WSG, WSG, 5});
        queued.Record(2, {AB, AB, 5});
        queued.Record(3, {WSG, WSG, 4});
        queued.Record(0, {WSG, WSG, 5}); // no GUID: ignored
        CHECK(queued.Size() == 3);
        auto all = [](std::uint64_t, P::BgQueuedHuman const&) { return true; };
        auto botIn = [&](std::uint32_t queue, std::uint32_t bracket) {
            return [=](P::BgQueuedHuman const& w) { return P::BgQueueMatches(queue, w.queue, RB, bracket, w.bracket); };
        };
        CHECK(queued.AnyMatching(all, botIn(WSG, 5)));   // human 1
        CHECK(queued.AnyMatching(all, botIn(AB, 5)));    // human 2
        CHECK(queued.AnyMatching(all, botIn(RB, 4)));    // random BG matches human 3's bracket
        CHECK(!queued.AnyMatching(all, botIn(WSG, 3)));  // no one in that bracket
        CHECK(queued.Size() == 3);
        // Human 1 left the queue / reached TBC: pruned, no longer matches.
        auto notOne = [](std::uint64_t g, P::BgQueuedHuman const&) { return g != 1; };
        CHECK(!queued.AnyMatching(notOne, botIn(WSG, 5)));
        CHECK(queued.Size() == 2);
        queued.Erase(2);
        CHECK(!queued.AnyMatching(all, botIn(AB, 5)));
        auto none = [](std::uint64_t, P::BgQueuedHuman const&) { return false; };
        CHECK(!queued.AnyMatching(none, botIn(WSG, 4)));
        CHECK(queued.Size() == 0);

        // Composition with identity: only a verified restricted bot is ever
        // refused; TBC/Wrath humans are never recorded as Vanilla.
        P::Bot ordinary = VerifiedBot(1);
        P::Bot altbot = VerifiedBot();
        altbot.verifiedRandomBotIdentity = false;
        for (P::Bot const& b : {ordinary, altbot, P::Bot{}})
            CHECK(!P::BgJoinRefused(true, P::BotIsRestrictedRandomBot(b), true, true) &&
                  !P::BgPortRefused(true, P::BotIsRestrictedRandomBot(b), true, true));
        CHECK(P::BgJoinRefused(true, P::BotIsRestrictedRandomBot(VerifiedBot(P::DraeneiRace)), true, true));
        for (std::uint8_t stage : {8, 12, 13, 18, 255})
            CHECK(!P::HumanStillVanilla(VanillaHuman(stage)));
    }
}

namespace
{
    // Option (d): the matcher never combines a still-Vanilla human with a
    // restricted RandomBot in one match, whatever the arrival order.
    void TestBattlegroundMatcher()
    {
        P::Bot const notABot{};
        P::Human const notAHuman{};
        P::MatchKinds const vanilla = P::KindsOf(VanillaHuman(3), notABot);
        P::MatchKinds const tbc = P::KindsOf(VanillaHuman(8), notABot);
        P::MatchKinds const bot = P::KindsOf(notAHuman, VerifiedBot(P::BloodElfRace));
        P::MatchKinds const ordinary = P::KindsOf(notAHuman, VerifiedBot(1));
        P::Bot altbotFacts = VerifiedBot();
        altbotFacts.verifiedRandomBotIdentity = false;
        P::MatchKinds const altbot = P::KindsOf(notAHuman, altbotFacts);
        P::MatchKinds const unknown{};
        CHECK(vanilla.stillVanillaHuman && !vanilla.restrictedBot && bot.restrictedBot && !bot.stillVanillaHuman);
        CHECK(!tbc.Any() && !ordinary.Any() && !altbot.Any() && !unknown.Any());

        // The rule, exhaustively over every candidate/others combination.
        for (int o = 0; o < 2; ++o)
            for (int cv = 0; cv < 2; ++cv)
                for (int cb = 0; cb < 2; ++cb)
                    for (int ov = 0; ov < 2; ++ov)
                        for (int ob = 0; ob < 2; ++ob)
                        {
                            P::MatchKinds c{cv != 0, cb != 0}, x{ov != 0, ob != 0};
                            bool const expected = o && ((cb && ov) || (cv && ob));
                            CHECK(P::MatchPoolRejects(o != 0, c, x) == expected);
                        }

        // Matcher simulation: groups arrive in every order; each is added
        // unless the rule refuses it. No final match may hold both kinds,
        // and harmless kinds are never refused.
        std::vector<P::MatchKinds> const kinds{vanilla, bot, tbc, ordinary, altbot, unknown, bot, vanilla};
        std::vector<int> order(kinds.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = static_cast<int>(i);
        unsigned permutations = 0;
        do
        {
            P::MatchKinds pool;
            for (int index : order)
            {
                P::MatchKinds const candidate = kinds[static_cast<std::size_t>(index)];
                bool const refused = P::MatchPoolRejects(true, candidate, pool);
                if (!candidate.Any())
                    CHECK(!refused);   // TBC, ordinary bot, Altbot, unknown: always admitted
                if (!refused)
                    pool.Add(candidate);
            }
            CHECK(!(pool.stillVanillaHuman && pool.restrictedBot));
            // Whoever is first decides; the other kind waits for another match.
            P::MatchKinds const first = kinds[static_cast<std::size_t>(
                *std::find_if(order.begin(), order.end(), [&](int i) { return kinds[static_cast<std::size_t>(i)].Any(); }))];
            CHECK(pool.stillVanillaHuman == first.stillVanillaHuman && pool.restrictedBot == first.restrictedBot);
            ++permutations;
        } while (std::next_permutation(order.begin(), order.end()));
        CHECK(permutations == 40320);

        // A group carrying both kinds itself (should not exist) is refused
        // against either kind and admitted into an empty or harmless match.
        P::MatchKinds both = vanilla;
        both.Add(bot);
        CHECK(P::MatchPoolRejects(true, both, vanilla) && P::MatchPoolRejects(true, both, bot));
        CHECK(!P::MatchPoolRejects(true, both, tbc));
        CHECK(!P::MatchPoolRejects(false, vanilla, bot));

        // Premade order in Core: check Alliance premade, check Horde
        // premade, THEN add both. Without the peer, each sees an empty pool
        // (the gap); comparing each with its paired premade closes it.
        P::MatchKinds const kindsList[] = {vanilla, bot, tbc, ordinary, unknown};
        for (P::MatchKinds const& ali : kindsList)
            for (P::MatchKinds const& horde : kindsList)
            {
                P::MatchKinds const emptyPool{};
                bool const withoutPeer = !P::MatchPoolRejects(true, ali, emptyPool) &&
                    !P::MatchPoolRejects(true, horde, emptyPool);
                bool const withPeer = !P::MatchPoolRejects(true, ali, horde) &&
                    !P::MatchPoolRejects(true, horde, ali);
                bool const mixed = (ali.stillVanillaHuman && horde.restrictedBot) ||
                    (ali.restrictedBot && horde.stillVanillaHuman);
                CHECK(withoutPeer);            // the gap the peer check closes
                CHECK(withPeer == !mixed);     // never a mixed premade match
            }
    }
}

namespace
{
    // Duel and mail follow the same pair rule and reply rule as every other
    // surface: blocked both ways for a restricted pair; the bot answers only
    // a human-started attempt; everyone else is stock.
    void TestDuelAndMail()
    {
        for (P::Surface s : {P::Surface::Duel, P::Surface::Mail, P::Surface::GuildInvite})
        {
            P::Decision const h2b = P::Decide(true, s, P::Direction::HumanToBot, VanillaHuman(), VerifiedBot(), false);
            P::Decision const b2h = P::Decide(true, s, P::Direction::BotToHuman, VanillaHuman(), VerifiedBot(), false);
            CHECK(h2b.block && h2b.reply);
            CHECK(b2h.block && !b2h.reply);
            CHECK(!P::Decide(true, s, P::Direction::HumanToBot, VanillaHuman(8), VerifiedBot(), false).block);
            CHECK(!P::Decide(true, s, P::Direction::HumanToBot, VanillaHuman(), VerifiedBot(1), false).block);
            CHECK(!P::Decide(false, s, P::Direction::HumanToBot, VanillaHuman(), VerifiedBot(), false).block);
            P::Bot altbot = VerifiedBot();
            altbot.verifiedRandomBotIdentity = false;
            CHECK(!P::Decide(true, s, P::Direction::HumanToBot, VanillaHuman(), altbot, false).block);
            // The addon flag only matters for whispers.
            CHECK(P::Decide(true, s, P::Direction::HumanToBot, VanillaHuman(), VerifiedBot(), true).reply);
        }
    }
}

namespace
{
    // Offline mail: only a bot confirmed earlier in this run is refused.
    void TestOfflineMail()
    {
        for (int o = 0; o < 2; ++o)
            for (int v = 0; v < 2; ++v)
                for (int c = 0; c < 2; ++c)
                    CHECK(P::OfflineMailRefused(o, v, c) == (o && v && c));
        P::ConfirmedRestrictedBots confirmed;
        CHECK(!confirmed.Contains(42));            // never seen this run: unknown
        confirmed.Remember(42);
        confirmed.Remember(0);                     // no GUID: ignored
        confirmed.Remember(42);                    // idempotent
        CHECK(confirmed.Size() == 1 && confirmed.Contains(42));
        bool const vanillaSender = P::HumanStillVanilla(VanillaHuman(3));
        bool const tbcSender = P::HumanStillVanilla(VanillaHuman(8));
        CHECK(P::OfflineMailRefused(true, vanillaSender, confirmed.Contains(42)));
        CHECK(!P::OfflineMailRefused(true, tbcSender, confirmed.Contains(42)));
        CHECK(!P::OfflineMailRefused(true, vanillaSender, confirmed.Contains(43)));
        CHECK(!P::OfflineMailRefused(false, vanillaSender, confirmed.Contains(42)));
        // Login forgets; a still-verified bot is re-remembered at logout, a
        // revoked one is not.
        confirmed.Forget(42);
        CHECK(!confirmed.Contains(42) && confirmed.Size() == 0);
        confirmed.Forget(42); // harmless when absent
        confirmed.Remember(42);
        CHECK(confirmed.Contains(42));
    }
}

namespace
{
    // Replies decided off the world thread wait in a bounded queue and are
    // sent once from the world update; full means dropped, never unbounded.
    void TestPendingReplies()
    {
        P::PendingReplies<3> pending;
        CHECK(!pending.Push(0, 5) && !pending.Push(5, 0));   // no GUID: ignored
        CHECK(pending.Push(1, 10) && pending.Push(2, 20) && pending.Push(3, 30));
        CHECK(!pending.Push(4, 40));                          // full: dropped
        CHECK(pending.Size() == 3 && pending.Dropped() == 1);
        std::vector<std::uint64_t> order;
        pending.Drain([&](std::uint64_t h, std::uint64_t b) { order.push_back(h * 100 + b); });
        CHECK((order == std::vector<std::uint64_t>{110, 220, 330}));   // oldest first, once each
        CHECK(pending.Size() == 0);
        unsigned again = 0;
        pending.Drain([&](std::uint64_t, std::uint64_t) { ++again; });
        CHECK(again == 0);                                    // nothing is sent twice
        CHECK(pending.Push(4, 40) && pending.Size() == 1);    // usable again after a drain

        // One reply per cooldown window survives deferral: the limiter is
        // consulted before queueing, so a burst queues at most one.
        P::ReplyLimiter<8> limiter;
        P::PendingReplies<16> queue;
        for (int attempt = 0; attempt < 10; ++attempt)
            if (limiter.MayReply(7, 70, 1000, 60))
                queue.Push(7, 70);
        CHECK(queue.Size() == 1);
    }
}

namespace
{
    // F-2: the group script's add-member path, step for step: the gate,
    // then the tracked-party refresh under the lock.
    bool ModelOnAddMember(P::QueueFactsTable& table, bool optionOn, bool worldThread,
                          std::uint64_t groupGuid, bool inRoleCheck, std::uint64_t member,
                          P::QueueFacts facts)
    {
        if (!optionOn || !groupGuid || !member || !worldThread)
            return false;
        bool const tracked = table.TracksGroup(groupGuid);
        if (!P::RoleCheckMemberRefresh(true, true, tracked, tracked && inRoleCheck))
            return false;
        return table.RefreshGroupMember(groupGuid, member, facts);
    }

    void TestRoleCheckMemberRefresh()
    {
        using G = std::vector<std::uint64_t>;
        P::QueueFacts const vanilla = P::MakeQueueFacts(VanillaHuman(3), P::Bot{});
        P::QueueFacts const tbc = P::MakeQueueFacts(VanillaHuman(8), P::Bot{});
        P::QueueFacts const restricted = P::MakeQueueFacts(P::Human{}, VerifiedBot());
        for (int o = 0; o < 2; ++o)
            for (int w = 0; w < 2; ++w)
                for (int t = 0; t < 2; ++t)
                    for (int r = 0; r < 2; ++r)
                        CHECK(P::RoleCheckMemberRefresh(o, w, t, r) == (o && w && t && r));

        std::uint64_t const party = 2000, soloBot = 700, soloHuman = 701;
        auto fresh = [&]()
        {
            P::QueueFactsTable t;
            t.Record(601, tbc);              // the party queued with only a TBC member
            t.RecordGroup(party, {601});
            t.Record(soloBot, restricted);
            t.Record(soloHuman, vanilla);
            return t;
        };

        // The gap without F-2: a Vanilla human added during the role check is
        // missing from the record, so party + solo restricted bot passes.
        {
            P::QueueFactsTable t = fresh();
            CHECK(!t.Rejects(true, G{party, soloBot}));
            // Role check add H: the party now carries the Vanilla human.
            CHECK(ModelOnAddMember(t, true, true, party, true, 602, vanilla));
            CHECK(t.Rejects(true, G{party, soloBot}));
            CHECK(t.Groups() == 1 && t.Size() == 4);
            // Duplicate add: idempotent (no second entry, same verdict).
            CHECK(ModelOnAddMember(t, true, true, party, true, 602, vanilla));
            CHECK(t.Rejects(true, G{party, soloBot}));
            // Member logout: its facts and the party record go (inert).
            t.Erase(602);
            CHECK(t.Groups() == 0 && !t.Rejects(true, G{party, soloBot}));
        }
        // Role check add R: the party now carries a restricted bot.
        {
            P::QueueFactsTable t = fresh();
            CHECK(!t.Rejects(true, G{party, soloHuman}));
            CHECK(ModelOnAddMember(t, true, true, party, true, 603, restricted));
            CHECK(t.Rejects(true, G{party, soloHuman}));
        }
        // Not applied: QUEUED (Core removes the party), untracked party, OFF,
        // off the world thread, zero GUIDs. Nothing changes in any case.
        {
            P::QueueFactsTable t = fresh();
            CHECK(!ModelOnAddMember(t, true, true, party, false, 602, vanilla));   // ordinary queued add
            CHECK(!ModelOnAddMember(t, true, true, 2999, true, 602, vanilla));     // untracked group
            CHECK(!ModelOnAddMember(t, false, true, party, true, 602, vanilla));   // OFF
            CHECK(!ModelOnAddMember(t, true, false, party, true, 602, vanilla));   // off-world callback
            CHECK(!ModelOnAddMember(t, true, true, 0, true, 602, vanilla));
            CHECK(!ModelOnAddMember(t, true, true, party, true, 0, vanilla));
            CHECK(t.Groups() == 1 && t.Size() == 3 && !t.TracksGroup(2999));
            CHECK(!t.Rejects(true, G{party, soloBot}));
            // The table itself never creates a party record either.
            CHECK(!t.RefreshGroupMember(2999, 602, vanilla) && !t.TracksGroup(2999));
        }
        // Requeue overwrites the refreshed list; disband drops the record.
        {
            P::QueueFactsTable t = fresh();
            CHECK(ModelOnAddMember(t, true, true, party, true, 602, vanilla));
            t.RecordGroup(party, {601});     // party left, rejoined without 602
            CHECK(!t.Rejects(true, G{party, soloBot}));
            t.EraseGroup(party);
            CHECK(t.Groups() == 0 && !t.TracksGroup(party));
            t.EraseGroup(party);             // harmless twice
            CHECK(!ModelOnAddMember(t, true, true, party, true, 602, vanilla));
        }
        // OFF -> ON -> OFF: OFF adds are ignored, ON adds apply, OFF vetoes nothing.
        {
            P::QueueFactsTable t = fresh();
            CHECK(!ModelOnAddMember(t, false, true, party, true, 602, vanilla));
            CHECK(!t.Rejects(true, G{party, soloBot}));
            CHECK(ModelOnAddMember(t, true, true, party, true, 602, vanilla));
            CHECK(t.Rejects(true, G{party, soloBot}));
            CHECK(!t.Rejects(false, G{party, soloBot}));
        }
    }

    // F-3: create/delete forget the offline-mail entry for that GUID, with
    // the option in any state, and never add one.
    void TestCharacterCreateDeleteCleanup()
    {
        P::ConfirmedRestrictedBots confirmed;
        bool const vanillaSender = P::HumanStillVanilla(VanillaHuman(3));
        confirmed.Remember(42);
        confirmed.Remember(77);
        // Remembered -> deleted -> absent.
        confirmed.Forget(42);                       // OnPlayerDelete(42)
        CHECK(!confirmed.Contains(42) && confirmed.Contains(77));
        // GUID reused by a new character before its first login: not blocked.
        confirmed.Forget(42);                       // OnPlayerCreate(new 42), duplicate is harmless
        CHECK(!P::OfflineMailRefused(true, vanillaSender, confirmed.Contains(42)));
        // Stale entry that was never deleted (cluster reuse): create clears it.
        confirmed.Remember(55);
        CHECK(P::OfflineMailRefused(true, vanillaSender, confirmed.Contains(55)));
        confirmed.Forget(55);                       // OnPlayerCreate(new 55)
        CHECK(!P::OfflineMailRefused(true, vanillaSender, confirmed.Contains(55)));
        // Unrelated GUID unchanged; nothing was ever added by cleanup.
        CHECK(confirmed.Contains(77) && confirmed.Size() == 1);
        // Cleanup is option-independent: forgetting with the option OFF still
        // leaves no entry for a later ON period.
        confirmed.Forget(77);
        CHECK(!P::OfflineMailRefused(true, vanillaSender, confirmed.Contains(77)) && confirmed.Size() == 0);
    }
}

namespace
{
    // Guild membership (owner decision 14 Nov 2026): the decision rule, the
    // per-member rule, and the drain's end state over small guilds.
    void TestGuildMembership()
    {
        using A = P::GuildJoinAction;
        for (int o = 0; o < 2; ++o)
            for (int h = 0; h < 2; ++h)
                for (int b = 0; b < 2; ++b)
                    for (int m = 0; m < 2; ++m)
                    {
                        A const a = P::DecideGuildJoin(o, h, b, m);
                        A const want = !o ? A::None : h ? A::RemoveRestrictedMembers
                            : (b && m) ? A::RemoveJoiner : A::None;
                        CHECK(a == want);
                    }
        // Per member: live facts while online; the confirmed set while offline.
        for (int on = 0; on < 2; ++on)
            for (int r = 0; r < 2; ++r)
                for (int c = 0; c < 2; ++c)
                    CHECK(P::GuildMemberRestricted(on, r, c) == (on ? r != 0 : c != 0));

        // Kinds of member: 0 Vanilla human online, 1 TBC human online,
        // 2 restricted bot online, 3 restricted bot offline+confirmed,
        // 4 restricted bot offline unknown, 5 Altbot (headless) online,
        // 6 Vanilla human offline, 7 ordinary bot online.
        bool const vanilla = P::HumanStillVanilla(VanillaHuman(3));
        bool const tbcH = P::HumanStillVanilla(VanillaHuman(8));
        P::Human altbot = VanillaHuman(3);
        altbot.nonHeadlessSession = false;
        CHECK(vanilla && !tbcH && !P::HumanStillVanilla(altbot));
        bool const rBot = P::BotIsRestrictedRandomBot(VerifiedBot());
        CHECK(rBot && !P::BotIsRestrictedRandomBot(P::Bot{}));
        struct Kind { bool online, vanillaHuman, restrictedOnline, confirmed; };
        Kind const kinds[] = {
            {true, true, false, false}, {true, false, false, false}, {true, false, true, false},
            {false, false, false, true}, {false, false, false, false}, {true, false, false, false},
            {false, false, false, false}, {true, false, false, false}};
        // Exhaustive: joiner kind x every guild of up to 8 other members.
        for (int j = 0; j < 8; ++j)
            for (unsigned mask = 0; mask < 256; ++mask)
            {
                Kind const joiner = kinds[j];
                if (!joiner.online)
                    continue; // an offline joiner is never evaluated
                bool anyHuman = false;
                std::vector<int> restricted;
                for (int k = 0; k < 8; ++k)
                    if (mask & (1u << k))
                    {
                        anyHuman = anyHuman || (kinds[k].online && kinds[k].vanillaHuman);
                        if (P::GuildMemberRestricted(kinds[k].online, kinds[k].restrictedOnline, kinds[k].confirmed))
                            restricted.push_back(k);
                    }
                A const a = P::DecideGuildJoin(true, joiner.vanillaHuman, joiner.restrictedOnline, anyHuman);
                // End state: no restricted bot shares the guild with the
                // joining Vanilla human, and a restricted joiner never stays
                // beside an online Vanilla human.
                bool joinerStays = true;
                std::size_t removed = 0;
                if (a == A::RemoveRestrictedMembers)
                    removed = restricted.size();
                if (a == A::RemoveJoiner)
                    joinerStays = false;
                if (joiner.vanillaHuman)
                {
                    CHECK(a == A::RemoveRestrictedMembers);
                    CHECK(removed == restricted.size());
                    // Kind 4 (offline, never confirmed) is unknown: kept.
                    bool const unknownKept = (mask & (1u << 4)) == 0 ||
                        std::find(restricted.begin(), restricted.end(), 4) == restricted.end();
                    CHECK(unknownKept);
                }
                else if (joiner.restrictedOnline)
                {
                    CHECK(joinerStays == !anyHuman);
                    CHECK(removed == 0);
                }
                else
                {
                    CHECK(a == A::None); // TBC humans, Altbots, ordinary bots: stock
                }
                // OFF: nothing, ever.
                CHECK(P::DecideGuildJoin(false, joiner.vanillaHuman, joiner.restrictedOnline, anyHuman) == A::None);
            }
        // Joiner facts: live when online; captured at the join (world
        // thread) when it logged out before the world update; a confirmed
        // bot; otherwise UNKNOWN (never acted on).
        P::GuildJoinerFacts const none{};
        P::GuildJoinerFacts const capHuman{true, true, false};
        P::GuildJoinerFacts const capBot{true, false, true};
        for (int on = 0; on < 2; ++on)
            for (int lh = 0; lh < 2; ++lh)
                for (int lb = 0; lb < 2; ++lb)
                    for (int c = 0; c < 3; ++c)
                        for (int conf = 0; conf < 2; ++conf)
                        {
                            P::GuildJoinerFacts const cap = c == 0 ? none : c == 1 ? capHuman : capBot;
                            P::GuildJoinerFacts const f = P::ResolveGuildJoiner(on, lh, lb, cap, conf);
                            if (on)
                                CHECK(f.known && f.stillVanillaHuman == (lh != 0) && f.restrictedBot == (lb != 0));
                            else if (cap.known)
                                CHECK(f.known && f.stillVanillaHuman == cap.stillVanillaHuman &&
                                      f.restrictedBot == cap.restrictedBot);
                            else if (conf)
                                CHECK(f.known && !f.stillVanillaHuman && f.restrictedBot);
                            else
                                CHECK(!f.known && !f.stillVanillaHuman && !f.restrictedBot); // offline add, no facts
                        }
        // Logout before the world update: the captured human still acts.
        {
            P::GuildJoinerFacts const f = P::ResolveGuildJoiner(false, false, false, capHuman, false);
            CHECK(P::DecideGuildJoin(true, f.stillVanillaHuman, f.restrictedBot, false) == A::RemoveRestrictedMembers);
        }
        // Removal safety, proven before every removal.
        for (int g = 0; g < 2; ++g)
            for (int m = 0; m < 2; ++m)
                for (std::size_t count = 0; count < 4; ++count)
                    for (int r = 0; r < 2; ++r)
                        CHECK(P::GuildRemovalSafe(g, m, count, r) == (g && m && count >= 2 && r));
        CHECK(!P::GuildRemovalSafe(true, true, 1, true));   // sole member (even the leader): never, no disband
        CHECK(!P::GuildRemovalSafe(true, true, 5, false));  // add -> leave -> drain: the reason is gone
        CHECK(!P::GuildRemovalSafe(true, false, 5, true));  // already removed (duplicate)
        CHECK(!P::GuildRemovalSafe(false, true, 5, true));  // guild gone
        // Sequential removals: re-proven after each one; the human survivor
        // keeps every removal safe, and the guild never drops below one.
        for (std::size_t bots = 0; bots < 20; ++bots)
        {
            std::size_t members = bots + 1; // the Vanilla human + restricted bots
            for (std::size_t i = 0; i < bots; ++i)
            {
                CHECK(P::GuildRemovalSafe(true, true, members, true));
                --members;
            }
            CHECK(members == 1);
        }
        // Guild chat sender gate: only a restricted bot, only beside an
        // online Vanilla human, only ON. Humans are never blocked here.
        for (int o2 = 0; o2 < 2; ++o2)
            for (int b = 0; b < 2; ++b)
                for (int h = 0; h < 2; ++h)
                    CHECK(P::GuildChatSenderBlocked(o2, b, h) == (o2 && b && h));

        // Pending joins never drop: coalesce per (guild, joiner); facts
        // overwrite only when known; drained completely.
        P::PendingGuildJoins joins;
        joins.Mark(0, 5, capHuman);
        joins.Mark(7, 0, capHuman);
        CHECK(joins.Size() == 0); // no id: ignored
        for (std::uint64_t i = 1; i <= 10000; ++i)
            joins.Mark(7, i, none);
        CHECK(joins.Size() == 10000);
        joins.Mark(7, 42, capHuman);
        joins.Mark(7, 42, none);  // an unknown re-mark keeps the known facts
        joins.Mark(7, 42, none);
        CHECK(joins.Size() == 10000);
        joins.Mark(8, 42, capBot); // the same character in another guild id: separate
        CHECK(joins.Size() == 10001);
        std::size_t drained = 0, known = 0;
        bool sawHuman = false, sawBot = false;
        joins.Drain([&](std::uint32_t g, std::uint64_t p, P::GuildJoinerFacts f)
        {
            ++drained;
            known += f.known ? 1 : 0;
            if (g == 7 && p == 42)
                sawHuman = f.known && f.stillVanillaHuman;
            if (g == 8 && p == 42)
                sawBot = f.known && f.restrictedBot;
        });
        CHECK(drained == 10001 && known == 2 && sawHuman && sawBot && joins.Size() == 0);
        joins.Drain([&](std::uint32_t, std::uint64_t, P::GuildJoinerFacts) { CHECK(false); }); // empty after drain
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
    TestGroupAcceptMemberScan();
    TestThreadSafeReads();
    TestOwnedMapHelpers();
    TestDungeonFinderQueue();
    TestBattlegrounds();
    TestBattlegroundMatcher();
    TestDuelAndMail();
    TestOfflineMail();
    TestPendingReplies();
    TestRoleCheckMemberRefresh();
    TestCharacterCreateDeleteCleanup();
    TestGuildMembership();
    std::printf("F14 SOCIAL ISOLATION POLICY %u/%u passed; NO WORLD HIDING\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
