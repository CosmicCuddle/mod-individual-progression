"""Execute actual F14 group-view/reader/sync code with explicit Core test doubles.

This is source-linked adapter testing, not native Worldserver integration.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


PRELUDE = r'''
#include "IndividualProgressionF14GroupSyncView.h"
#include <cstdio>
#include <functional>
#include <mutex>
#include <unordered_set>
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using int32 = std::int32_t;
enum QuestStatus { QUEST_STATUS_NONE, QUEST_STATUS_COMPLETE, QUEST_STATUS_INCOMPLETE, QUEST_STATUS_REWARDED };
struct QuestStatusData { QuestStatus Status = QUEST_STATUS_NONE; };
using QuestStatusMap = std::map<uint32, QuestStatusData>;
struct Quest {
    bool GetQuestMethod() const { return true; }
    bool IsAutoComplete() const { return false; }
    bool IsSeasonal() const { return false; }
    bool IsRepeatable() const { return false; }
};
struct ObjectMgr {
    Quest quest;
    Quest const* GetQuestTemplate(uint32 id) const { return id ? &quest : nullptr; }
};
static ObjectMgr Objects;
[[maybe_unused]] static ObjectMgr* sObjectMgr = &Objects;
[[maybe_unused]] constexpr int ADDITIONAL_SAVING_QUEST_STATUS = 1;
struct ObjectGuid {
    uint64 raw = 0;
    ObjectGuid() = default;
    explicit ObjectGuid(uint64 value) : raw(value) { }
    explicit operator bool() const { return raw != 0; }
    uint64 GetRawValue() const { return raw; }
    uint64 GetCounter() const { return raw; }
    bool operator==(ObjectGuid other) const { return raw == other.raw; }
    bool operator!=(ObjectGuid other) const { return raw != other.raw; }
};
struct Session {
    bool headless = false, closed = false;
    bool IsHeadless() const { return headless; }
    bool IsSocketClosed() const { return closed; }
};
struct Group;
struct Player {
    ObjectGuid guid;
    Group* group = nullptr;
    Session session;
    uint8 race = 1, earned = 0;
    bool inWorld = true, gm = false, verified = false, normal = true;
    unsigned writes = 0;
    std::unordered_set<uint32> m_RewardedQuests;
    std::map<uint32, bool> m_RewardedQuestsSave;
    bool questBacked = false;
    unsigned questUpdates = 0;
    QuestStatusMap m_QuestStatus;
    std::map<uint32, bool> m_QuestStatusSave;
    void SetRewardedQuest(uint32 quest_id);
    void RemoveRewardedQuest(uint32 questId, bool update = true);
    void SetQuestStatus(uint32, QuestStatus, bool update = true);
    void RemoveActiveQuest(uint32, bool update = true);
    QuestStatus GetQuestStatus(uint32) const;
    bool IsQuestRewarded(uint32 id) const { return m_RewardedQuests.count(id) != 0; }
    bool SatisfyQuestSeasonal(Quest const*, bool) const { return true; }
    uint32 GetZoneId() const { return 0; }
    uint32 GetAreaId() const { return 0; }
    void UpdateZoneDependentAuras(uint32) { }
    void UpdateAreaDependentAuras(uint32) { }
    void AdditionalSavingAddMask(int) { }
    void SendQuestUpdate(uint32) { ++questUpdates; }
    ObjectGuid GetGUID() const { return guid; }
    Group* GetGroup() const { return group; }
    Session const* GetSession() const { return &session; }
    bool IsInWorld() const { return inWorld; }
    bool IsGameMaster() const { return gm; }
    uint8 getRace() const { return race; }
    uint32 GetLevel() const { return 60; }
};
using RewardedQuestSet = std::unordered_set<uint32>;
struct GroupReference {
    Player* player;
    GroupReference* following = nullptr;
    Player* GetSource() const { return player; }
    GroupReference* next() const { return following; }
};
struct Group {
    ObjectGuid guid, leader;
    GroupReference* members = nullptr;
    ObjectGuid GetGUID() const { return guid; }
    ObjectGuid GetLeaderGUID() const { return leader; }
    GroupReference* GetFirstMember() const { return members; }
};
enum RemoveMethod : uint8 { TestRemoval };
enum Hook {
    WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_AFTER_CONFIG_LOAD, GROUPHOOK_ON_ADD_MEMBER,
    GROUPHOOK_ON_REMOVE_MEMBER, GROUPHOOK_ON_CHANGE_LEADER, GROUPHOOK_ON_DISBAND,
    PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_BEFORE_UPDATE,
    PLAYERHOOK_ON_F14_SESSION_REBOUND, PLAYERHOOK_ON_F14_SESSION_DISCONNECTED,
    PLAYERHOOK_ON_BEFORE_REWARDED_QUEST_CHANGE, PLAYERHOOK_ON_AFTER_REWARDED_QUEST_CHANGE
};
struct WorldScript {
    WorldScript(char const*, std::initializer_list<Hook>) { }
    virtual ~WorldScript() = default;
    virtual void OnUpdate(uint32) { }
    virtual void OnAfterConfigLoad(bool) { }
};
struct GroupScript {
    GroupScript(char const*, std::initializer_list<Hook>) { }
    virtual ~GroupScript() = default;
    virtual void OnAddMember(Group*, ObjectGuid) { }
    virtual void OnRemoveMember(Group*, ObjectGuid, RemoveMethod, ObjectGuid, char const*) { }
    virtual void OnChangeLeader(Group*, ObjectGuid, ObjectGuid) { }
    virtual void OnDisband(Group*) { }
};
struct PlayerScript {
    std::set<Hook> hooks;
    PlayerScript(char const*, std::initializer_list<Hook> enabled) : hooks(enabled) { }
    bool Enabled(Hook hook) const { return hooks.count(hook) != 0; }
    virtual ~PlayerScript() = default;
    virtual void OnPlayerLogin(Player*) { }
    virtual void OnPlayerLogout(Player*) { }
    virtual void OnPlayerBeforeUpdate(Player*, uint32) { }
    virtual void OnF14PlayerSessionRebound(Player*) { }
    virtual void OnF14PlayerSessionDisconnected(Player*) { }
    virtual void OnPlayerBeforeRewardedQuestChange(Player*, uint32) { }
    virtual void OnPlayerAfterRewardedQuestChange(Player*, uint32) { }
};
static std::vector<PlayerScript*> QuestScripts;
static std::function<void(Player*, uint32)> BeforeMutationProbe;
static std::function<void(Player*, uint32)> AfterMutationProbe;
static unsigned BeforeEvents = 0, AfterEvents = 0;
[[maybe_unused]] static void QuestProbe(Hook hook, Player* player, uint32 id) {
    auto& probe = hook == PLAYERHOOK_ON_BEFORE_REWARDED_QUEST_CHANGE ? BeforeMutationProbe : AfterMutationProbe;
    if (hook == PLAYERHOOK_ON_BEFORE_REWARDED_QUEST_CHANGE) ++BeforeEvents; else ++AfterEvents;
    auto call = std::move(probe);
    probe = {};
    if (call) call(player, id);
}
#define CALL_ENABLED_HOOKS(type, hook, expression) do { for (auto* script : QuestScripts) if (script->Enabled(hook)) { expression; } QuestProbe(hook, player, questId); } while (false)
struct ScriptMgr {
    void OnPlayerBeforeRewardedQuestChange(Player*, uint32);
    void OnPlayerAfterRewardedQuestChange(Player*, uint32);
};
static ScriptMgr Manager;
[[maybe_unused]] static ScriptMgr* sScriptMgr = &Manager;
static std::map<uint64, Player*> Players;
struct ObjectAccessor {
    static Player* FindPlayer(ObjectGuid guid) {
        auto it = Players.find(guid.raw);
        return it == Players.end() ? nullptr : it->second;
    }
};
struct GroupMgr {
    std::map<uint64, Group*> groups;
    Group* GetGroupByGUID(uint64 guid) {
        auto it = groups.find(guid);
        return it == groups.end() ? nullptr : it->second;
    }
};
static GroupMgr GroupManager;
static GroupMgr* sGroupMgr = &GroupManager;
enum ProgressionState : uint8 { TestStage = 0 };
constexpr ProgressionState PROGRESSION_PRE_TBC = static_cast<ProgressionState>(8);
constexpr uint8 WrathAccessStage = 13;
static uint8 EffectiveStage(int) { return 0; } // paid path is disabled in this fixture
struct IndividualProgression {
    bool enabled = true, f14ProtectVerifiedRandomBotGroupFloor = true, enforceGroupRules = false;
    int32 progressionLimit = 0;
    bool paidActive = false;
    bool VanillaPvpTitlesKeepPostVanilla = false;
    bool DropRanksFromEarned(Player*);
    bool UsesF14TemporaryGroupProgression(Player const*);
    bool TryGetF14TemporaryGroupProgression(Player*, uint8, uint8&);
    void RequestF14GroupSyncView(Group*);
    void SyncBotsProgressionToLeader(Group*);
    uint8 GetEffectiveProgression(Player*);
    bool hasPassedProgression(Player*, ProgressionState) const;
    uint8 GetPlayerProgressionFromQuests(Player* player) {
        if (!player) return 0;
        if (!player->questBacked) return player->earned;
        uint8 stage = 0;
        for (uint8 next = 1; next <= 18; ++next)
            if (player->GetQuestStatus(66000 + next) == QUEST_STATUS_REWARDED) stage = next;
        return stage;
    }
    bool HasF14VerifiedRandomBotIdentity(Player const* player) { return player->verified; }
    bool isNormalAccount(Player* player) { return player->normal; }
    bool IsPaidExpansionActive() { return paidActive; }
    int GetExpansionFacts(Player*) { return 0; }
    bool ShouldSkipF14VerifiedRandomBotGroupDowngrade(Player*, uint8) { return false; }
    void ForceUpdateProgressionState(Player* player, ProgressionState stage) {
        ++player->writes;
        player->earned = uint8(stage);
    }
};
static IndividualProgression IP;
static IndividualProgression* sIndividualProgression = &IP;
'''

PAIRED_TEST = r'''
    IP.progressionLimit = 0;
    IP.f14ProtectVerifiedRandomBotGroupFloor = true;
    human.questBacked = true;
    human.group = bot.group = &group;
    human.inWorld = true;
    human.session.closed = false;
    group.leader = human.guid;
    bot.earned = 8;
    human.SetRewardedQuest(66008);
    IP.RequestF14GroupSyncView(&group);
    world.OnUpdate(0);
    bool duringSafe = true;
    auto during = [&](Player*, uint32) {
        world.OnUpdate(0); // force publication while Core's write is still bracketed
        duringSafe = duringSafe && IP.GetEffectiveProgression(&bot) == 8;
    };
    BeforeMutationProbe = during;
    human.SetRewardedQuest(66015);
    CHECK(duringSafe);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS && QUEST_HOOKS ? 15 : 8));
    IP.paidActive = true;
    CHECK(IP.GetEffectiveProgression(&bot) == 8); // receipt-mutation support is not implied
    IP.paidActive = false;
    BeforeMutationProbe = during;
    human.RemoveRewardedQuest(66015, false);
    CHECK(duringSafe);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    BeforeMutationProbe = [&](Player*, uint32) {
        world.OnAfterConfigLoad(true); // must not clear the active mutation count
        world.OnUpdate(0);
        duringSafe = duringSafe && IP.GetEffectiveProgression(&bot) == 8;
    };
    human.SetRewardedQuest(66015);
    CHECK(duringSafe);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS && QUEST_HOOKS ? 15 : 8));
    BeforeMutationProbe = [&](Player* player, uint32) {
        player->SetRewardedQuest(66016); // nested pair must not release the outer fence
        world.OnUpdate(0);
        duringSafe = duringSafe && IP.GetEffectiveProgression(&bot) == 8;
    };
    human.SetRewardedQuest(66014);
    CHECK(duringSafe);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS && QUEST_HOOKS ? 16 : 8));
    BeforeMutationProbe = [&](Player*, uint32) {
        duringSafe = duringSafe && IP.GetEffectiveProgression(&bot) == (HOOKS && QUEST_HOOKS ? 16 : 8);
    };
    human.SetRewardedQuest(123); // unrelated quests do not enter the IP fence
    CHECK(duringSafe);
    auto const beforeNoop = BeforeEvents;
    human.SetRewardedQuest(66016);
    human.RemoveRewardedQuest(999999, false);
    CHECK(BeforeEvents == beforeNoop && BeforeEvents == AfterEvents);
    BeforeMutationProbe = [&](Player*, uint32) {
        IP.f14ProtectVerifiedRandomBotGroupFloor = false;
        CHECK_OPTION_TOGGLE
        IP.f14ProtectVerifiedRandomBotGroupFloor = true;
        world.OnUpdate(0);
        duringSafe = duringSafe && IP.GetEffectiveProgression(&bot) == 8;
    };
    human.RemoveRewardedQuest(66016, false);
    CHECK(duringSafe && BeforeEvents == AfterEvents);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS && QUEST_HOOKS ? 15 : 8));
    CHECK(bot.earned == 8 && human.m_RewardedQuests.count(66015) == 1);
    human.RemoveRewardedQuest(66014, false);
    world.OnUpdate(0);
    BeforeMutationProbe = during;
    human.SetQuestStatus(66015, QUEST_STATUS_INCOMPLETE, false); // masks the rewarded marker
    CHECK(duringSafe && human.IsQuestRewarded(66015));
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    BeforeMutationProbe = during;
    human.RemoveActiveQuest(66015, false); // reveals the same earned marker again
    CHECK(duringSafe && human.IsQuestRewarded(66015));
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS && QUEST_HOOKS ? 15 : 8));
#if QUEST_HOOKS
    sessions.OnPlayerAfterRewardedQuestChange(&human, 66008); // malformed pair fails closed
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    world.OnAfterConfigLoad(true);
    IP.RequestF14GroupSyncView(&group);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
#endif
'''.replace('CHECK_OPTION_TOGGLE', 'duringSafe = duringSafe && IP.GetEffectiveProgression(&bot) == bot.earned;')

TEST = r'''
#define CHECK(value) do { ++checks; if (!(value)) { std::printf("FAIL line %d\n", __LINE__); return 1; } } while (0)
int main()
{
    unsigned checks = 0;
    Player human;
    human.guid = ObjectGuid(10);
    human.earned = 1;
    Player bot;
    bot.guid = ObjectGuid(20);
    bot.race = 10;
    bot.earned = 17;
    bot.session.headless = true;
    bot.normal = false;
    bot.verified = true;
    Group group;
    group.guid = ObjectGuid(100);
    group.leader = human.guid;
    GroupReference botRef{&bot}, humanRef{&human, &botRef};
    group.members = &humanRef;
    human.group = bot.group = &group;
    Players[10] = &human;
    Players[20] = &bot;
    GroupManager.groups[100] = &group;
    GroupViewGroups groups;
    GroupViewSessions sessions;
    QuestScripts.push_back(&sessions);
    GroupViewWorld world;
    CHECK(IP.GetEffectiveProgression(&bot) == 8); // no snapshot cannot grant Wrath
    IP.SyncBotsProgressionToLeader(&group); // actual production function, not a copied model
    CHECK(bot.earned == 17 && bot.writes == 0);
    CHECK(human.earned == 1 && human.writes == 0);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    for (uint8 desired : {uint8{8}, uint8{15}, uint8{8}})
    {
        human.earned = desired;
        IP.RequestF14GroupSyncView(&group);
        CHECK(IP.GetEffectiveProgression(&bot) == 8); // old positive stage is invalid already
        world.OnUpdate(0);
        CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS ? desired : 8));
        CHECK(bot.earned == 17 && bot.writes == 0);
    }
    // A hidden progression quest changed outside IP's two writers.
    human.earned = 15;
    sessions.OnPlayerBeforeUpdate(&human, 0);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS ? 15 : 8));
    bot.earned = 8;
    CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS && QUEST_HOOKS ? 15 : 8));
    bot.earned = 17;
    human.earned = 8;
    sessions.OnPlayerBeforeUpdate(&human, 0);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    bot.group = nullptr;
    CHECK(IP.GetEffectiveProgression(&bot) == 17); // leave exposes exact earned stage
    Group other;
    other.guid = ObjectGuid(101);
    other.leader = human.guid;
    bot.group = &other;
    CHECK(IP.GetEffectiveProgression(&bot) == 8); // old group cannot donate its cached view
    bot.group = &group;
    bot.verified = false;
    CHECK(IP.GetEffectiveProgression(&bot) == 17); // raw earned evidence only, no borrowed era
    bot.earned = 0;
    CHECK(IP.GetEffectiveProgression(&bot) == 0); // unknown identity cannot gain TBC
    bot.verified = true;
    CHECK(IP.GetEffectiveProgression(&bot) == 8); // approved grouped floor needs positive identity
    CHECK(!IP.DropRanksFromEarned(&bot)); // borrowed TBC is not permanent title-removal authority
    bot.group = nullptr;
    CHECK(IP.GetEffectiveProgression(&bot) == 0); // no fabricated earned/solo stage
    bot.group = &group;
    bot.earned = 17;
    CHECK(IP.DropRanksFromEarned(&bot)); // original earned post-Vanilla rule still applies
    bot.session.headless = false;
    CHECK(!IP.UsesF14TemporaryGroupProgression(&bot)); // humans/linked client immunity
    bot.session.headless = true;
    bot.gm = true;
    CHECK(!IP.UsesF14TemporaryGroupProgression(&bot));
    bot.gm = false;
    bot.race = 1;
    CHECK(!IP.UsesF14TemporaryGroupProgression(&bot));
    // Legacy normal-race sync still reaches Vanilla and ignores the new view.
    human.earned = 1;
    IP.SyncBotsProgressionToLeader(&group);
    CHECK(bot.earned == 1 && bot.writes == 1);
    bot.race = 11;
    bot.earned = 17;
    IP.RequestF14GroupSyncView(&group);
    world.OnUpdate(0);
    groups.OnChangeLeader(&group, ObjectGuid(99), human.guid);
    group.leader = ObjectGuid(99);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    group.leader = human.guid;
    groups.OnRemoveMember(&group, bot.guid, TestRemoval, ObjectGuid(), "test");
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    sessions.OnPlayerLogout(&human);
    human.inWorld = false;
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    human.inWorld = true;
    human.earned = 15;
    sessions.OnPlayerLogin(&human);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == (HOOKS ? 15 : 8));
    Player replacement = human; // relog under the same GUID, different Player
    replacement.earned = 8;
    Players[10] = &replacement;
    sessions.OnPlayerLogin(&replacement);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    Players[10] = &human;
    sessions.OnPlayerLogin(&human);
    world.OnUpdate(0);
#if HOOKS
    sessions.OnF14PlayerSessionDisconnected(&human);
    human.session.closed = true;
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == 8);
    human.session.closed = false;
    sessions.OnF14PlayerSessionRebound(&human);
    world.OnUpdate(0);
    CHECK(IP.GetEffectiveProgression(&bot) == 15);
#endif
    groups.OnDisband(&group);
    CHECK(IP.GetEffectiveProgression(&bot) == 8); // even before Core clears own membership
    bot.group = nullptr;
    CHECK(IP.GetEffectiveProgression(&bot) == 17);
    bot.group = &group;
    IP.f14ProtectVerifiedRandomBotGroupFloor = false;
    CHECK(IP.GetEffectiveProgression(&bot) == 17);
    CHECK(bot.earned == 17 && human.writes == 0);
    IP.f14ProtectVerifiedRandomBotGroupFloor = true;
    CHECK(CommandRefusesFloor(&bot, 0));
    CHECK(CommandRefusesFloor(&bot, 7));
    CHECK(!CommandRefusesFloor(&bot, 8));
    CHECK(PreserveLoginStage(&bot)); // raw 17 survives the level-60 write
    bot.verified = false;
    CHECK(!CommandRefusesFloor(&bot, 0));
    CHECK(!PreserveLoginStage(&bot));
    bot.verified = true;
    bot.earned = 0;
    CHECK(!CommandRefusesFloor(&bot, 0)); // missing floor is not an earned marker
    CHECK(!PreserveLoginStage(&bot)); // preserve legacy newborn login/attunement handling
    bot.earned = 17;
    IP.progressionLimit = 7;
    CHECK(!IP.hasPassedProgression(&bot, static_cast<ProgressionState>(8)));
    CHECK(!IP.DropRanksFromEarned(&bot)); // preserve the existing cap check too
    CHECK(bot.earned == 17); // lowering a cap does not delete historic rewards
    PAIRED_QUEST_TESTS
    std::printf("F14 ACTUAL GROUP ADAPTER %u/%u passed (session=%d quest=%d; Core test doubles)\n", checks, checks, HOOKS, QUEST_HOOKS);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", nargs="+", default=["clang++"])
    parser.add_argument("--std", choices=("c++17", "c++20"), default="c++20")
    parser.add_argument("--ndebug", action="store_true")
    parser.add_argument("--core", type=Path, help="Paired Core checkout; compiles its actual mutation writers and dispatch")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    read = lambda name: (root / "src" / name).read_text(encoding="utf-8")
    adapter = re.sub(r'^#include[^\n]*\n', '', read("IndividualProgressionF14GroupSyncView.cpp"), flags=re.M)
    sync = function(read("IndividualProgression.cpp"), "void IndividualProgression::SyncBotsProgressionToLeader(")
    reader = function(read("IndividualProgressionExpansion.cpp"), "uint8 IndividualProgression::GetEffectiveProgression(")
    access = function(read("IndividualProgression.cpp"), "bool IndividualProgression::hasPassedProgression(")
    writer = function(read("IndividualProgression.cpp"), "void IndividualProgression::ForceUpdateProgressionState(")
    start = writer.index("    if (sIndividualProgression->UsesF14TemporaryGroupProgression(player)")
    end = writer.index("        return;", start) + len("        return;")
    command_guard = writer[start:end].replace("        return;", "        return true;")
    command_guard = "bool CommandRefusesFloor(Player* player, ProgressionState newState) {\n" + command_guard + "\nreturn false;\n}\n"
    command_guard = command_guard.replace("GetPlayerProgressionFromQuests(player)", "IP.GetPlayerProgressionFromQuests(player)")
    # The real condition takes ProgressionState; the fixture callers use byte stages.
    command_guard += "bool CommandRefusesFloor(Player* p, int s) { return CommandRefusesFloor(p, static_cast<ProgressionState>(s)); }\n"
    login = read("IndividualProgressionPlayer.cpp")
    start = login.index("            bool const preserveF14GroupEarnedStage =")
    end = login.index(";", start) + 1
    login_guard = "bool PreserveLoginStage(Player* player) {\n" + login[start:end] + "\nreturn preserveF14GroupEarnedStage;\n}\n"
    cleanup = read("IndividualProgression.cpp")
    start = cleanup.index("    bool const earnedOnly =")
    end = cleanup.index("    bool dropAllRanks =", start)
    end = cleanup.index(";", end) + 1
    title_guard = "bool IndividualProgression::DropRanksFromEarned(Player* player) {\n" + cleanup[start:end] + "\nreturn dropAllRanks;\n}\n"
    paired = "QuestStatus Player::GetQuestStatus(uint32 id) const { auto it = m_QuestStatus.find(id); return it != m_QuestStatus.end() ? it->second.Status : (IsQuestRewarded(id) ? QUEST_STATUS_REWARDED : QUEST_STATUS_NONE); }\n"
    if args.core:
        core = args.core.resolve()
        core_header = (core / "src/server/game/Scripting/ScriptDefines/PlayerScript.h").read_text(encoding="utf-8")
        if "#define COSMIC_REWARDED_QUEST_CHANGE_HOOK_VERSION 1" not in core_header:
            parser.error("--core must provide rewarded-quest change hook version 1")
        core_quest = (core / "src/server/game/Entities/Player/PlayerQuest.cpp").read_text(encoding="utf-8")
        core_dispatch = (core / "src/server/game/Scripting/ScriptDefines/PlayerScript.cpp").read_text(encoding="utf-8")
        paired = function(core_quest, "class RewardedQuestChangeGuard") + ";\n"
        paired += "\n".join(function(core_quest, sig) for sig in ("void Player::SetRewardedQuest(", "void Player::RemoveRewardedQuest(",
                                                                 "void Player::SetQuestStatus(", "void Player::RemoveActiveQuest(",
                                                                 "QuestStatus Player::GetQuestStatus("))
        paired += "\n".join(function(core_dispatch, "void ScriptMgr::OnPlayer" + phase + "RewardedQuestChange(") for phase in ("Before", "After"))
    with tempfile.TemporaryDirectory(prefix="f14-group-view-") as tmp:
        source = Path(tmp) / "group-view.cpp"
        source.write_text(PRELUDE + '#include "IndividualProgressionF14LoginPolicy.h"\n' + adapter + reader + access + sync + command_guard + login_guard + title_guard + paired + TEST.replace("PAIRED_QUEST_TESTS", PAIRED_TEST if args.core else ""), encoding="utf-8")
        modes = [(hooks, quest) for hooks in (0, 1) for quest in ((0, 1) if args.core else (0,))]
        for hooks, quest in modes:
            binary = Path(tmp) / ("group-view-" + str(hooks) + "-" + str(quest) + ".exe")
            flags = ["-DHOOKS=" + str(hooks), "-DQUEST_HOOKS=" + str(quest)]
            if args.ndebug:
                flags.append("-DNDEBUG")
            if hooks:
                flags += ["-DCOSMIC_F14_SESSION_REBIND_EXPERIMENT=1", "-DCOSMIC_F14_SESSION_DISCONNECT_EXPERIMENT=1"]
            if quest:
                flags.append("-DCOSMIC_REWARDED_QUEST_CHANGE_HOOK_VERSION=1")
            command = args.compiler + ["-std=" + args.std, "-pthread", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2",
                                       "-I" + str(root / "src")] + flags + [str(source), "-o", str(binary)]
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
