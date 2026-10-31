// IP progression-marker reset harness: compiled extracted-body test. TEST-ONLY.
//
// Build (see docs/IP_MARKER_RESET_REGRESSION_HARNESS_2026-10-31.md):
//   python3 tests/ip_marker_reset_extract.py generate --out build/marker-reset
//   g++ -std=c++17 -Wall -Wextra -Werror -I build/marker-reset -I tests/marker_reset_harness
//       tests/marker_reset_harness/harness_main.cpp -o build/marker-reset/harness
//
// Executes the VERBATIM pinned bodies of
//   mod-playerbots PlayerbotFactory::ResetQuests
//   Core Player::GetQuestStatus / RemoveActiveQuest / RemoveRewardedQuest /
//        SetRewardedQuest / the rewarded loop of _SaveQuestStatus
//   IP   IndividualProgression::GetPlayerProgressionFromQuests
// against stand-in types, with quest flags taken from IP's real SQL.
//
// Evidence level: "compiled extracted bodies". NOT Worldserver, NOT a
// database, NOT gameplay and NOT the owner's installed revisions.
// Exit status 0 only if every expectation about the PINNED chain holds.

#include "harness_stubs.h"

#include <algorithm>
#include <cstdio>
#include <set>

#include "pinned_bodies.inc"
#include "pinned_sql_markers.inc"

namespace
{
int g_test = 0;
int g_failed = 0;

void Expect(bool ok, std::string const& what)
{
    ++g_test;
    if (!ok)
        ++g_failed;
    std::printf("%s %d - %s\n", ok ? "ok" : "not ok", g_test, what.c_str());
}

constexpr uint32 kOrdinaryRewarded = 100;   // normal non-repeatable quest
constexpr uint32 kRepeatableRewarded = 200; // SpecialFlags REPEATABLE
constexpr uint32 kActiveInLog = 300;        // currently in quest log

void LoadTemplates(bool forceMarkersRepeatable)
{
    sObjectMgr->HarnessReset();
    for (HarnessMarkerRow const& r : kStageMarkers)
        sObjectMgr->HarnessAdd(r.id, r.questSortId, r.flags,
                               r.effectiveSpecialFlags | (forceMarkersRepeatable ? QUEST_SPECIAL_FLAGS_REPEATABLE : 0));
    for (HarnessMarkerRow const& r : kPvpMarkers)
        sObjectMgr->HarnessAdd(r.id, r.questSortId, r.flags, r.effectiveSpecialFlags);
    sObjectMgr->HarnessAdd(kOrdinaryRewarded, 12, 0, 0);
    sObjectMgr->HarnessAdd(kRepeatableRewarded, 12, 0, QUEST_SPECIAL_FLAGS_REPEATABLE);
    sObjectMgr->HarnessAdd(kActiveInLog, 12, 0, 0);
}

// A saved character: markers 66001..66000+stage rewarded through the verified
// Core reward tail, then flushed by the verbatim save loop (as if persisted).
void PrepareSavedBot(Player& bot, uint8 stage, bool withExtras)
{
    for (uint32 i = 1; i <= stage; ++i)
        bot.HarnessRewardLikeCore(66000 + i);
    if (withExtras)
    {
        bot.HarnessRewardLikeCore(kPvpMarkers[0].id);
        bot.HarnessRewardLikeCore(kOrdinaryRewarded);
        bot.HarnessRewardLikeCore(kRepeatableRewarded);
        bot.m_QuestStatus[kActiveInLog].Status = QUEST_STATUS_INCOMPLETE;
    }
    HarnessTransaction flush;
    bot.HarnessSaveRewardedQuests(&flush);
}

struct SaveResult
{
    std::set<uint32> deleted;
    std::set<uint32> inserted;
};

SaveResult Save(Player& bot)
{
    HarnessTransaction trans;
    bot.HarnessSaveRewardedQuests(&trans);
    SaveResult out;
    for (CharacterDatabasePreparedStatement* stmt : trans.appended)
    {
        uint32 quest = static_cast<uint32>(stmt->data[1]);
        if (stmt->data[0] != bot.GetGUID().GetRawValue())
            continue;
        if (stmt->statementId == CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST)
            out.deleted.insert(quest);
        else if (stmt->statementId == CHAR_INS_CHAR_QUESTSTATUS_REWARDED)
            out.inserted.insert(quest);
    }
    return out;
}

std::set<uint32> MarkersUpTo(uint8 stage)
{
    std::set<uint32> s;
    for (uint32 i = 1; i <= stage; ++i)
        s.insert(66000 + i);
    return s;
}

std::string Str(uint32 v) { return std::to_string(v); }
} // namespace

int main()
{
    std::printf("# IP marker reset harness (compiled extracted pinned bodies; NOT runtime)\n");
    std::printf("# mutations: %s\n", kHarnessMutations[0] ? kHarnessMutations : "none");
    IndividualProgression ip;

    // --- SQL-derived template facts --------------------------------------
    LoadTemplates(false);
    Expect(sizeof(kStageMarkers) / sizeof(kStageMarkers[0]) == 18, "IP SQL defines 18 stage marker templates");
    bool idsOk = true, nonRepeatable = true, nonSeasonal = true;
    for (uint32 i = 0; i < 18; ++i)
    {
        Quest const* q = sObjectMgr->GetQuestTemplate(66001 + i);
        idsOk = idsOk && q && kStageMarkers[i].id == 66001 + i;
        nonRepeatable = nonRepeatable && q && !q->IsRepeatable();
        nonSeasonal = nonSeasonal && q && !q->IsSeasonal();
    }
    Expect(idsOk, "stage marker IDs are exactly 66001..66018");
    Expect(nonRepeatable, "verbatim Quest::IsRepeatable() is false for every SQL marker (SpecialFlags=256)");
    Expect(nonSeasonal, "verbatim Quest::IsSeasonal() is false for every SQL marker (QuestSortID=0)");

    // --- Stage sweep: every saved stage 0..18 is wiped by ResetQuests ------
    for (uint8 stage = 0; stage <= 18; ++stage)
    {
        LoadTemplates(false);
        Player bot;
        PrepareSavedBot(bot, stage, false);
        std::string tag = "stage " + Str(stage) + ": ";
        Expect(ip.GetPlayerProgressionFromQuests(&bot) == stage, tag + "IP reads saved stage before reset");

        PlayerbotFactory(&bot).ResetQuests();

        bool anyMarkerLeft = false;
        for (uint32 id : MarkersUpTo(stage))
            anyMarkerLeft = anyMarkerLeft || bot.IsQuestRewarded(id);
        SaveResult saved = Save(bot);
        if (stage == 0)
        {
            Expect(ip.GetPlayerProgressionFromQuests(&bot) == 0 && saved.deleted.empty(),
                   tag + "control: nothing to delete, no DB delete queued");
            continue;
        }
        Expect(ip.GetPlayerProgressionFromQuests(&bot) == 0, tag + "IP reads stage 0 after pinned ResetQuests");
        Expect(!anyMarkerLeft, tag + "every rewarded stage marker removed from m_RewardedQuests");
        Expect(saved.deleted == MarkersUpTo(stage) && saved.inserted.empty(),
               tag + "save queues CHAR_DEL_CHAR_QUESTSTATUS_REWARDED_BY_QUEST for exactly the " + Str(stage) +
                   " marker(s)");
    }

    // --- Stage-8 bot with collateral quests --------------------------------
    {
        LoadTemplates(false);
        Player bot;
        PrepareSavedBot(bot, 8, true);
        Expect(ip.GetPlayerProgressionFromQuests(&bot) == 8, "stage-8 bot with extras: IP reads 8");
        PlayerbotFactory(&bot).ResetQuests();
        SaveResult saved = Save(bot);
        Expect(!bot.IsQuestRewarded(kPvpMarkers[0].id) && saved.deleted.count(kPvpMarkers[0].id),
               "PvP marker " + Str(kPvpMarkers[0].id) + " is also deleted (collateral; NOT a stage marker)");
        Expect(!bot.IsQuestRewarded(kOrdinaryRewarded), "ordinary rewarded quest also deleted (ResetQuests is not IP-aware)");
        Expect(bot.IsQuestRewarded(kRepeatableRewarded) && !saved.deleted.count(kRepeatableRewarded),
               "control: rewarded REPEATABLE quest survives (GetQuestStatus returns NONE for it)");
        Expect(bot.m_QuestStatus.find(kActiveInLog) == bot.m_QuestStatus.end(),
               "active quest-log entry removed by RemoveActiveQuest");
    }

    // --- Control: no reset, no delete --------------------------------------
    {
        LoadTemplates(false);
        Player bot;
        PrepareSavedBot(bot, 8, true);
        SaveResult saved = Save(bot);
        Expect(saved.deleted.empty() && ip.GetPlayerProgressionFromQuests(&bot) == 8,
               "control: without ResetQuests a save queues no delete and stage 8 is kept");
    }

    // --- Counterfactual: 'make the markers repeatable' is NOT a mitigation --
    {
        LoadTemplates(true);
        Player bot;
        PrepareSavedBot(bot, 8, false);
        Expect(ip.GetPlayerProgressionFromQuests(&bot) == 0,
               "counterfactual REPEATABLE markers: IP itself can no longer read stage 8 (would break IP)");
        PlayerbotFactory(&bot).ResetQuests();
        bool allKept = true;
        for (uint32 id : MarkersUpTo(8))
            allKept = allKept && bot.IsQuestRewarded(id);
        Expect(allKept, "counterfactual REPEATABLE markers: ResetQuests skips them (rows kept but unreadable by IP)");
    }

    std::printf("1..%d\n", g_test);
    if (g_failed)
    {
        std::printf("# FAIL: %d expectation(s) about the pinned marker-reset chain did not hold\n", g_failed);
        return 1;
    }
    std::printf("# PASS: pinned ResetQuests deletes IP stage markers and queues the DB delete (extracted bodies only)\n");
    return 0;
}
