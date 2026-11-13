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
#include "Chat.h"
#include "GameTime.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PlayerScript.h"
#include "ScriptMgr.h"
#include "WorldObjectScript.h"
#include "WorldPacket.h"
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

    // initiator -> other. Returns true to ALLOW. Either side may be the bot.
    bool AllowPair(Policy::Surface surface, Player* initiator, Player* other, bool addonMessage)
    {
        if (!OptionOn() || !initiator || !other || initiator == other)
            return true;

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
             PLAYERHOOK_CAN_GROUP_INVITE, PLAYERHOOK_CAN_GROUP_ACCEPT}) { }

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
            // 1. The leader, in both directions (unchanged pair rule).
            Player* leader = ObjectAccessor::FindPlayerByLowGUID(group->GetLeaderGUID().GetCounter());
            if (leader && !AllowPair(Policy::Surface::GroupAccept, player, leader, false))
                return false;
            // 2. Every other current member, for a human who is still in
            // Vanilla. Unresolved members are skipped: never guessed by race.
            return AllowGroupMembers(player, group);
        }

    private:
        bool AllowGroupMembers(Player* human, Group const* group)
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
                Player* member = ObjectAccessor::FindPlayer(slot.guid);
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
}
