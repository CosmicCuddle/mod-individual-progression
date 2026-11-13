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
#include "Battleground.h"
#include "BattlegroundMgr.h"
#include "Chat.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "GlobalScript.h"
#include "Group.h"
#include "LFG.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "WorldObjectScript.h"
#include "WorldPacket.h"
#include "WorldScript.h"
#include "WorldSession.h"
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
        // One line, bounded length; empty means decline silently.
        std::string const text = Policy::NormalizeReply(sIndividualProgression->GetF14SocialIsolationReply());
        if (text.empty())
            return;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_UNIVERSAL, bot, human, text);
        human->SendDirectMessage(&data);
    }

    // Dungeon Finder facts captured at queue join, by GUID (see policy).
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
    bool AllowPair(Policy::Surface surface, Player* initiator, Player* other, bool addonMessage)
    {
        if (!OptionOn() || !initiator || !other || initiator == other)
            return true;
        if (!MayReadPair(CurrentReadScope(), initiator, other))
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

    class F14SocialIsolationPlayerScript final : public PlayerScript
    {
    public:
        F14SocialIsolationPlayerScript() : PlayerScript("IndividualProgression_F14SocialIsolation",
            {PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT, PLAYERHOOK_CAN_INIT_TRADE,
             PLAYERHOOK_CAN_GROUP_INVITE, PLAYERHOOK_CAN_GROUP_ACCEPT,
             PLAYERHOOK_ON_BEFORE_UPDATE, PLAYERHOOK_CAN_JOIN_LFG, PLAYERHOOK_ON_LOGOUT,
             PLAYERHOOK_CAN_JOIN_IN_BATTLEGROUND_QUEUE, PLAYERHOOK_CAN_BATTLEFIELD_PORT}) { }

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
            if (Group const* group = player->GetGroup())
                for (GroupReference const* itr = group->GetFirstMember(); itr; itr = itr->next())
                    if (Player const* member = itr->GetSource())
                        if (member != player)
                            facts.emplace_back(member->GetGUID().GetRawValue(),
                                               Policy::MakeQueueFacts(ReadHuman(member), ReadBot(member)));
            std::lock_guard<std::mutex> lock(QueueFactsLock());
            for (auto const& [guid, value] : facts)
                QueueFacts().Record(guid, value);
            return true;
        }

        void OnPlayerLogout(Player* player) override
        {
            if (!player)
                return;
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

        bool OnPlayerCanGroupInvite(Player* player, std::string& membername) override
        {
            if (!OptionOn() || !player)
                return true;
            Player* invitee = ObjectAccessor::FindPlayerByName(membername, false);
            if (!invitee)
                return true; // unknown target: not ours to decide
            return AllowPair(Policy::Surface::GroupInvite, player, invitee, false);
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
            std::vector<uint64> guids;
            if (proposal.roles)
                for (auto const& [guid, role] : *proposal.roles)
                    guids.push_back(guid.GetRawValue());
            else
                for (ObjectGuid const& guid : proposal.guids)
                    if (guid && guid.IsPlayer())
                        guids.push_back(guid.GetRawValue());
            std::lock_guard<std::mutex> lock(QueueFactsLock());
            return !QueueFacts().Rejects(true, guids);
        }
    };

    // Battleground roster (a): which instances hold a still-Vanilla human.
    // Humans enter on their worldport ack, a world-thread client packet, so
    // they are read only there; anything else records nothing. Leaving and
    // instance destruction use only the GUID and instance id values.
    class F14SocialIsolationBattlegroundScript final : public AllBattlegroundScript
    {
    public:
        F14SocialIsolationBattlegroundScript() : AllBattlegroundScript("IndividualProgression_F14SocialIsolationBg",
            {ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_ADD_PLAYER, ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_REMOVE_PLAYER_AT_LEAVE,
             ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY}) { }

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

    // T1 only: remembers the world thread and counts world ticks. Reads nothing.
    class F14SocialIsolationWorldThreadScript final : public WorldScript
    {
    public:
        F14SocialIsolationWorldThreadScript() : WorldScript("IndividualProgression_F14SocialIsolationWorldThread",
            {WORLDHOOK_ON_UPDATE}) { }

        void OnUpdate(uint32 /*diff*/) override
        {
            WorldThread().Mark();
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
    new F14SocialIsolationBattlegroundScript();
}
