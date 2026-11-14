/*
 * CosmicCuddle IP F14 social isolation (DEFAULT OFF).
 *
 * While an ordinary human is still in Vanilla, a positively verified Blood
 * Elf/Draenei RandomBot declines whisper, trade and group with that human and
 * is left out of the human's /who. When the human starts the interaction,
 * the bot answers once (rate-limited) with an in-character whisper.
 *
 * These are new, separate scripts: Core ANDs boolean hooks across scripts,
 * so every existing IP hook keeps its behaviour. Nothing here hides anyone
 * in the world, changes a stage, queries a database or asks Playerbots.
 * Identity comes only from IP's login-thread verified RandomBot cache.
 *
 * Every hook below runs in the initiating player's session handler, where
 * Core has itself just resolved and read the other Player. The reply is a
 * packet built here and sent to the human's own session only; it never goes
 * through Player::Whisper, so it cannot trip this block or bot chat logic.
 */
#include "IndividualProgression.h"
#include "IndividualProgressionF14SocialIsolationPolicy.h"
#include "AccountMgr.h"
#include "AllBattlegroundScript.h"
#include "AllSpellScript.h"
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "BattlegroundQueue.h"
#include "Chat.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GlobalScript.h"
#include "Group.h"
#include "GroupScript.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GuildScript.h"
#include "LFG.h"
#include "LFGMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "WorldObjectScript.h"
#include "WorldPacket.h"
#include "WorldScript.h"
#include "WorldSession.h"
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace
{
    namespace Policy = IndividualProgressionF14SocialIsolationPolicy;

    static_assert(RACE_BLOODELF == Policy::BloodElfRace && RACE_DRAENEI == Policy::DraeneiRace,
                  "F14 social isolation race identifiers changed");
    static_assert(PROGRESSION_PRE_TBC == Policy::FirstTbcStage,
                  "F14 social isolation Vanilla/TBC boundary changed");
    static_assert(PROGRESSION_WOTLK_TIER_5 == Policy::LastSupportedStage,
                  "F14 social isolation last stage changed");

    bool OptionOn()
    {
        return sIndividualProgression->enabled && sIndividualProgression->f14RandomBotSocialIsolation;
    }

    // Const mirror of IndividualProgression::GetPlayerProgressionFromQuests:
    // the highest rewarded hidden progression quest (66000 + stage). In-memory
    // quest status only; no paid/account lookup. Out of world = unknown.
    uint8 ReadRawStage(Player const* player)
    {
        if (!player || !player->IsInWorld())
            return 255;
        uint8 stage = 0;
        for (uint8 i = PROGRESSION_MOLTEN_CORE; i <= PROGRESSION_WOTLK_TIER_5; ++i)
            if (player->GetQuestStatus(66000 + i) == QUEST_STATUS_REWARDED)
                stage = i;
        return stage;
    }

    Policy::Human ReadHuman(Player const* player)
    {
        Policy::Human h;
        if (!player)
            return h;
        WorldSession const* session = player->GetSession();
        h.present = true;
        h.inWorld = player->IsInWorld();
        h.nonHeadlessSession = session && !session->IsHeadless();
        h.playerAccountSecurity = session && AccountMgr::IsPlayerAccount(session->GetSecurity());
        h.gameMasterMode = player->IsGameMaster();
        h.verifiedRandomBot = sIndividualProgression->HasF14VerifiedRandomBotIdentity(player);
        h.rawStage = ReadRawStage(player);
        return h;
    }

    Policy::Bot ReadBot(Player const* player)
    {
        Policy::Bot b;
        if (!player)
            return b;
        WorldSession const* session = player->GetSession();
        b.present = true;
        b.inWorld = player->IsInWorld();
        b.verifiedRandomBotIdentity = sIndividualProgression->HasF14VerifiedRandomBotIdentity(player);
        b.headlessSession = session && session->IsHeadless();
        b.gameMasterMode = player->IsGameMaster();
        b.race = player->getRace();
        return b;
    }

    std::mutex& ReplyLock()
    {
        static std::mutex lock;
        return lock;
    }

    Policy::ReplyLimiter<1024>& ReplyLimiter()
    {
        static Policy::ReplyLimiter<1024> limiter;
        return limiter;
    }

    // One in-character whisper from the bot to the human, rate-limited per
    // pair. Sent only to the human's own session; nothing is written to the
    // bot. An empty configured reply disables the answer, not the block.
    Policy::WorldThreadMarker& WorldThread();

    std::mutex& PendingRepliesLock()
    {
        static std::mutex lock;
        return lock;
    }

    Policy::PendingReplies<256>& PendingReplies()
    {
        static Policy::PendingReplies<256> pending;
        return pending;
    }

    // Build and send one reply. World thread only.
    void SendReplyPacket(Player const* bot, Player* human)
    {
        // One line, bounded length; empty means decline silently.
        std::string const text = Policy::NormalizeReply(sIndividualProgression->GetF14SocialIsolationReply());
        if (text.empty())
            return;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_UNIVERSAL, bot, human, text);
        human->SendDirectMessage(&data);
    }

    // World thread: send every reply that was decided elsewhere. Both
    // players are looked up again; anyone gone simply gets no reply.
    void SendPendingReplies()
    {
        Policy::PendingReplies<256> due;
        {
            std::lock_guard<std::mutex> lock(PendingRepliesLock());
            PendingReplies().Drain([&due](uint64 human, uint64 bot) { due.Push(human, bot); });
        }
        due.Drain([](uint64 humanGuid, uint64 botGuid)
        {
            Player* human = ObjectAccessor::FindPlayer(ObjectGuid(humanGuid));
            Player const* bot = ObjectAccessor::FindPlayer(ObjectGuid(botGuid));
            if (human && bot && human->GetSession())
                SendReplyPacket(bot, human);
        });
    }

    void SendInCharacterReply(Player const* bot, Player* human)
    {
        if (!bot || !human || !human->GetSession())
            return;
        bool mayReply = false;
        {
            std::lock_guard<std::mutex> lock(ReplyLock());
            mayReply = ReplyLimiter().MayReply(human->GetGUID().GetRawValue(), bot->GetGUID().GetRawValue(),
                static_cast<uint64>(GameTime::GetGameTime().count()),
                sIndividualProgression->f14RandomBotSocialIsolationReplyCooldownSeconds);
        }
        if (!mayReply)
            return;
        // Packets are sent only from the world thread; a reply decided on a
        // map thread waits for the world update (the cooldown is already
        // spent, so it is still at most one reply per window).
        if (!WorldThread().IsCurrent())
        {
            std::lock_guard<std::mutex> lock(PendingRepliesLock());
            PendingReplies().Push(human->GetGUID().GetRawValue(), bot->GetGUID().GetRawValue());
            return;
        }
        SendReplyPacket(bot, human);
    }

    // Dungeon Finder facts captured at queue join, by GUID (see policy).
#if defined(COSMIC_F14_GUILD_POLICY_HOOK_VERSION) && COSMIC_F14_GUILD_POLICY_HOOK_VERSION >= 1
    // Guild remediation work (values only) for the world update.
    std::mutex& GuildRemediationLock()
    {
        static std::mutex lock;
        return lock;
    }

    Policy::PendingGuildRemediation& GuildRemediation()
    {
        static Policy::PendingGuildRemediation work;
        return work;
    }

    // Adds Core performed off the world thread: (guild, joiner) values,
    // re-checked at the next world update. Same never-dropping set type.
    Policy::PendingGuildRemediation& GuildLateJoins()
    {
        static Policy::PendingGuildRemediation work;
        return work;
    }

    // World thread only: a still-Vanilla human in a guild (join or login)
    // asks the next world update to remove the restricted bots.
    void MarkGuildRemediation(Player* player)
    {
        if (!OptionOn() || !player || !WorldThread().IsCurrent())
            return;
        uint32 const guildId = player->GetGuildId();
        bool const human = Policy::HumanStillVanilla(ReadHuman(player));
        if (!Policy::GuildRemediationTriggered(true, true, guildId != 0, human))
            return;
        std::lock_guard<std::mutex> lock(GuildRemediationLock());
        GuildRemediation().Mark(guildId, player->GetGUID().GetRawValue());
    }
#endif

    std::mutex& QueueFactsLock()
    {
        static std::mutex lock;
        return lock;
    }

    Policy::QueueFactsTable& QueueFacts()
    {
        static Policy::QueueFactsTable table;
        return table;
    }

    // Battleground facts (owner decision, option (c)); values only.
    std::mutex& BgLock()
    {
        static std::mutex lock;
        return lock;
    }

    Policy::BgVanillaRoster& BgRoster()
    {
        static Policy::BgVanillaRoster roster;
        return roster;
    }

    Policy::BgQueuedHumans& BgQueued()
    {
        static Policy::BgQueuedHumans queued;
        return queued;
    }

    // World thread only (BattlegroundMgr data): the queue and level bracket
    // a player of this level would wait in for this battleground type.
    bool BgQueueAndBracket(BattlegroundTypeId bgTypeId, uint32 level, uint32& queue, uint32& bracket)
    {
        Battleground* templ = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);
        if (!templ)
            return false;
        PvPDifficultyEntry const* entry = GetBattlegroundBracketByLevel(templ->GetMapId(), level);
        if (!entry)
            return false;
        queue = uint32(BattlegroundMgr::BGQueueTypeId(bgTypeId, 0));
        bracket = uint32(entry->GetBracketId());
        return true;
    }

    // Restricted RandomBots confirmed earlier in this run (offline mail).
    std::mutex& ConfirmedLock()
    {
        static std::mutex lock;
        return lock;
    }

    Policy::ConfirmedRestrictedBots& Confirmed()
    {
        static Policy::ConfirmedRestrictedBots confirmed;
        return confirmed;
    }

    Policy::WorldThreadMarker& WorldThread()
    {
        static Policy::WorldThreadMarker marker;
        return marker;
    }

    // T1: the map this thread owns in the current world tick, recorded at
    // the start of each player update it runs (that player's own map).
    struct OwnedMapRecord
    {
        Map const* map = nullptr;
        uint64 tick = 0;
        bool recorded = false;
    };

    OwnedMapRecord& ThisThreadOwnedMap()
    {
        thread_local OwnedMapRecord record;
        return record;
    }

    // What this thread may read. On the world thread: any player. Off it:
    // only players whose POINTER is in the owned map's player list; the
    // candidate Player object itself is never touched to decide that.
    struct ReadScope
    {
        bool anyPlayer = false;
        Map const* ownedMap = nullptr;

        bool Contains(Player const* candidate) const
        {
            if (!candidate)
                return false;
            if (anyPlayer)
                return true;
            if (!ownedMap)
                return false;
            return Policy::OwnedListContains(ownedMap->GetPlayers(), candidate);
        }

        // Off the world thread: a player name only among the owned map's
        // players (never the global name map). Exact, case-sensitive match,
        // as Core's name map uses the normalized name.
        Player* ResolveByName(std::string const& name) const
        {
            if (!ownedMap || name.empty())
                return nullptr;
            for (auto const& ref : ownedMap->GetPlayers())
                if (Player* owned = ref.GetSource())
                    if (owned->GetName() == name)
                        return owned->IsInWorld() ? owned : nullptr;
            return nullptr;
        }

        // Members and leaders: off the world thread, only from the owned
        // map's own list (no global lookup that touches foreign players).
        Player* Resolve(ObjectGuid const& guid) const
        {
            if (anyPlayer)
                return ObjectAccessor::FindPlayer(guid);
            if (!ownedMap || !guid)
                return nullptr;
            return Policy::OwnedListResolve<Player>(ownedMap->GetPlayers(), guid);
        }
    };

    ReadScope CurrentReadScope()
    {
        ReadScope scope;
        if (WorldThread().IsCurrent())
        {
            scope.anyPlayer = true;
            return scope;
        }
        OwnedMapRecord const& owned = ThisThreadOwnedMap();
        if (Policy::OwnedScopeValid(owned.recorded && owned.map, owned.tick, WorldThread().Tick()))
            scope.ownedMap = owned.map;
        return scope;
    }

    bool MayReadPair(ReadScope const& scope, Player const* a, Player const* b)
    {
        return Policy::MayReadPair(scope.anyPlayer, scope.ownedMap != nullptr,
                                   scope.Contains(a), scope.Contains(b));
    }

    // initiator -> other. Returns true to ALLOW. Either side may be the bot.
    bool AllowPairIn(ReadScope const& scope, Policy::Surface surface, Player* initiator, Player* other,
                     bool addonMessage)
    {
        if (!OptionOn() || !initiator || !other || initiator == other)
            return true;
        if (!MayReadPair(scope, initiator, other))
            return true; // T1: unknown facts, not restricted (stock)

        // Human started it: block and (maybe) answer from the bot.
        Policy::Decision const fromHuman = Policy::Decide(true, surface, Policy::Direction::HumanToBot,
            ReadHuman(initiator), ReadBot(other), addonMessage);
        if (fromHuman.block)
        {
            if (fromHuman.reply)
                SendInCharacterReply(other, initiator);
            return false;
        }

        // Bot started it: block quietly.
        Policy::Decision const fromBot = Policy::Decide(true, surface, Policy::Direction::BotToHuman,
            ReadHuman(other), ReadBot(initiator), addonMessage);
        return !fromBot.block;
    }

    bool AllowPair(Policy::Surface surface, Player* initiator, Player* other, bool addonMessage)
    {
        return AllowPairIn(CurrentReadScope(), surface, initiator, other, addonMessage);
    }

    class F14SocialIsolationPlayerScript final : public PlayerScript
    {
    public:
        F14SocialIsolationPlayerScript() : PlayerScript("IndividualProgression_F14SocialIsolation",
            {PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT, PLAYERHOOK_CAN_INIT_TRADE,
             PLAYERHOOK_CAN_GROUP_INVITE, PLAYERHOOK_CAN_GROUP_ACCEPT,
             PLAYERHOOK_ON_BEFORE_UPDATE, PLAYERHOOK_CAN_JOIN_LFG, PLAYERHOOK_ON_LOGOUT,
             PLAYERHOOK_CAN_JOIN_IN_BATTLEGROUND_QUEUE, PLAYERHOOK_CAN_BATTLEFIELD_PORT,
             PLAYERHOOK_CAN_SEND_MAIL, PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_CREATE,
             PLAYERHOOK_ON_DELETE}) { }

        // Offline mail: a character that logs in is no longer "remembered";
        // its next logout re-remembers it only if still a verified
        // restricted bot (so a revoked identity is not kept until restart).
        void OnPlayerLogin(Player* player) override
        {
            if (!player)
                return;
            {
                std::lock_guard<std::mutex> lock(ConfirmedLock());
                Confirmed().Forget(player->GetGUID().GetRawValue());
            }
#if defined(COSMIC_F14_GUILD_POLICY_HOOK_VERSION) && COSMIC_F14_GUILD_POLICY_HOOK_VERSION >= 1
            // Owner decision (14 Nov 2026): a still-Vanilla human's login to
            // a guild removes its restricted bots (next world update).
            MarkGuildRemediation(player);
#endif
        }

        // F-3: a created or deleted character never keeps an offline-mail
        // entry for its GUID, so the set no longer depends on GUIDs never
        // being reused (cluster mode). Cleanup only: runs with the option
        // OFF too and never classifies anyone. Core calls OnPlayerDelete
        // only on its accepted deletion path (refusals go to
        // OnPlayerFailedDelete), and OnPlayerCreate after the creation
        // transaction succeeded.
        void OnPlayerCreate(Player* player) override
        {
            if (!player)
                return;
            std::lock_guard<std::mutex> lock(ConfirmedLock());
            Confirmed().Forget(player->GetGUID().GetRawValue());
        }

        void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
        {
            std::lock_guard<std::mutex> lock(ConfirmedLock());
            Confirmed().Forget(guid.GetRawValue());
        }

        // Mail. CMSG_SEND_MAIL is PROCESS_THREADUNSAFE (world thread), so the
        // receiver may be looked up. Only an ONLINE receiver has live facts;
        // an offline one is unknown and never restricted. Core also calls
        // this when a mail is returned to its sender (same arguments).
        bool OnPlayerCanSendMail(Player* player, ObjectGuid receiverGuid, ObjectGuid /*mailbox*/,
                                 std::string& /*subject*/, std::string& /*body*/, uint32 /*money*/,
                                 uint32 /*COD*/, Item* /*item*/) override
        {
            if (!OptionOn() || !player || !WorldThread().IsCurrent())
                return true;
            Player* receiver = ObjectAccessor::FindPlayer(receiverGuid);
            if (receiver)
                return AllowPair(Policy::Surface::Mail, player, receiver, false);
            // Offline: refused only for a bot confirmed earlier in this run.
            bool confirmed = false;
            {
                std::lock_guard<std::mutex> lock(ConfirmedLock());
                confirmed = Confirmed().Contains(receiverGuid.GetRawValue());
            }
            return !Policy::OfflineMailRefused(true, Policy::HumanStillVanilla(ReadHuman(player)), confirmed);
        }

        // Battleground queue join (b). CMSG_BATTLEMASTER_JOIN is
        // PROCESS_THREADUNSAFE and Playerbots queues the same packet, so this
        // runs on the world thread. Vanilla humans (and their group's Vanilla
        // humans) are recorded; a restricted bot's SOLO join is refused while a
        // still-Vanilla human waits in a matching queue and bracket. Group
        // joins are never refused here: the port check (a) covers them.
        bool OnPlayerCanJoinInBattlegroundQueue(Player* player, ObjectGuid /*battlemaster*/, BattlegroundTypeId bgTypeId,
                                                uint8 joinAsGroup, GroupJoinBattlegroundResult& err) override
        {
            if (!OptionOn() || !player || !WorldThread().IsCurrent())
                return true;
            std::vector<Player*> joiners{player};
            if (joinAsGroup)
                if (Group const* group = player->GetGroup())
                    for (GroupReference const* itr = group->GetFirstMember(); itr; itr = itr->next())
                        if (Player* member = itr->GetSource())
                            if (member != player)
                                joiners.push_back(member);

            std::lock_guard<std::mutex> lock(BgLock());
            for (Player* joiner : joiners)
            {
                if (!Policy::HumanStillVanilla(ReadHuman(joiner)))
                    continue;
                Policy::BgQueuedHuman where;
                where.bgType = uint32(bgTypeId);
                if (BgQueueAndBracket(bgTypeId, joiner->GetLevel(), where.queue, where.bracket))
                    BgQueued().Record(joiner->GetGUID().GetRawValue(), where);
            }

            bool const solo = !joinAsGroup || !player->GetGroup();
            bool const restricted = Policy::BotIsRestrictedRandomBot(ReadBot(player));
            if (!restricted || !solo)
                return true;
            uint32 botQueue = 0;
            uint32 botBracket = 0;
            if (!BgQueueAndBracket(bgTypeId, player->GetLevel(), botQueue, botBracket))
                return true;
            uint32 const randomQueue = uint32(BattlegroundMgr::BGQueueTypeId(BATTLEGROUND_RB, 0));
            uint32 const botLevel = player->GetLevel();
            bool const matching = BgQueued().AnyMatching(
                [](uint64 guid, Policy::BgQueuedHuman const& where)
                {
                    // Live re-validation on the world thread: still online,
                    // still in that queue, still Vanilla.
                    Player const* human = ObjectAccessor::FindPlayer(ObjectGuid(guid));
                    return human &&
                        human->InBattlegroundQueueForBattlegroundQueueType(BattlegroundQueueTypeId(where.queue)) &&
                        Policy::HumanStillVanilla(ReadHuman(human));
                },
                [&](Policy::BgQueuedHuman const& where)
                {
                    uint32 queue = 0;
                    uint32 bracket = 0;
                    // The bot's bracket on the human's battleground map.
                    if (!BgQueueAndBracket(BattlegroundTypeId(where.bgType), botLevel, queue, bracket))
                        return false;
                    return Policy::BgQueueMatches(botQueue, where.queue, randomQueue, bracket, where.bracket);
                });
            if (!Policy::BgJoinRefused(true, restricted, solo, matching))
                return true;
            err = ERR_BATTLEGROUND_NONE; // quiet refusal (<= 0 stops Core)
            return false;
        }

        // Battleground "enter battle" (a). Playerbots calls the port handler
        // from bot AI (a map thread), so only the bot's own state is read
        // (after the T1 ownership proof) plus the value-only roster. Leaving
        // a queue (action 0) is never refused.
        bool OnPlayerCanBattleFieldPort(Player* player, uint8 /*arenaType*/, BattlegroundTypeId /*bgTypeId*/,
                                        uint8 action) override
        {
            if (!OptionOn() || !player || action != 1)
                return true;
            if (!CurrentReadScope().Contains(player))
                return true; // T1: not provably ours to read
            bool const restricted = Policy::BotIsRestrictedRandomBot(ReadBot(player));
            if (!restricted)
                return true;
            bool invitedToVanilla = false;
            {
                std::lock_guard<std::mutex> lock(BgLock());
                invitedToVanilla = BgRoster().AnyInvitedInstanceHasVanillaHuman(
                    [player](uint32 instance) { return player->IsInvitedForBattlegroundInstance(instance); });
            }
            return !Policy::BgPortRefused(true, restricted, true, invitedToVanilla);
        }

        // Dungeon Finder: record the joiner's (and its group's) facts. A
        // client or bot CMSG_LFG_JOIN is PROCESS_THREADUNSAFE, so this runs on
        // the world thread where any player may be read; anywhere else
        // nothing is recorded (unknown: never restricts). Never blocks.
        bool OnPlayerCanJoinLfg(Player* player, uint8 /*roles*/, std::set<uint32>& /*dungeons*/,
                                std::string const& /*comment*/) override
        {
            if (!OptionOn() || !player || !WorldThread().IsCurrent())
                return true;
            std::vector<std::pair<uint64, Policy::QueueFacts>> facts;
            facts.emplace_back(player->GetGUID().GetRawValue(),
                               Policy::MakeQueueFacts(ReadHuman(player), ReadBot(player)));
            uint64 groupGuid = 0;
            std::vector<uint64> members{player->GetGUID().GetRawValue()};
            if (Group const* group = player->GetGroup())
            {
                groupGuid = group->GetGUID().GetRawValue();
                for (GroupReference const* itr = group->GetFirstMember(); itr; itr = itr->next())
                    if (Player const* member = itr->GetSource())
                        if (member != player)
                        {
                            facts.emplace_back(member->GetGUID().GetRawValue(),
                                               Policy::MakeQueueFacts(ReadHuman(member), ReadBot(member)));
                            members.push_back(member->GetGUID().GetRawValue());
                        }
            }
            std::lock_guard<std::mutex> lock(QueueFactsLock());
            for (auto const& [guid, value] : facts)
                QueueFacts().Record(guid, value);
            if (groupGuid)
                QueueFacts().RecordGroup(groupGuid, std::move(members));
            return true;
        }

        void OnPlayerLogout(Player* player) override
        {
            if (!player)
                return;
            // Remember a confirmed restricted RandomBot for offline mail. This
            // script is registered before IP's player script (loader order),
            // so the verified identity is still present here. World thread
            // only; anywhere else nothing is remembered (unknown stays so).
            if (OptionOn() && WorldThread().IsCurrent() && Policy::BotIsRestrictedRandomBot(ReadBot(player)))
            {
                std::lock_guard<std::mutex> lock(ConfirmedLock());
                Confirmed().Remember(player->GetGUID().GetRawValue());
            }
            {
                std::lock_guard<std::mutex> lock(QueueFactsLock());
                QueueFacts().Erase(player->GetGUID().GetRawValue());
            }
            std::lock_guard<std::mutex> lock(BgLock());
            BgQueued().Erase(player->GetGUID().GetRawValue());
        }

        // T1 only: this thread is about to update `player`, so it owns that
        // player's map for the rest of this world tick. Records, reads no
        // other player.
        void OnPlayerBeforeUpdate(Player* player, uint32 /*p_time*/) override
        {
            if (!OptionOn() || !player)
                return;
            ThisThreadOwnedMap() = {player->FindMap(), WorldThread().Tick(), true};
        }

        using PlayerScript::OnPlayerCanUseChat;

        // Reached from Player::Whisper, after Core's receiver, whisper-filter
        // and faction checks.
        bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 language,
                                std::string& /*msg*/, Player* receiver) override
        {
            if (type != CHAT_MSG_WHISPER)
                return true;
            return AllowPair(Policy::Surface::Whisper, player, receiver, language == LANG_ADDON);
        }

        // Reached after Core's faction, distance and state checks.
        bool OnPlayerCanInitTrade(Player* player, Player* target) override
        {
            return AllowPair(Policy::Surface::Trade, player, target, false);
        }

        // A client CMSG_GROUP_INVITE is PROCESS_THREADUNSAFE (world thread),
        // where the global name lookup is safe. Playerbots may call the
        // handler from bot AI on a map thread (CS01 6865093100): there IP
        // never uses the global name map and resolves the name only among
        // the owned map's players. Core's own earlier lookup on that route is
        // a cross-repository issue (Playerbots dispatch), not fixed here.
        bool OnPlayerCanGroupInvite(Player* player, std::string& membername) override
        {
            if (!OptionOn() || !player)
                return true;
            ReadScope const scope = CurrentReadScope();
            Player* invitee = scope.anyPlayer ? ObjectAccessor::FindPlayerByName(membername, false)
                                              : scope.ResolveByName(membername);
            if (!invitee)
                return true; // unknown target: not ours to decide
            return AllowPairIn(scope, Policy::Surface::GroupInvite, player, invitee, false);
        }

        // Accepting an invitation. A client CMSG_GROUP_ACCEPT is
        // PROCESS_THREADUNSAFE (world thread, no map updating); Playerbots
        // AI calls the same handler from the bot's map update (doc: T1).
        bool OnPlayerCanGroupAccept(Player* player, Group* group) override
        {
            if (!OptionOn() || !player || !group)
                return true;
            ReadScope const scope = CurrentReadScope();
            if (!scope.Contains(player))
                return true; // T1: the acceptor is not ours to read
            // 1. The leader, in both directions (unchanged pair rule).
            Player* leader = scope.Resolve(group->GetLeaderGUID());
            if (leader && !AllowPair(Policy::Surface::GroupAccept, player, leader, false))
                return false;
            // 2. Every other current member, for a human who is still in
            // Vanilla. Unresolved members are skipped: never guessed by race.
            return AllowGroupMembers(scope, player, group);
        }

    private:
        bool AllowGroupMembers(ReadScope const& scope, Player* human, Group const* group)
        {
            Policy::Human const facts = ReadHuman(human);
            if (!Policy::HumanStillVanilla(facts))
                return true;
            std::vector<Player*> players;
            std::vector<Policy::Bot> bots;
            for (Group::MemberSlot const& slot : group->GetMemberSlots())
            {
                if (slot.guid == human->GetGUID() || slot.guid == group->GetLeaderGUID())
                    continue;
                // Off the world thread only members on the owned map
                // resolve; others are never touched (T1).
                Player* member = scope.Resolve(slot.guid);
                if (!member)
                    continue; // offline/unresolved: no facts, never restricted
                players.push_back(member);
                bots.push_back(ReadBot(member));
            }
            std::size_t const index = Policy::FirstRestrictedGroupMember(true, facts, bots);
            if (index >= bots.size())
                return true;
            // Same decision and reply rule as every other surface.
            Policy::Decision const d = Policy::Decide(true, Policy::Surface::GroupAccept,
                Policy::Direction::HumanToBot, facts, bots[index], false);
            if (d.reply)
                SendInCharacterReply(players[index], human);
            return !d.block;
        }
    };

    // Dungeon Finder: refuse a candidate group that would put a still-Vanilla
    // human together with a restricted RandomBot. Core then tries other
    // combinations; TBC/Wrath humans are unaffected. This runs on the LFG
    // worker thread beside the map updates, so it reads NO Player at all:
    // only GUIDs from the proposal and the facts recorded at queue join.
    class F14SocialIsolationLfgScript final : public GlobalScript
    {
    public:
        F14SocialIsolationLfgScript() : GlobalScript("IndividualProgression_F14SocialIsolationLfg",
            {GLOBALHOOK_CAN_CREATE_LFG_PROPOSAL}) { }

        bool CanCreateLfgProposal(lfg::Lfg5Guids const& proposal) override
        {
            if (!OptionOn())
                return true;
            // A full proposal carries no roles map and names a party only by
            // its group GUID; recorded parties are expanded by the table.
            std::vector<uint64> guids;
            if (proposal.roles)
                for (auto const& [guid, role] : *proposal.roles)
                    guids.push_back(guid.GetRawValue());
            for (ObjectGuid const& guid : proposal.guids)
                if (guid)
                    guids.push_back(guid.GetRawValue());
            std::lock_guard<std::mutex> lock(QueueFactsLock());
            return !QueueFacts().Rejects(true, guids);
        }
    };

    // F-2: Core keeps a party in the Dungeon Finder role check when a member
    // is added (only a QUEUED party is removed), then queues it with the
    // stored role-check set. A party IP already tracks gets the new member
    // and its facts here, on the world thread, so the value-only proposal
    // veto sees it. Disband drops the party record (cleanup, also OFF).
    class F14SocialIsolationGroupScript final : public GroupScript
    {
    public:
        F14SocialIsolationGroupScript() : GroupScript("IndividualProgression_F14SocialIsolationGroup",
            {GROUPHOOK_ON_ADD_MEMBER, GROUPHOOK_ON_DISBAND}) { }

        void OnAddMember(Group* group, ObjectGuid guid) override
        {
            if (!OptionOn() || !group || !guid || !WorldThread().IsCurrent())
                return;
            uint64 const groupGuid = group->GetGUID().GetRawValue();
            bool tracked;
            {
                std::lock_guard<std::mutex> lock(QueueFactsLock());
                tracked = QueueFacts().TracksGroup(groupGuid);
            }
            bool const inRoleCheck = tracked && sLFGMgr->GetState(group->GetGUID()) == lfg::LFG_STATE_ROLECHECK;
            if (!Policy::RoleCheckMemberRefresh(true, true, tracked, inRoleCheck))
                return;
            Player const* member = ObjectAccessor::FindPlayer(guid);
            if (!member)
                return;
            Policy::QueueFacts const facts = Policy::MakeQueueFacts(ReadHuman(member), ReadBot(member));
            std::lock_guard<std::mutex> lock(QueueFactsLock());
            QueueFacts().RefreshGroupMember(groupGuid, guid.GetRawValue(), facts);
        }

        void OnDisband(Group* group) override
        {
            if (!group)
                return;
            std::lock_guard<std::mutex> lock(QueueFactsLock());
            QueueFacts().EraseGroup(group->GetGUID().GetRawValue());
        }
    };

    // World thread only: what kind of player a GUID is right now. Offline
    // or unknown players carry no facts and never restrict anyone.
    Policy::MatchKinds KindsOfGuid(ObjectGuid const& guid)
    {
        Player const* player = ObjectAccessor::FindPlayer(guid);
        if (!player)
            return {};
        return Policy::KindsOf(ReadHuman(player), ReadBot(player));
    }

    Policy::MatchKinds KindsOfGroup(GroupQueueInfo const& group)
    {
        Policy::MatchKinds kinds;
        for (ObjectGuid const& guid : group.Players)
            kinds.Add(KindsOfGuid(guid));
        return kinds;
    }

    // Premade path: CheckPremadeMatch asks about the Alliance premade, then
    // the Horde premade, and only then adds both. So a premade candidate is
    // also compared with the premade it would be paired with: the first
    // not-yet-invited group of the other premade queue, exactly as Core
    // picks it. Not a premade candidate: no peer.
    GroupQueueInfo const* PremadePeer(BattlegroundQueue const& queue, GroupQueueInfo const* group,
                                      BattlegroundBracketId bracketId)
    {
        for (uint8 side = 0; side < 2; ++side)
        {
            auto const& own = queue.m_QueuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE + side];
            if (std::find(own.begin(), own.end(), group) == own.end())
                continue;
            for (GroupQueueInfo const* other : queue.m_QueuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE + (1 - side)])
                if (other && !other->IsInvitedToBGInstanceGUID)
                    return other;
            return nullptr;
        }
        return nullptr;
    }

    // Battleground roster (a): which instances hold a still-Vanilla human.
    // Humans enter on their worldport ack, a world-thread client packet, so
    // they are read only there; anything else records nothing. Leaving and
    // instance destruction use only the GUID and instance id values.
    class F14SocialIsolationBattlegroundScript final : public AllBattlegroundScript
    {
    public:
        F14SocialIsolationBattlegroundScript() : AllBattlegroundScript("IndividualProgression_F14SocialIsolationBg",
            {ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_ADD_PLAYER, ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_REMOVE_PLAYER_AT_LEAVE,
             ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY, ALLBATTLEGROUNDHOOK_CAN_ADD_GROUP_TO_MATCHING_POOL}) { }

        // Option (d): the matcher never puts a still-Vanilla human and a
        // restricted RandomBot into one match, whichever came first. Every
        // queue update runs in BattlegroundMgr::Update on the world thread,
        // after the map phase, so players may be read here; anywhere else
        // nothing is read and nothing is refused.
        bool CanAddGroupToMatchingPool(BattlegroundQueue* queue, GroupQueueInfo* group, uint32 /*poolPlayerCount*/,
                                       Battleground* bg, BattlegroundBracketId bracketId) override
        {
            if (!OptionOn() || !queue || !group || !WorldThread().IsCurrent())
                return true;
            Policy::MatchKinds const candidate = KindsOfGroup(*group);
            if (!candidate.Any())
                return true;

            Policy::MatchKinds others;
            // Groups already selected for this match, both teams.
            for (auto const& pool : queue->m_SelectionPools)
                for (GroupQueueInfo const* selected : pool.SelectedGroups)
                    if (selected && selected != group)
                        others.Add(KindsOfGroup(*selected));
            // A premade candidate: the opposite premade it would be paired with.
            if (bracketId < MAX_BATTLEGROUND_BRACKETS)
                if (GroupQueueInfo const* peer = PremadePeer(*queue, group, bracketId))
                    others.Add(KindsOfGroup(*peer));
            // An existing instance: players inside and groups already invited.
            if (bg && bg->GetInstanceID())
            {
                for (auto const& [guid, inside] : bg->GetPlayers())
                    others.Add(KindsOfGuid(guid));
                if (bracketId < MAX_BATTLEGROUND_BRACKETS)
                    for (auto const& waiting : queue->m_QueuedGroups[bracketId])
                        for (GroupQueueInfo const* invited : waiting)
                            if (invited && invited != group && invited->IsInvitedToBGInstanceGUID == bg->GetInstanceID())
                                others.Add(KindsOfGroup(*invited));
            }
            return !Policy::MatchPoolRejects(true, candidate, others);
        }

        void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override
        {
            if (!OptionOn() || !bg || !player || !WorldThread().IsCurrent())
                return;
            if (!Policy::HumanStillVanilla(ReadHuman(player)))
                return;
            std::lock_guard<std::mutex> lock(BgLock());
            BgRoster().Add(bg->GetInstanceID(), player->GetGUID().GetRawValue());
            BgQueued().Erase(player->GetGUID().GetRawValue()); // no longer waiting
        }

        void OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player) override
        {
            if (!bg || !player)
                return;
            std::lock_guard<std::mutex> lock(BgLock());
            BgRoster().Remove(bg->GetInstanceID(), player->GetGUID().GetRawValue());
        }

        void OnBattlegroundDestroy(Battleground* bg) override
        {
            if (!bg)
                return;
            std::lock_guard<std::mutex> lock(BgLock());
            BgRoster().Clear(bg->GetInstanceID());
        }
    };

    // Duel. A duel request is a spell with SPELL_EFFECT_DUEL; this global
    // check needs no spell_script_names row. A client CMSG_CAST_SPELL is
    // PROCESS_THREADSAFE, so it is processed inside the caster's own map
    // update (bot AI casts too): the caster's map is owned here. The target
    // is accepted only by pointer membership in that map's player list, so
    // a foreign object is never touched first. On the world thread any
    // player may be read.
    class F14SocialIsolationSpellScript final : public AllSpellScript
    {
    public:
        F14SocialIsolationSpellScript() : AllSpellScript("IndividualProgression_F14SocialIsolationDuel",
            {ALLSPELLHOOK_ON_SPELL_CHECK_CAST}) { }

        void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& res) override
        {
            if (res != SPELL_CAST_OK || !OptionOn() || !spell)
                return;
            SpellInfo const* info = spell->GetSpellInfo();
            if (!info || !info->HasEffect(SPELL_EFFECT_DUEL))
                return;
            WorldObject* casterObject = spell->GetCaster();
            Player* caster = casterObject ? casterObject->ToPlayer() : nullptr;
            Unit* targetUnit = spell->m_targets.GetUnitTarget();
            if (!caster || !targetUnit)
                return;
            ReadScope scope;
            if (WorldThread().IsCurrent())
                scope.anyPlayer = true;
            else
                scope.ownedMap = caster->FindMap(); // the caster's own map update
            if (!scope.anyPlayer && !scope.ownedMap)
                return;
            // Resolve the target only among the owned map's players.
            Player* target = nullptr;
            if (scope.anyPlayer)
                target = targetUnit->ToPlayer();
            else
                for (auto const& ref : scope.ownedMap->GetPlayers())
                    if (Player* owned = ref.GetSource())
                        if (static_cast<Unit const*>(owned) == targetUnit)
                            target = owned;
            if (!target)
                return; // not a player on this map: not ours to decide
            if (!AllowPairIn(scope, Policy::Surface::Duel, caster, target, false))
                res = SPELL_FAILED_BAD_TARGETS;
        }
    };

#if defined(COSMIC_F14_GUILD_INVITE_HOOK_VERSION) && COSMIC_F14_GUILD_INVITE_HOOK_VERSION >= 1
    // Guild invite (owner-approved Core hook, applied by
    // tests/core_patches/apply_f14_guild_invite_hook.py). Core consults it
    // after its own checks, before the invitation is sent. A client
    // CMSG_GUILD_INVITE is PROCESS_THREADUNSAFE (world thread); any other
    // caller goes through the same T1 ownership proof as every surface.
    // Without the patched Core this whole script is compiled out.
    class F14SocialIsolationGuildScript final : public PlayerScript
    {
    public:
        F14SocialIsolationGuildScript() : PlayerScript("IndividualProgression_F14SocialIsolationGuild",
            {PLAYERHOOK_ON_F14_CAN_GUILD_INVITE}) { }

        bool OnPlayerCanGuildInvite(Player* inviter, Player* invitee) override
        {
            return AllowPair(Policy::Surface::GuildInvite, inviter, invitee, false);
        }
    };
#endif

#if defined(COSMIC_F14_GUILD_POLICY_HOOK_VERSION) && COSMIC_F14_GUILD_POLICY_HOOK_VERSION >= 1
    // Live scan of the members other than `except` (world thread): is a
    // still-Vanilla human online, and which members are restricted bots
    // (online by live facts, offline only if confirmed at their last logout).
    void ScanGuild(Guild const& guild, ObjectGuid except, bool& anyOnlineHuman, std::vector<ObjectGuid>* restricted)
    {
        anyOnlineHuman = false;
        std::vector<ObjectGuid> offline;
        for (auto const& [low, member] : guild.GetUnorderedMembers())
        {
            ObjectGuid const guid = member.GetGUID();
            if (guid == except)
                continue;
            Player const* online = ObjectAccessor::FindPlayer(guid);
            if (!online)
            {
                offline.push_back(guid);
                continue;
            }
            anyOnlineHuman = anyOnlineHuman || Policy::HumanStillVanilla(ReadHuman(online));
            if (restricted && Policy::GuildMemberRestricted(true, Policy::BotIsRestrictedRandomBot(ReadBot(online)), false))
                restricted->push_back(guid);
        }
        if (!restricted)
            return;
        std::lock_guard<std::mutex> lock(ConfirmedLock());
        for (ObjectGuid const& guid : offline)
            if (Policy::GuildMemberRestricted(false, false, Confirmed().Contains(guid.GetRawValue())))
                restricted->push_back(guid);
    }

    // Removal: Core's own Guild::DeleteMember with the stock kick flags (as
    // Guild::HandleRemoveMember), canDeleteGuild false; GuildRemovalSafe is
    // re-proven immediately before every call. World thread only.
    void RemoveRestrictedIfSafe(uint32 guildId, ObjectGuid target, ObjectGuid human)
    {
        Guild* guild = sGuildMgr->GetGuildById(guildId);
        bool const member = guild && guild->GetMember(target);
        bool const reason = guild && guild->GetMember(human);
        if (!Policy::GuildRemovalSafe(guild != nullptr, member, guild ? guild->GetMemberSize() : 0, reason))
            return;
        guild->DeleteMember(target, false, true, false);
    }

    void RemediateGuild(uint32 guildId, ObjectGuid human)
    {
        Guild* guild = sGuildMgr->GetGuildById(guildId);
        if (!guild || !guild->GetMember(human))
            return; // the human already left: nothing to separate
        bool anyHuman = false;
        std::vector<ObjectGuid> restricted;
        ScanGuild(*guild, human, anyHuman, &restricted);
        for (ObjectGuid const& guid : restricted)
            RemoveRestrictedIfSafe(guildId, guid, human);
    }

    // An off-world-thread add, re-checked on the world thread: the joiner
    // leaves only if it is a restricted bot and an online still-Vanilla
    // human member exists, re-proven immediately before the removal.
    void RecheckLateJoin(uint32 guildId, ObjectGuid joinerGuid)
    {
        Guild* guild = sGuildMgr->GetGuildById(guildId);
        if (!guild || !guild->GetMember(joinerGuid))
            return;
        bool joinerBot = false;
        if (Player const* joiner = ObjectAccessor::FindPlayer(joinerGuid))
            joinerBot = Policy::BotIsRestrictedRandomBot(ReadBot(joiner));
        else
        {
            std::lock_guard<std::mutex> lock(ConfirmedLock());
            joinerBot = Confirmed().Contains(joinerGuid.GetRawValue());
        }
        if (!joinerBot)
            return;
        bool anyHuman = false;
        ScanGuild(*guild, joinerGuid, anyHuman, nullptr);
        // Nothing runs between this scan and the removal (world thread).
        if (!Policy::GuildLateJoinerRemoved(true, true, anyHuman) ||
            !Policy::GuildRemovalSafe(true, true, guild->GetMemberSize(), anyHuman))
            return;
        guild->DeleteMember(joinerGuid, false, true, false);
    }

    void RemediatePendingGuilds()
    {
        std::vector<std::pair<uint32, uint64>> work;
        std::vector<std::pair<uint32, uint64>> late;
        {
            std::lock_guard<std::mutex> lock(GuildRemediationLock());
            GuildRemediation().Drain([&work](std::uint32_t guildId, std::uint64_t human)
            {
                work.emplace_back(guildId, human);
            });
            GuildLateJoins().Drain([&late](std::uint32_t guildId, std::uint64_t joiner)
            {
                late.emplace_back(guildId, joiner);
            });
        }
        if (!OptionOn())
            return;
        for (auto const& [guildId, human] : work)
            RemediateGuild(guildId, ObjectGuid(human));
        for (auto const& [guildId, joiner] : late)
            RecheckLateJoin(guildId, ObjectGuid(joiner));
    }

    // Core #28 seams (COSMIC_F14_GUILD_POLICY_HOOK_VERSION).
    class F14SocialIsolationGuildPolicyScript final : public GuildScript
    {
    public:
        F14SocialIsolationGuildPolicyScript() : GuildScript("IndividualProgression_F14SocialIsolationGuildPolicy",
            {GUILDHOOK_CAN_ADD_MEMBER, GUILDHOOK_CAN_SEND_CHAT_TO_MEMBER, GUILDHOOK_ON_EVENT}) { }

        // Before any petition, membership, DB or broadcast mutation. Only on
        // the world thread (guild and members may be read); anywhere else
        // fail open. Only a positively restricted bot is ever refused.
        bool CanAddMember(Guild const* guild, ObjectGuid guid) override
        {
            if (!OptionOn() || !guild || !guid)
                return true;
            if (!WorldThread().IsCurrent())
            {
                // Values only: no Player or Guild read here. Allowed now,
                // re-checked at the next world update (Playerbots level-up
                // re-roll calls InitGuild from a map worker).
                std::lock_guard<std::mutex> lock(GuildRemediationLock());
                GuildLateJoins().Mark(guild->GetId(), guid.GetRawValue());
                return true;
            }
            bool joinerBot = false;
            if (Player const* joiner = ObjectAccessor::FindPlayer(guid))
                joinerBot = Policy::BotIsRestrictedRandomBot(ReadBot(joiner));
            else
            {
                std::lock_guard<std::mutex> lock(ConfirmedLock());
                joinerBot = Confirmed().Contains(guid.GetRawValue());
            }
            if (!joinerBot)
                return true;
            bool anyHuman = false;
            ScanGuild(*guild, guid, anyHuman, nullptr);
            return !Policy::GuildAdmissionRefused(true, true, anyHuman);
        }

        // Per recipient, guild and officer chat, both directions. Addon
        // traffic is always delivered. Off the world thread both players
        // must be proven in the owned map, else fail open (T1). No reply.
        bool CanSendChatToMember(Guild const* /*guild*/, Player* sender, Player* receiver, bool /*officerOnly*/,
                                 uint32 language, std::string_view /*msg*/) override
        {
            if (!OptionOn() || !sender || !receiver || sender == receiver || language == LANG_ADDON)
                return true;
            ReadScope const scope = CurrentReadScope();
            if (!MayReadPair(scope, sender, receiver))
                return true;
            bool const restricted =
                Policy::IsRestrictedPair(true, ReadHuman(sender), ReadBot(receiver)) ||
                Policy::IsRestrictedPair(true, ReadHuman(receiver), ReadBot(sender));
            return !restricted;
        }

        // A still-Vanilla human JOINED (client accept: world thread). Its
        // restricted bots leave at the next world update (owner decision).
        void OnEvent(Guild* guild, uint8 eventType, ObjectGuid::LowType playerGuid1,
                     ObjectGuid::LowType /*playerGuid2*/, uint8 /*newRank*/) override
        {
            if (!OptionOn() || !guild || eventType != GUILD_EVENT_LOG_JOIN_GUILD || !playerGuid1 ||
                !WorldThread().IsCurrent())
                return;
            if (Player* joiner = ObjectAccessor::FindPlayer(ObjectGuid::Create<HighGuid::Player>(playerGuid1)))
                if (joiner->GetGuildId() == guild->GetId())
                    MarkGuildRemediation(joiner);
        }
    };
#endif

    // T1 only: remembers the world thread and counts world ticks. Reads nothing.
    class F14SocialIsolationWorldThreadScript final : public WorldScript
    {
    public:
        F14SocialIsolationWorldThreadScript() : WorldScript("IndividualProgression_F14SocialIsolationWorldThread",
            {WORLDHOOK_ON_UPDATE}) { }

        void OnUpdate(uint32 /*diff*/) override
        {
            WorldThread().Mark();
            SendPendingReplies();
#if defined(COSMIC_F14_GUILD_POLICY_HOOK_VERSION) && COSMIC_F14_GUILD_POLICY_HOOK_VERSION >= 1
            RemediatePendingGuilds();
#endif
        }
    };

    // /who only. The world-visibility (actor) query is NOT subscribed: visual
    // disappearance is paused by the owner. Core calls this only in builds
    // with its WHO GUID query compiled in, before it counts the row.
    class F14SocialIsolationWhoScript final : public WorldObjectScript
    {
    public:
        F14SocialIsolationWhoScript() : WorldObjectScript("IndividualProgression_F14SocialIsolationWho",
            {WORLDOBJECTHOOK_ON_F14_WHO_GUID_EXCLUSION_QUERY}) { }

        bool OnF14ShouldExcludeWhoEntryForViewer(Player const* viewer, ObjectGuid const& targetGuid,
                                                 uint32 cachedRace) override
        {
            if (!OptionOn() || !viewer || !targetGuid || viewer->GetGUID() == targetGuid)
                return false;
            return Policy::ExcludeFromWho(true, ReadHuman(viewer),
                sIndividualProgression->HasF14VerifiedRandomBotGuid(targetGuid), cachedRace);
        }
    };
}

void AddSC_mod_individual_progression_f14_social_isolation()
{
    new F14SocialIsolationPlayerScript();
    new F14SocialIsolationWhoScript();
    new F14SocialIsolationWorldThreadScript();
    new F14SocialIsolationLfgScript();
    new F14SocialIsolationGroupScript();
    new F14SocialIsolationBattlegroundScript();
    new F14SocialIsolationSpellScript();
#if defined(COSMIC_F14_GUILD_INVITE_HOOK_VERSION) && COSMIC_F14_GUILD_INVITE_HOOK_VERSION >= 1
    new F14SocialIsolationGuildScript();
#endif
#if defined(COSMIC_F14_GUILD_POLICY_HOOK_VERSION) && COSMIC_F14_GUILD_POLICY_HOOK_VERSION >= 1
    new F14SocialIsolationGuildPolicyScript();
#endif
}
