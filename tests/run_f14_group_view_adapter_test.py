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
#include <mutex>
using uint8 = std::uint8_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using int32 = std::int32_t;
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
    ObjectGuid GetGUID() const { return guid; }
    Group* GetGroup() const { return group; }
    Session const* GetSession() const { return &session; }
    bool IsInWorld() const { return inWorld; }
    bool IsGameMaster() const { return gm; }
    uint8 getRace() const { return race; }
    uint32 GetLevel() const { return 60; }
};
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
    PLAYERHOOK_ON_F14_SESSION_REBOUND, PLAYERHOOK_ON_F14_SESSION_DISCONNECTED
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
    PlayerScript(char const*, std::initializer_list<Hook>) { }
    virtual ~PlayerScript() = default;
    virtual void OnPlayerLogin(Player*) { }
    virtual void OnPlayerLogout(Player*) { }
    virtual void OnPlayerBeforeUpdate(Player*, uint32) { }
    virtual void OnF14PlayerSessionRebound(Player*) { }
    virtual void OnF14PlayerSessionDisconnected(Player*) { }
};
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
    bool VanillaPvpTitlesKeepPostVanilla = false;
    bool DropRanksFromEarned(Player*);
    bool UsesF14TemporaryGroupProgression(Player const*);
    bool TryGetF14TemporaryGroupProgression(Player*, uint8, uint8&);
    void RequestF14GroupSyncView(Group*);
    void SyncBotsProgressionToLeader(Group*);
    uint8 GetEffectiveProgression(Player*);
    bool hasPassedProgression(Player*, ProgressionState) const;
    uint8 GetPlayerProgressionFromQuests(Player* player) { return player ? player->earned : 0; }
    bool HasF14VerifiedRandomBotIdentity(Player const* player) { return player->verified; }
    bool isNormalAccount(Player* player) { return player->normal; }
    bool IsPaidExpansionActive() { return false; }
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
    CHECK(IP.GetEffectiveProgression(&bot) == 8); // blocked: unearned Wrath needs atomic Core invalidation
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
    std::printf("F14 ACTUAL GROUP ADAPTER %u/%u passed (hooks=%d; Core test doubles)\n", checks, checks, HOOKS);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", nargs="+", default=["clang++"])
    parser.add_argument("--std", choices=("c++17", "c++20"), default="c++20")
    parser.add_argument("--ndebug", action="store_true")
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
    with tempfile.TemporaryDirectory(prefix="f14-group-view-") as tmp:
        source = Path(tmp) / "group-view.cpp"
        source.write_text(PRELUDE + '#include "IndividualProgressionF14LoginPolicy.h"\n' + adapter + reader + access + sync + command_guard + login_guard + title_guard + TEST, encoding="utf-8")
        for hooks in (0, 1):
            binary = Path(tmp) / ("group-view-" + str(hooks) + ".exe")
            flags = ["-DHOOKS=" + str(hooks)]
            if args.ndebug:
                flags.append("-DNDEBUG")
            if hooks:
                flags += ["-DCOSMIC_F14_SESSION_REBIND_EXPERIMENT=1", "-DCOSMIC_F14_SESSION_DISCONNECT_EXPERIMENT=1"]
            command = args.compiler + ["-std=" + args.std, "-pthread", "-Wall", "-Wextra", "-Werror", "-pedantic", "-O2",
                                       "-I" + str(root / "src")] + flags + [str(source), "-o", str(binary)]
            subprocess.run(command, check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
