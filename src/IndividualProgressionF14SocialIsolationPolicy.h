/*
 * CosmicCuddle F14 social isolation policy (pure, value-only).
 *
 * Owner-selected safer F14 behaviour: while an ordinary human is still in
 * Vanilla, a POSITIVELY VERIFIED Blood Elf/Draenei RandomBot does not trade,
 * whisper, group or appear in /who with that human. When the human tries,
 * the bot answers once (rate-limited) in character. The restriction is
 * recomputed on every attempt, so it lifts the moment the human reaches the
 * TBC floor.
 *
 * Identity is never race alone: the target must carry the existing
 * login-thread verified RandomBot identity, a headless session and no GM
 * mode. Humans, Altbots, SelfBots and unknown characters are never
 * restricted. Unknown or out-of-range facts mean "not restricted", so the
 * historical behaviour is the fallback. This never hides anyone in the world.
 *
 * No Core pointers, DB, Playerbots or packets live here.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_SOCIAL_ISOLATION_POLICY_H
#define INDIVIDUAL_PROGRESSION_F14_SOCIAL_ISOLATION_POLICY_H

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace IndividualProgressionF14SocialIsolationPolicy
{
    constexpr std::uint8_t BloodElfRace = 10;
    constexpr std::uint8_t DraeneiRace = 11;
    constexpr std::uint8_t FirstTbcStage = 8;
    constexpr std::uint8_t LastSupportedStage = 18;

    constexpr std::uint32_t MinReplyCooldownSeconds = 5;
    constexpr std::uint32_t MaxReplyCooldownSeconds = 3600;
    constexpr std::uint32_t DefaultReplyCooldownSeconds = 60;
    constexpr char const* DefaultReply =
        "You are not yet ready for the road I walk. Return when your journey has carried you into a later age.";

    // Module-level safety cap for the configured reply (not a claimed Core
    // protocol limit): one line, at most this many UTF-8 bytes.
    constexpr std::size_t MaxReplyBytes = 512;

    // Make an operator-configured reply safe to send: every control byte
    // (CR, LF, NUL, tab, other C0 and DEL) becomes a space, surrounding spaces
    // are trimmed, and the text is cut to MaxReplyBytes without splitting a
    // UTF-8 character. An empty result means "decline silently".
    inline std::string NormalizeReply(std::string const& raw)
    {
        std::string text;
        text.reserve(raw.size() < MaxReplyBytes ? raw.size() : MaxReplyBytes);
        for (char c : raw)
        {
            unsigned char const u = static_cast<unsigned char>(c);
            text.push_back(u < 0x20 || u == 0x7F ? ' ' : c);
        }
        std::size_t const first = text.find_first_not_of(' ');
        if (first == std::string::npos)
            return std::string();
        text.erase(0, first);
        if (text.size() > MaxReplyBytes)
        {
            std::size_t cut = MaxReplyBytes;
            // Never end inside a multi-byte sequence: back up over
            // continuation bytes, then drop the partial lead byte.
            while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
                --cut;
            text.resize(cut);
        }
        std::size_t const last = text.find_last_not_of(' ');
        text.resize(last == std::string::npos ? 0 : last + 1);
        return text;
    }

    // The human side of a pair. Defaults are the non-restricting values.
    struct Human
    {
        bool present = false;
        bool inWorld = false;
        bool nonHeadlessSession = false;     // a real client is connected
        bool playerAccountSecurity = false;  // not a GM-level account
        bool gameMasterMode = true;
        bool verifiedRandomBot = true;       // a verified bot is never "the human"
        std::uint8_t rawStage = 255;         // in-memory IP stage; 255 = unknown
    };

    // The RandomBot side of a pair. Defaults are the non-restricting values.
    struct Bot
    {
        bool present = false;
        bool inWorld = false;
        bool verifiedRandomBotIdentity = false; // login-thread verified epoch
        bool headlessSession = false;
        bool gameMasterMode = true;
        std::uint8_t race = 0;
    };

    enum class Surface : std::uint8_t
    {
        Whisper,
        Trade,
        GroupInvite,
        GroupAccept
    };

    enum class Direction : std::uint8_t
    {
        HumanToBot, // the human initiated
        BotToHuman  // the bot initiated
    };

    struct Decision
    {
        bool block = false;
        bool reply = false;
    };

    constexpr bool IsTbcRace(std::uint32_t race)
    {
        return race == BloodElfRace || race == DraeneiRace;
    }

    constexpr bool HumanStillVanilla(Human const& h)
    {
        return h.present && h.inWorld && h.nonHeadlessSession && h.playerAccountSecurity &&
            !h.gameMasterMode && !h.verifiedRandomBot &&
            h.rawStage <= LastSupportedStage && h.rawStage < FirstTbcStage;
    }

    constexpr bool BotIsRestrictedRandomBot(Bot const& b)
    {
        return b.present && b.inWorld && b.verifiedRandomBotIdentity && b.headlessSession &&
            !b.gameMasterMode && IsTbcRace(b.race);
    }

    constexpr bool IsRestrictedPair(bool optionEnabled, Human const& h, Bot const& b)
    {
        return optionEnabled && HumanStillVanilla(h) && BotIsRestrictedRandomBot(b);
    }

    // Block the interaction for a restricted pair, in either direction. The
    // bot answers only when the human started it, and never for addon data.
    constexpr Decision Decide(bool optionEnabled, Surface surface, Direction direction,
                              Human const& h, Bot const& b, bool addonMessage)
    {
        Decision d;
        if (!IsRestrictedPair(optionEnabled, h, b))
            return d;
        d.block = true;
        d.reply = direction == Direction::HumanToBot &&
            !(surface == Surface::Whisper && addonMessage);
        return d;
    }

    // Accepting a group invitation: the human is refused when ANY current
    // member (leader included) is a restricted RandomBot for that human.
    // Members that could not be resolved carry default (non-restricting)
    // facts, so nobody is ever classified from race alone. Returns the
    // index of the first restricted member, or members.size() for "allow".
    template<typename BotRange>
    std::size_t FirstRestrictedGroupMember(bool optionEnabled, Human const& h, BotRange const& members)
    {
        std::size_t index = 0;
        for (Bot const& b : members)
        {
            if (IsRestrictedPair(optionEnabled, h, b))
                return index;
            ++index;
        }
        return index;
    }

    // T1: which thread may read another player's state.
    //
    // Client packets for the social opcodes run on the world thread while no
    // map updates, so any pair may be read there. Playerbots AI calls the
    // same handlers from a bot's update inside a map update. There, a player
    // may be read only when its pointer is found in the player list of the
    // map this thread owns in the current world tick: either the map it is
    // updating now, or one it already finished this tick (each map updates
    // once per tick, and the world waits for all of them before the tick
    // ends). The foreign Player object is never touched to decide this.
    // Anything else is not read and not restricted: the existing "unknown
    // means not restricted" fallback (owner decision: allow).
    constexpr bool OwnedScopeValid(bool recorded, std::uint64_t recordedTick, std::uint64_t currentTick)
    {
        return recorded && recordedTick == currentTick;
    }

    constexpr bool MayReadPair(bool onWorldThread, bool ownedScopeValid, bool firstInOwnedMap,
                               bool secondInOwnedMap)
    {
        return onWorldThread || (ownedScopeValid && firstInOwnedMap && secondInOwnedMap);
    }

    // Remembers the world thread and counts world ticks; marked from the
    // world update, which runs after every map update of the tick. Before
    // the first mark nothing counts as the world thread.
    class WorldThreadMarker
    {
    public:
        void Mark()
        {
            _id.store(std::this_thread::get_id(), std::memory_order_release);
            _tick.fetch_add(1, std::memory_order_acq_rel);
        }
        bool IsCurrent() const
        {
            std::thread::id const id = _id.load(std::memory_order_acquire);
            return id != std::thread::id() && id == std::this_thread::get_id();
        }
        std::uint64_t Tick() const { return _tick.load(std::memory_order_acquire); }

    private:
        std::atomic<std::thread::id> _id{};
        std::atomic<std::uint64_t> _tick{0};
    };

    // /who carries only a cached GUID and race: the target is restricted
    // only when that exact GUID holds a current verified RandomBot identity.
    constexpr bool ExcludeFromWho(bool optionEnabled, Human const& h,
                                  bool targetGuidVerifiedRandomBot, std::uint32_t cachedRace)
    {
        return optionEnabled && HumanStillVanilla(h) && targetGuidVerifiedRandomBot &&
            IsTbcRace(cachedRace);
    }

    constexpr std::uint32_t ClampCooldown(std::int64_t seconds)
    {
        return seconds < MinReplyCooldownSeconds ? MinReplyCooldownSeconds
            : seconds > MaxReplyCooldownSeconds ? MaxReplyCooldownSeconds
            : static_cast<std::uint32_t>(seconds);
    }

    // At most one in-character reply per (human, bot) pair per cooldown.
    // Bounded: when every slot is still cooling down, no reply is sent (the
    // interaction stays blocked). A clock that moves backwards never
    // produces a reply; it restarts that pair's cooldown.
    template<std::size_t Capacity>
    class ReplyLimiter
    {
        static_assert(Capacity > 0, "reply limiter needs capacity");

    public:
        bool MayReply(std::uint64_t human, std::uint64_t bot, std::uint64_t nowSeconds,
                      std::uint32_t cooldownSeconds)
        {
            if (!human || !bot)
                return false;
            std::uint32_t const cooldown = ClampCooldown(cooldownSeconds);
            std::size_t reusable = Capacity;
            for (std::size_t i = 0; i < Capacity; ++i)
            {
                Entry& e = _entries[i];
                if (e.used && e.human == human && e.bot == bot)
                {
                    if (nowSeconds < e.lastReply)
                    {
                        e.lastReply = nowSeconds;
                        ++_suppressed;
                        return false;
                    }
                    if (nowSeconds - e.lastReply < cooldown)
                    {
                        ++_suppressed;
                        return false;
                    }
                    e.lastReply = nowSeconds;
                    return true;
                }
                if (reusable == Capacity &&
                    (!e.used || nowSeconds < e.lastReply || nowSeconds - e.lastReply >= cooldown))
                    reusable = i;
            }
            if (reusable == Capacity)
            {
                ++_full;
                return false;
            }
            _entries[reusable] = Entry{human, bot, nowSeconds, true};
            return true;
        }

        std::uint64_t Suppressed() const { return _suppressed; }
        std::uint64_t FullRefusals() const { return _full; }

    private:
        struct Entry
        {
            std::uint64_t human = 0;
            std::uint64_t bot = 0;
            std::uint64_t lastReply = 0;
            bool used = false;
        };
        std::array<Entry, Capacity> _entries{};
        std::uint64_t _suppressed = 0;
        std::uint64_t _full = 0;
    };
}

#endif
