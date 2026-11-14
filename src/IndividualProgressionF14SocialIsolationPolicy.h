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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
        GroupAccept,
        Duel,
        Mail,
        GuildInvite
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

    // Replies decided off the world thread (for example a duel request,
    // processed in the caster's map update) are not sent there: they wait
    // here and the world-thread update sends them. Bounded: when full, the
    // reply is dropped (the interaction stays blocked either way).
    template<std::size_t Capacity>
    class PendingReplies
    {
        static_assert(Capacity > 0, "pending replies need capacity");

    public:
        bool Push(std::uint64_t human, std::uint64_t bot)
        {
            if (!human || !bot || _size == Capacity)
            {
                if (_size == Capacity)
                    ++_dropped;
                return false;
            }
            _items[_size++] = Item{human, bot};
            return true;
        }

        // Hands every waiting reply to `send(human, bot)` once, oldest
        // first, and empties the queue.
        template<typename Send>
        void Drain(Send send)
        {
            std::size_t const count = _size;
            _size = 0;
            for (std::size_t i = 0; i < count; ++i)
                send(_items[i].human, _items[i].bot);
        }

        std::size_t Size() const { return _size; }
        std::uint64_t Dropped() const { return _dropped; }

    private:
        struct Item
        {
            std::uint64_t human = 0;
            std::uint64_t bot = 0;
        };
        std::array<Item, Capacity> _items{};
        std::size_t _size = 0;
        std::uint64_t _dropped = 0;
    };

    // Dungeon Finder (owner decision 13 Nov 2026): a still-Vanilla human
    // who queues is never matched into one group with a restricted
    // RandomBot. TBC/Wrath humans keep stock matching. The facts are taken
    // when each player joins the queue (world thread) and looked up by GUID
    // only, because the matcher runs on a worker thread beside the maps.
    struct QueueFacts
    {
        bool restrictedBot = false;      // BotIsRestrictedRandomBot at join
        bool stillVanillaHuman = false;  // HumanStillVanilla at join
    };

    constexpr QueueFacts MakeQueueFacts(Human const& asHuman, Bot const& asBot)
    {
        return QueueFacts{BotIsRestrictedRandomBot(asBot), HumanStillVanilla(asHuman)};
    }

    constexpr bool QueueGroupRejected(bool optionEnabled, bool anyStillVanillaHuman, bool anyRestrictedBot)
    {
        return optionEnabled && anyStillVanillaHuman && anyRestrictedBot;
    }

    // GUID -> facts captured at queue join. Not thread-safe by itself; the
    // script guards it with a mutex. Unknown GUIDs carry no facts and so
    // never restrict anyone.
    class QueueFactsTable
    {
    public:
        void Record(std::uint64_t guid, QueueFacts facts)
        {
            if (guid)
                _facts[guid] = facts;
        }
        // A party that queued together: Core's proposal may name it only by
        // its group GUID (a full proposal carries no roles map), so its
        // members are recorded at join and expanded here.
        void RecordGroup(std::uint64_t groupGuid, std::vector<std::uint64_t> members)
        {
            if (groupGuid)
                _groups[groupGuid] = std::move(members);
        }

        // Logout: forget the player's facts and every party that lists them.
        void Erase(std::uint64_t guid)
        {
            _facts.erase(guid);
            for (auto it = _groups.begin(); it != _groups.end();)
            {
                if (std::find(it->second.begin(), it->second.end(), guid) != it->second.end())
                    it = _groups.erase(it);
                else
                    ++it;
            }
        }
        // F-2: a member added to a party that IP already tracks while Core's
        // role check is still running. Core queues the party afterwards
        // with its stored role-check set, so the record made at join would
        // miss this member. Appends the member (once) and records its facts.
        // An untracked party is never created here. Returns true if applied.
        bool RefreshGroupMember(std::uint64_t groupGuid, std::uint64_t member, QueueFacts facts)
        {
            if (!groupGuid || !member)
                return false;
            auto const party = _groups.find(groupGuid);
            if (party == _groups.end())
                return false;
            if (std::find(party->second.begin(), party->second.end(), member) == party->second.end())
                party->second.push_back(member);
            _facts[member] = facts;
            return true;
        }
        bool TracksGroup(std::uint64_t groupGuid) const { return _groups.count(groupGuid) != 0; }
        // Group disbanded: its GUID is never reused, so the record can go.
        void EraseGroup(std::uint64_t groupGuid) { _groups.erase(groupGuid); }
        std::size_t Size() const { return _facts.size(); }
        std::size_t Groups() const { return _groups.size(); }

        // True when this candidate group must not be formed. `guids` may
        // mix player GUIDs and recorded party GUIDs.
        template<typename GuidRange>
        bool Rejects(bool optionEnabled, GuidRange const& guids) const
        {
            bool vanillaHuman = false;
            bool restrictedBot = false;
            auto const consider = [&](std::uint64_t guid)
            {
                auto const found = _facts.find(guid);
                if (found == _facts.end())
                    return; // unknown: no facts, never restricts
                vanillaHuman = vanillaHuman || found->second.stillVanillaHuman;
                restrictedBot = restrictedBot || found->second.restrictedBot;
            };
            for (std::uint64_t guid : guids)
            {
                consider(guid);
                auto const party = _groups.find(guid);
                if (party != _groups.end())
                    for (std::uint64_t member : party->second)
                        consider(member);
            }
            return QueueGroupRejected(optionEnabled, vanillaHuman, restrictedBot);
        }

    private:
        std::unordered_map<std::uint64_t, QueueFacts> _facts;
        std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> _groups;
    };

    // Battlegrounds (owner decision 13 Nov 2026, option (c)): restricted
    // RandomBots are kept out of the queue and battle of a still-Vanilla
    // human. (b) A restricted bot's solo queue join is refused while a
    // still-Vanilla human waits in a matching queue and bracket. (a) A
    // restricted bot's "enter battle" is refused when it is invited to an
    // instance that already holds a still-Vanilla human. TBC/Wrath humans
    // are unaffected; leaving a queue is never refused.
    constexpr bool BgQueueMatches(std::uint32_t botQueue, std::uint32_t humanQueue, std::uint32_t randomQueue,
                                  std::uint32_t botBracket, std::uint32_t humanBracket)
    {
        return botBracket == humanBracket &&
            (botQueue == humanQueue || botQueue == randomQueue || humanQueue == randomQueue);
    }

    constexpr bool BgJoinRefused(bool optionEnabled, bool restrictedBot, bool soloJoin,
                                 bool matchingVanillaHumanQueued)
    {
        return optionEnabled && restrictedBot && soloJoin && matchingVanillaHumanQueued;
    }

    constexpr bool BgPortRefused(bool optionEnabled, bool restrictedBot, bool enteringBattle,
                                 bool invitedToInstanceWithVanillaHuman)
    {
        return optionEnabled && restrictedBot && enteringBattle && invitedToInstanceWithVanillaHuman;
    }

    // Mail to an OFFLINE receiver (owner-approved): refused only when that
    // GUID was positively confirmed as a restricted RandomBot earlier in this
    // server run (recorded at its logout). A bot not seen since the last
    // restart is unknown and never restricted. No reply: an offline bot has
    // no live object to answer with.
    // F-2 gate: refresh only on the world thread, only while the option is
    // on, only a party IP already tracks, and only during Core's role check
    // (a QUEUED party is removed from the queue by Core on any add).
    constexpr bool RoleCheckMemberRefresh(bool optionEnabled, bool worldThread, bool tracked,
                                          bool inRoleCheck)
    {
        return optionEnabled && worldThread && tracked && inRoleCheck;
    }

    // ---- Guild policy (owner Option B, 14 Nov 2026; Core #28 seams) ----
    // A still-Vanilla human (real client) and a verified restricted
    // RandomBot do not share guild membership or guild chat.

    // Admission, before Core persists anything: a restricted RandomBot is
    // refused by a guild that has an ONLINE still-Vanilla human member.
    // (A human joining is never refused; the restricted bots leave instead.)
    constexpr bool GuildAdmissionRefused(bool optionEnabled, bool joinerRestrictedBot,
                                         bool anyOnlineStillVanillaHumanMember)
    {
        return optionEnabled && joinerRestrictedBot && anyOnlineStillVanillaHumanMember;
    }

    // A member is restricted when online live facts say so, or, while
    // offline, when it was confirmed at its last logout this run.
    constexpr bool GuildMemberRestricted(bool online, bool onlineRestrictedBot, bool offlineConfirmed)
    {
        return online ? onlineRestrictedBot : offlineConfirmed;
    }

    // Remediation is started only by a still-Vanilla human (real client) in
    // a guild, on the world thread: at that human's join or login (owner
    // decisions). Era regression adds nothing: the next login does it.
    constexpr bool GuildRemediationTriggered(bool optionEnabled, bool worldThread, bool inGuild,
                                             bool stillVanillaHuman)
    {
        return optionEnabled && worldThread && inGuild && stillVanillaHuman;
    }

    // Re-proven immediately before EVERY removal: the guild exists, the
    // target is still a member, another member survives (so Core's
    // DeleteMember can never disband the guild, whatever its flags), and
    // the reason holds (the triggering human is still a member).
    constexpr bool GuildRemovalSafe(bool guildExists, bool targetIsMember, std::size_t memberCount,
                                    bool reasonStillHolds)
    {
        return guildExists && targetIsMember && memberCount >= 2 && reasonStillHolds;
    }

    // Remediation work for the world update. Never drops: (guild, human)
    // keys coalesce, so it holds at most one entry per triggering
    // join/login between two world updates, and every update empties it.
    class PendingGuildRemediation
    {
    public:
        void Mark(std::uint32_t guildId, std::uint64_t human)
        {
            if (guildId && human)
                _work.insert({guildId, human});
        }
        template<typename Fn>
        void Drain(Fn fn)
        {
            std::set<std::pair<std::uint32_t, std::uint64_t>> work;
            work.swap(_work);
            for (auto const& [guildId, human] : work)
                fn(guildId, human);
        }
        std::size_t Size() const { return _work.size(); }

    private:
        std::set<std::pair<std::uint32_t, std::uint64_t>> _work;
    };

    constexpr bool OfflineMailRefused(bool optionEnabled, bool senderStillVanillaHuman,
                                      bool receiverConfirmedRestrictedBot)
    {
        return optionEnabled && senderStillVanillaHuman && receiverConfirmedRestrictedBot;
    }

    class ConfirmedRestrictedBots
    {
    public:
        void Remember(std::uint64_t guid)
        {
            if (guid)
                _guids.insert(guid);
        }
        // At every login: the live facts take over while online, and the
        // next logout re-remembers it only if it is still a verified
        // restricted bot. So the set always reflects the LAST logout.
        void Forget(std::uint64_t guid) { _guids.erase(guid); }
        bool Contains(std::uint64_t guid) const { return _guids.count(guid) != 0; }
        std::size_t Size() const { return _guids.size(); }

    private:
        std::unordered_set<std::uint64_t> _guids;
    };

    // Option (d) (owner-approved): the battleground matcher itself never puts
    // a still-Vanilla human and a restricted RandomBot into the same match,
    // whichever queued or entered first. A candidate group is refused when
    // it would meet the other kind among the groups already selected for
    // this match (either team) or the players already in / invited to the
    // target instance.
    struct MatchKinds
    {
        bool stillVanillaHuman = false;
        bool restrictedBot = false;

        void Add(MatchKinds other)
        {
            stillVanillaHuman = stillVanillaHuman || other.stillVanillaHuman;
            restrictedBot = restrictedBot || other.restrictedBot;
        }
        constexpr bool Any() const { return stillVanillaHuman || restrictedBot; }
    };

    constexpr MatchKinds KindsOf(Human const& asHuman, Bot const& asBot)
    {
        return MatchKinds{HumanStillVanilla(asHuman), BotIsRestrictedRandomBot(asBot)};
    }

    constexpr bool MatchPoolRejects(bool optionEnabled, MatchKinds candidate, MatchKinds others)
    {
        return optionEnabled &&
            ((candidate.restrictedBot && others.stillVanillaHuman) ||
             (candidate.stillVanillaHuman && others.restrictedBot));
    }

    // Instance id -> still-Vanilla humans inside (recorded on the world
    // thread as they enter; removed as they leave). Values only.
    class BgVanillaRoster
    {
    public:
        void Add(std::uint32_t instance, std::uint64_t guid)
        {
            if (instance && guid)
                _inside[instance].insert(guid);
        }
        void Remove(std::uint32_t instance, std::uint64_t guid)
        {
            auto const found = _inside.find(instance);
            if (found == _inside.end())
                return;
            found->second.erase(guid);
            if (found->second.empty())
                _inside.erase(found);
        }
        void Clear(std::uint32_t instance) { _inside.erase(instance); }
        bool HasVanillaHuman(std::uint32_t instance) const { return _inside.count(instance) != 0; }
        std::size_t Instances() const { return _inside.size(); }

        // True if `invitedTo(instance)` holds for any instance with a
        // still-Vanilla human inside.
        template<typename InvitedTo>
        bool AnyInvitedInstanceHasVanillaHuman(InvitedTo invitedTo) const
        {
            for (auto const& entry : _inside)
                if (invitedTo(entry.first))
                    return true;
            return false;
        }

    private:
        std::unordered_map<std::uint32_t, std::unordered_set<std::uint64_t>> _inside;
    };

    // Still-Vanilla humans who joined a battleground queue: GUID -> where.
    // Entries are re-validated live (world thread) before use and pruned.
    struct BgQueuedHuman
    {
        std::uint32_t bgType = 0;
        std::uint32_t queue = 0;
        std::uint32_t bracket = 0;
    };

    class BgQueuedHumans
    {
    public:
        void Record(std::uint64_t guid, BgQueuedHuman where)
        {
            if (guid)
                _queued[guid] = where;
        }
        void Erase(std::uint64_t guid) { _queued.erase(guid); }
        std::size_t Size() const { return _queued.size(); }

        // `stillQueuedVanilla(guid, where)` re-validates an entry (and is
        // where stale ones are noticed); `matches(where)` compares it with
        // the bot's queue and bracket. Stale entries are erased.
        template<typename StillValid, typename Matches>
        bool AnyMatching(StillValid stillQueuedVanilla, Matches matches)
        {
            bool found = false;
            for (auto it = _queued.begin(); it != _queued.end();)
            {
                if (!stillQueuedVanilla(it->first, it->second))
                {
                    it = _queued.erase(it);
                    continue;
                }
                found = found || matches(it->second);
                ++it;
            }
            return found;
        }

    private:
        std::unordered_map<std::uint64_t, BgQueuedHuman> _queued;
    };

    // T1 owned-map list helpers. Generic so the exact code also runs in the
    // native tests with fake types. `players` is the owned map's own player
    // list (elements expose GetSource()).
    //
    // Membership is pointer identity only: the candidate is never touched.
    template<typename PlayerList, typename Candidate>
    bool OwnedListContains(PlayerList const& players, Candidate const* candidate)
    {
        if (!candidate)
            return false;
        for (auto const& ref : players)
            if (ref.GetSource() == candidate)
                return true;
        return false;
    }

    // Resolve a GUID among the owned map's players only (their state may be
    // read: this thread owns them). Never consults anything else.
    template<typename PlayerT, typename PlayerList, typename Guid>
    PlayerT* OwnedListResolve(PlayerList const& players, Guid const& guid)
    {
        for (auto const& ref : players)
            if (PlayerT* owned = ref.GetSource())
                if (owned->GetGUID() == guid)
                    return owned->IsInWorld() ? owned : nullptr;
        return nullptr;
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
