// Injected persistence-failure harness for the paid purchase durability rule
// (IP API v2). REAL Core database layer: DatabaseWorkerPool (CharacterDatabase),
// Transaction/TransactionWithResultTask, AsyncCommitTransaction and
// AsyncCallbackProcessor<TransactionCallback>, the real prepared statement
// CHAR_REP_CHAR_SETTINGS, plus the real IP receipt and purchase-state headers.
// MODELLED: the Player object (money + settings map) and the parts of
// Player::SaveToDB this rule relies on (one transaction: characters.money
// first, every settings source later, more statements after).
// Failures are injected by DB triggers controlled by harness_ctl.fail_stage:
//   1 = characters UPDATE fails (start of the save)
//   2 = a statement AFTER the receipt REPLACE fails (end of the save)
//   3 = the characters UPDATE is SLOW (SLEEP 2): a logout save still in flight
// Mode "ordering <asyncThreads>": fast relog - a slow logout save, then an
// immediately queued async login read (as Core's DelayQueryHolder) and the
// sync read IP does after it. Reports what the login read observed.
// Run only against a THROWAWAY database (see run_harness.sh).
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "MySQLThreading.h"
#include "AsyncCallbackProcessor.h"
#include "QueryCallback.h"
#include "IndividualProgressionPaidPurchaseState.h"
#include "IndividualProgressionReceiptPolicy.h"

#include <chrono>
#include <cstdio>
#include <optional>
#include <string>
#include <thread>

namespace R = IndividualProgressionReceiptPolicy;
namespace S = IndividualProgressionPaidPurchaseState;

namespace
{
    unsigned g_pass = 0, g_fail = 0;
#define CHECK(expr) do { if (expr) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

    constexpr char const* Source = "ip_paid_expansion";
    AsyncCallbackProcessor<TransactionCallback> g_callbacks;

    struct Player
    {
        uint32 guid;
        int64 money;
        std::optional<R::Ledger> settings; // in-memory ip_paid_expansion source
        S::Record record;
        bool tbcBit = false;
        uint64 nextToken = 0;
    };

    void SetFailStage(int stage)
    {
        CharacterDatabase.DirectExecute(Acore::StringFormat("UPDATE harness_ctl SET fail_stage = {}", stage));
    }

    // The parts of Player::SaveToDB(trans) this rule relies on.
    void AppendSave(CharacterDatabaseTransaction trans, Player const& p)
    {
        trans->Append(Acore::StringFormat("UPDATE characters SET money = {} WHERE guid = {}", p.money, p.guid));
        if (p.settings)
        {
            CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHAR_SETTINGS);
            stmt->SetData(0, p.guid);
            stmt->SetData(1, Source);
            stmt->SetData(2, R::Serialize(*p.settings));
            trans->Append(stmt);
        }
        trans->Append(Acore::StringFormat("INSERT INTO harness_tail (guid) VALUES ({})", p.guid));
    }

    void Pump(bool const& done)
    {
        for (int i = 0; i < 2000 && !done; ++i)
        {
            g_callbacks.ProcessReadyCallbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // Fire-and-forget like Player::SaveToDB(bool,bool); then drain the async
    // queue (FIFO, one async thread) with a callback-tracked sentinel.
    void OrdinarySave(Player const& p)
    {
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        AppendSave(trans, p);
        CharacterDatabase.CommitTransaction(trans);
        CharacterDatabaseTransaction sentinel = CharacterDatabase.BeginTransaction();
        sentinel->Append("DO 0");
        bool done = false;
        g_callbacks.AddCallback(CharacterDatabase.AsyncCommitTransaction(sentinel)).AfterComplete([&done](bool) { done = true; });
        Pump(done);
    }

    struct DbState
    {
        int64 money = -1;
        R::Decoded receipt;
    };

    DbState ReadDb(uint32 guid)
    {
        DbState s;
        if (QueryResult r = CharacterDatabase.Query("SELECT money FROM characters WHERE guid = {}", guid))
            s.money = r->Fetch()[0].Get<int64>();
        QueryResult r = CharacterDatabase.Query("SELECT data FROM character_settings WHERE guid = {} AND source = '{}'", guid, Source);
        s.receipt = r ? R::DecodeRaw(r->Fetch()[0].IsNull() ? std::string() : r->Fetch()[0].Get<std::string>(), true)
                      : R::DecodeRaw({}, false);
        return s;
    }

    // Login: DB truth through the strict decoder (absent row: empty v1, as
    // with CommissionMissingReceipt = 1).
    Player Login(uint32 guid)
    {
        DbState const s = ReadDb(guid);
        Player p{guid, s.money, std::nullopt, {}, false, 0};
        R::Decoded const ledger = S::CommissionIfAbsent(s.receipt, true);
        if (ledger)
        {
            p.settings = ledger.ledger;
            p.tbcBit = ledger.ledger.fields[R::TbcFlag] != R::FlagNone;
        }
        return p;
    }

    enum class Submit { Submitted, Pending, Refused };

    // Mirrors IndividualProgression::PurchaseExpansion (TBC) step by step.
    Submit Purchase(Player& p, uint32 cost, bool& done, bool& committedResult)
    {
        if (!S::MayStartPurchase(p.record))
            return Submit::Pending;
        if (!p.settings || p.tbcBit || p.money < cost)
            return Submit::Refused;
        R::Ledger staged = *p.settings;
        if (!R::RecordPurchase(staged, R::Era::Tbc, cost, 1800000000u))
            return Submit::Refused;
        uint64 const token = ++p.nextToken;
        p.record = S::Submit(static_cast<uint8>(1), token, 1);
        p.money -= cost;
        p.settings = staged;
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        AppendSave(trans, p);
        Player* player = &p;
        g_callbacks.AddCallback(CharacterDatabase.AsyncCommitTransaction(trans)).AfterComplete(
            [player, token, &done, &committedResult](bool committed)
            {
                committedResult = committed;
                if (S::Complete(player->record, token, committed) == S::Completion::GrantAccess)
                    player->tbcBit = true;
                done = true;
            });
        return Submit::Submitted;
    }

    void Reset(uint32 guid, int64 money)
    {
        SetFailStage(0);
        CharacterDatabase.DirectExecute(Acore::StringFormat("DELETE FROM character_settings WHERE guid = {}", guid));
        CharacterDatabase.DirectExecute(Acore::StringFormat("DELETE FROM harness_tail WHERE guid = {}", guid));
        CharacterDatabase.DirectExecute(Acore::StringFormat("UPDATE characters SET money = {} WHERE guid = {}", money, guid));
    }

    bool DbPaid(DbState const& s)
    {
        return s.receipt && R::HasPaidReceipt(s.receipt.ledger, R::Era::Tbc);
    }

    void ScenarioSuccess(uint32 guid)
    {
        Reset(guid, 1000);
        Player p = Login(guid);
        bool done = false, committed = false;
        CHECK(Purchase(p, 400, done, committed) == Submit::Submitted);
        CHECK(!p.tbcBit && p.record.state == S::State::Pending);
        CHECK(Purchase(p, 400, done, committed) == Submit::Pending);  // duplicate click while pending
        Pump(done);
        CHECK(done && committed);
        CHECK(p.tbcBit && p.record.state == S::State::Confirmed);
        DbState const s = ReadDb(guid);
        CHECK(s.money == 600 && DbPaid(s));
        std::printf("S1 success: callback=%d db_money=%lld db_receipt=%s\n", committed, (long long)s.money,
            s.receipt ? R::Serialize(s.receipt.ledger).c_str() : "invalid");
    }

    void ScenarioFirstFailLaterSuccess(uint32 guid)
    {
        Reset(guid, 1000);
        Player p = Login(guid);
        SetFailStage(1);
        bool done = false, committed = true;
        CHECK(Purchase(p, 400, done, committed) == Submit::Submitted);
        Pump(done);
        CHECK(done && !committed);
        CHECK(p.record.state == S::State::Unconfirmed && !p.tbcBit && p.money == 600); // no refund, no access
        DbState s = ReadDb(guid);
        CHECK(s.money == 1000 && !s.receipt && s.receipt.status == R::Status::Missing); // neither persisted
        bool d2 = false, c2 = false;
        CHECK(Purchase(p, 400, d2, c2) == Submit::Pending);                 // no second debit
        SetFailStage(0);
        OrdinarySave(p);                                                     // later autosave
        s = ReadDb(guid);
        CHECK(s.money == 600 && DbPaid(s));                                  // both, together
        Player relog = Login(guid);
        CHECK(relog.tbcBit && relog.money == 600);
        std::printf("S2 first-fail/later-success: callback=%d after_autosave db_money=%lld paid=%d relog_bit=%d\n",
            committed, (long long)s.money, DbPaid(s), relog.tbcBit);
    }

    void ScenarioFailAfterReceiptStatement(uint32 guid)
    {
        Reset(guid, 1000);
        Player p = Login(guid);
        SetFailStage(2);
        bool done = false, committed = true;
        CHECK(Purchase(p, 400, done, committed) == Submit::Submitted);
        Pump(done);
        CHECK(done && !committed);
        DbState const s = ReadDb(guid);
        CHECK(s.money == 1000 && s.receipt.status == R::Status::Missing);   // REPLACE rolled back
        std::printf("S3 fail-after-receipt-statement: callback=%d db_money=%lld receipt_status=%d (1=Missing)\n",
            committed, (long long)s.money, static_cast<int>(s.receipt.status));
    }

    void ScenarioBothFail(uint32 guid)
    {
        Reset(guid, 1000);
        Player p = Login(guid);
        SetFailStage(1);
        bool done = false, committed = true;
        Purchase(p, 400, done, committed);
        Pump(done);
        CHECK(done && !committed);
        OrdinarySave(p);                                                     // logout save fails too
        DbState s = ReadDb(guid);
        CHECK(s.money == 1000 && s.receipt.status == R::Status::Missing);
        SetFailStage(0);
        Player relog = Login(guid);                                          // DB truth: never bought
        CHECK(relog.money == 1000 && !relog.tbcBit);
        bool d2 = false, c2 = false;
        CHECK(Purchase(relog, 400, d2, c2) == Submit::Submitted);
        Pump(d2);
        CHECK(c2);
        s = ReadDb(guid);
        CHECK(s.money == 600 && DbPaid(s));                                  // charged exactly once in DB
        std::printf("S4 both-fail then relog+buy: db_money=%lld paid=%d\n", (long long)s.money, DbPaid(s));
    }

    // Fast relog: logout save (fire-and-forget, as Player::SaveToDB) still in
    // flight when the next login's async read is queued.
    int ScenarioFastRelogOrdering(uint32 guid, unsigned asyncThreads)
    {
        Reset(guid, 1000);
        Player p = Login(guid);
        R::Ledger staged = *p.settings;
        R::RecordPurchase(staged, R::Era::Tbc, 400, 1800000000u);
        p.money = 600;
        p.settings = staged;
        SetFailStage(3);                                      // slow save
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        AppendSave(trans, p);
        CharacterDatabase.CommitTransaction(trans);           // logout save
        bool done = false;
        int64 seenMoney = -1;
        std::string seenData = "<no row>";
        AsyncCallbackProcessor<QueryCallback> queries;
        queries.AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat(
            "SELECT c.money, s.data FROM characters c LEFT JOIN character_settings s ON s.guid = c.guid "
            "AND s.source = '{}' WHERE c.guid = {}", Source, guid)).WithCallback(
            [&](QueryResult r)
            {
                if (r)
                {
                    Field* f = r->Fetch();
                    seenMoney = f[0].Get<int64>();
                    seenData = f[1].IsNull() ? std::string("<no row>") : f[1].Get<std::string>();
                }
                done = true;
            }));
        for (int i = 0; i < 2000 && !done; ++i)
        {
            queries.ProcessReadyCallbacks();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        // IP's strict sync read runs after the login holder completed.
        DbState const syncRead = ReadDb(guid);
        bool const loginSawSave = seenMoney == 600 && seenData == R::Serialize(staged);
        bool const syncSawSave = syncRead.money == 600 && DbPaid(syncRead);
        std::printf("ORDERING async_threads=%u: login_read money=%lld receipt=\"%s\" -> %s; ip_sync_read_after -> %s\n",
            asyncThreads, (long long)seenMoney, seenData.c_str(), loginSawSave ? "FRESH" : "STALE",
            syncSawSave ? "FRESH" : "STALE");
        SetFailStage(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(2500)); // let the slow save finish
        return loginSawSave && syncSawSave ? 0 : 1;
    }

    void ScenarioMalformedNeverOverwritten(uint32 guid)
    {
        Reset(guid, 1000);
        CharacterDatabase.DirectExecute(Acore::StringFormat(
            "INSERT INTO character_settings (guid, source, data) VALUES ({}, '{}', '1 0 0 ')", guid, Source));
        Player p = Login(guid);
        CHECK(!p.settings);                                                  // legacy row refused
        bool done = false, committed = false;
        CHECK(Purchase(p, 400, done, committed) == Submit::Refused);
        CHECK(p.money == 1000);
        QueryResult r = CharacterDatabase.Query("SELECT data FROM character_settings WHERE guid = {}", guid);
        CHECK(r && r->Fetch()[0].Get<std::string>() == "1 0 0 ");
        std::printf("S5 legacy row: refused, row unchanged\n");
    }
}

int main(int argc, char** argv)
{
    bool const ordering = argc == 4 && std::string(argv[2]) == "ordering";
    if (argc != 2 && !ordering)
    {
        std::printf("usage: %s \"host;port;user;password;database\" [ordering <asyncThreads>]\n", argv[0]);
        return 2;
    }
    unsigned const asyncThreads = ordering ? static_cast<unsigned>(std::stoul(argv[3])) : 1;
    MySQL::Library_Init();
    CharacterDatabase.SetConnectionInfo(argv[1], static_cast<uint8>(asyncThreads), 1);
    if (CharacterDatabase.Open() || !CharacterDatabase.PrepareStatements())
    {
        std::printf("cannot open/prepare the throwaway character database\n");
        return 2;
    }
    if (ordering)
    {
        int const stale = ScenarioFastRelogOrdering(1006, asyncThreads);
        CharacterDatabase.Close();
        MySQL::Library_End();
        // Exit 0 = fresh, 3 = stale observed (a finding, not a harness error).
        return stale ? 3 : 0;
    }
    ScenarioSuccess(1001);
    ScenarioFirstFailLaterSuccess(1002);
    ScenarioFailAfterReceiptStatement(1003);
    ScenarioBothFail(1004);
    ScenarioMalformedNeverOverwritten(1005);
    CharacterDatabase.Close();
    MySQL::Library_End();
    std::printf("PAID PERSISTENCE DB HARNESS %u/%u passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
