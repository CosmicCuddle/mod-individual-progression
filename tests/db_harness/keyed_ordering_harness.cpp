// Keyed save-ordering PROTOTYPE harness (design C rev2, CS01 Part09 decision 2).
// REAL Core database layer (DatabaseWorkerPool/CharacterDatabase with N async
// workers, AsyncCommitTransaction, AsyncQuery, CHAR_REP_CHAR_SETTINGS) and the
// real IP receipt policy; the dispatcher under test is
// keyed_dispatcher_prototype.h. THROWAWAY DATABASE ONLY (run_harness.sh).
//
//   keyed <workers>   K1-K10, K12-K14 (K12 needs >= 2 workers)
//   restart1          K11 part 1: purchase left Unresolved at shutdown
//   restart2          K11 part 2: a NEW process reconciles it from the DB
#include "keyed_dispatcher_prototype.h"
#include "MySQLThreading.h"
#include "IndividualProgressionPaidPurchaseState.h"
#include "IndividualProgressionReceiptPolicy.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace R = IndividualProgressionReceiptPolicy;
namespace S = IndividualProgressionPaidPurchaseState;
using namespace KeyedProto;

namespace
{
    unsigned g_pass = 0, g_fail = 0;
#define CHECK(expr) do { if (expr) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } } while (0)

    constexpr char const* Source = "ip_paid_expansion";
    std::string g_socket; // for the external lock holder (mysql client)

    struct Player
    {
        uint32 guid = 0;
        int64 money = 0;
        std::optional<R::Ledger> settings;
        bool tbcBit = false;
        double sleepSec = 0;    // test knob: make the NEXT built save slow
        bool failNext = false;  // test knob: make the NEXT built save fail (definite)
        bool throwNext = false; // test knob: throw while building the NEXT save
    };

    // Whole-state save (the parts of Player::SaveToDB the paid rule relies on),
    // built lazily at dispatch from CURRENT memory.
    Builder SaveBuilder(Player* p)
    {
        return [p](CharacterDatabaseTransaction& trans)
        {
            if (p->throwNext)
            {
                p->throwNext = false;
                throw std::runtime_error("injected build failure");
            }
            if (p->sleepSec > 0)
            {
                trans->Append(Acore::StringFormat("DO SLEEP({})", p->sleepSec));
                p->sleepSec = 0;
            }
            trans->Append(Acore::StringFormat("UPDATE characters SET money = {} WHERE guid = {}", p->money, p->guid));
            if (p->settings)
            {
                CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_CHAR_SETTINGS);
                stmt->SetData(0, p->guid);
                stmt->SetData(1, Source);
                stmt->SetData(2, R::Serialize(*p->settings));
                trans->Append(stmt);
            }
            if (p->failNext)
            {
                p->failNext = false;
                trans->Append(Acore::StringFormat("INSERT INTO harness_fail (guid) VALUES ({})", p->guid));
            }
            trans->Append(Acore::StringFormat("INSERT INTO harness_tail (guid) VALUES ({})", p->guid));
        };
    }

    struct DbState
    {
        int64 money = -1;
        R::Decoded receipt;
        uint64 markerSeq = 0;
        uint64 journalRows = 0;
        uint64 tailRows = 0;
    };

    uint64 Count(std::string const& sql)
    {
        QueryResult r = CharacterDatabase.Query(sql);
        return r ? r->Fetch()[0].Get<uint64>() : 0;
    }

    DbState ReadDb(uint32 guid)
    {
        DbState s;
        if (QueryResult r = CharacterDatabase.Query("SELECT money FROM characters WHERE guid = {}", guid))
            s.money = r->Fetch()[0].Get<int64>();
        QueryResult r = CharacterDatabase.Query("SELECT data FROM character_settings WHERE guid = {} AND source = '{}'", guid, Source);
        s.receipt = r ? R::DecodeRaw(r->Fetch()[0].IsNull() ? std::string() : r->Fetch()[0].Get<std::string>(), true)
                      : R::DecodeRaw({}, false);
        if (QueryResult m = CharacterDatabase.Query("SELECT seq FROM harness_write_seq WHERE guid = {}", guid))
            s.markerSeq = m->Fetch()[0].Get<uint64>();
        s.journalRows = Count(Acore::StringFormat("SELECT COUNT(*) FROM harness_purchase_journal WHERE guid = {}", guid));
        s.tailRows = Count(Acore::StringFormat("SELECT COUNT(*) FROM harness_tail WHERE guid = {}", guid));
        return s;
    }

    bool DbPaid(DbState const& s) { return s.receipt && R::HasPaidReceipt(s.receipt.ledger, R::Era::Tbc); }

    void Reset(uint32 guid, int64 money)
    {
        for (char const* t : {"character_settings", "harness_tail", "harness_write_seq", "harness_purchase_journal"})
            CharacterDatabase.DirectExecute(Acore::StringFormat("DELETE FROM {} WHERE guid = {}", t, guid));
        CharacterDatabase.DirectExecute(Acore::StringFormat("UPDATE characters SET money = {} WHERE guid = {}", money, guid));
    }

    Player Load(uint32 guid)
    {
        DbState const s = ReadDb(guid);
        Player p;
        p.guid = guid;
        p.money = s.money;
        R::Decoded const ledger = S::CommissionIfAbsent(s.receipt, true);
        if (ledger)
        {
            p.settings = ledger.ledger;
            p.tbcBit = ledger.ledger.fields[R::TbcFlag] != R::FlagNone;
        }
        return p;
    }

    // Debit + stage the TBC receipt in memory (as PurchaseExpansion does).
    void StagePurchase(Player& p, uint32 cost)
    {
        R::Ledger staged = *p.settings;
        R::RecordPurchase(staged, R::Era::Tbc, cost, 1800000000u);
        p.money -= cost;
        p.settings = staged;
    }

    template <class Pred>
    bool RunUntil(Dispatcher& d, Pred pred, uint32 ms)
    {
        Clock::time_point const end = Clock::now() + std::chrono::milliseconds(ms);
        while (!pred() && Clock::now() < end)
        {
            d.Process();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        d.Process();
        return pred();
    }

    bool Drain(Dispatcher& d, uint32 ms = 15000) { return RunUntil(d, [&] { return d.Idle(); }, ms); }

    std::string LoginSql(uint32 guid)
    {
        return Acore::StringFormat("SELECT c.money, s.data FROM characters c LEFT JOIN character_settings s "
            "ON s.guid = c.guid AND s.source = '{}' WHERE c.guid = {}", Source, guid);
    }

    // External session holding the marker row lock (models a server-side
    // transaction still running after the client lost the connection).
    // `effects` (optional) run inside that transaction and COMMIT after the
    // sleep: the server finishing a transaction the client already gave up on.
    void HoldMarkerLock(uint32 guid, double seconds, std::string const& effects = std::string())
    {
        std::string const cmd = Acore::StringFormat(
            "mysql -uroot --socket={} ip_paid_harness -e \"BEGIN; SELECT seq FROM harness_write_seq WHERE guid = {} FOR UPDATE; "
            "{} DO SLEEP({}); COMMIT;\" >/dev/null 2>&1 &", g_socket, guid, effects, seconds);
        CHECK(std::system(cmd.c_str()) == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(400)); // the holder has the lock
    }

    double Ms(Clock::time_point a, Clock::time_point b)
    {
        return std::chrono::duration<double, std::milli>(b - a).count();
    }

    // K1: the reverse hazard (older autosave delayed, newer purchase) under the
    // keyed dispatcher: the purchase waits for the older save; newest kept.
    void K1Reverse(unsigned workers)
    {
        uint32 const g = 1101;
        Reset(g, 1000);
        Player p = Load(g);
        Dispatcher d({}, {});
        uint64 const e = d.BeginSession(g);
        p.sleepSec = 1.5; // the OLDER save is slow in the DB
        Outcome older = Outcome::Unresolved, purchase = Outcome::Unresolved;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { older = o; }) == Admit::Queued);
        RunUntil(d, [&] { return d.StateOf(g) == KeyState::Running; }, 2000); // older save dispatched
        S::Record record = S::Submit(static_cast<uint8>(1), 1, 1);
        StagePurchase(p, 400);
        CHECK(d.SubmitPurchase(g, e, "k1-1101", SaveBuilder(&p), [&](Outcome o)
            {
                purchase = o;
                if (S::Complete(record, 1, o == Outcome::Committed) == S::Completion::GrantAccess)
                    p.tbcBit = true;
            }) == Admit::Queued);
        CHECK(Drain(d));
        DbState const s = ReadDb(g);
        CHECK(older == Outcome::Committed && purchase == Outcome::Committed && p.tbcBit);
        CHECK(s.money == 600 && DbPaid(s) && s.markerSeq == 2 && s.journalRows == 1);
        std::printf("K1 reverse workers=%u: older=%s purchase=%s final money=%lld paid=%d seq=%llu -> %s\n", workers,
            Name(older), Name(purchase), (long long)s.money, DbPaid(s), (unsigned long long)s.markerSeq,
            s.money == 600 && DbPaid(s) ? "newest state kept" : "OVERWRITTEN");
    }

    // K2: relog fence. The old session's logout save is slow; the new session's
    // login read is queued behind it and sees the saved state.
    void K2RelogFence(unsigned workers)
    {
        uint32 const g = 1102;
        Reset(g, 1000);
        Player p = Load(g);
        Dispatcher d({}, {});
        uint64 const e1 = d.BeginSession(g);
        StagePurchase(p, 400); // memory the logout save must carry
        p.sleepSec = 1.0;
        Outcome logout = Outcome::Unresolved;
        CHECK(d.SubmitSave(g, e1, SaveBuilder(&p), [&](Outcome o) { logout = o; }) == Admit::Queued);
        d.EndSession(g, e1);
        uint64 const e2 = d.BeginSession(g);
        CHECK(e2 > e1);
        int64 seenMoney = -1;
        std::string seenData;
        bool delivered = false;
        CHECK(d.SubmitRead(g, e2, LoginSql(g), [&](QueryResult r)
            {
                delivered = true;
                if (r)
                {
                    seenMoney = r->Fetch()[0].Get<int64>();
                    seenData = r->Fetch()[1].IsNull() ? std::string() : r->Fetch()[1].Get<std::string>();
                }
            }) == Admit::Queued);
        CHECK(Drain(d));
        bool const fresh = delivered && seenMoney == 600 && seenData == R::Serialize(*p.settings);
        CHECK(logout == Outcome::Committed && fresh);
        std::printf("K2 relog fence workers=%u: login read money=%lld -> %s\n", workers, (long long)seenMoney,
            fresh ? "FRESH" : "STALE");
    }

    // K3: fence timeout refuses the login; a queued login read is cancelled
    // (removed) and a RUNNING read's late result is discarded; the key advances.
    void K3FenceTimeout()
    {
        uint32 const g = 1103;
        Reset(g, 1000);
        Player p = Load(g);
        Dispatcher d({}, {});
        uint64 const e1 = d.BeginSession(g);
        p.sleepSec = 1.5;
        CHECK(d.SubmitSave(g, e1, SaveBuilder(&p), [](Outcome) {}) == Admit::Queued);
        d.EndSession(g, e1);
        uint64 const e2 = d.BeginSession(g);
        bool delivered = false;
        uint64 ticket = 0;
        CHECK(d.SubmitRead(g, e2, LoginSql(g), [&](QueryResult) { delivered = true; }, &ticket) == Admit::Queued);
        bool const inTime = RunUntil(d, [&] { return delivered; }, 300); // fence timeout (test value)
        CHECK(!inTime);
        bool const refused = !inTime;          // login refused, never admitted on a guess
        CHECK(d.CancelRead(g, ticket));        // session gone: queued read removed
        // Running read cancelled: its result must never be delivered.
        uint64 const e3 = d.BeginSession(g);
        bool slowDelivered = false, nextDelivered = false;
        uint64 slowTicket = 0;
        CHECK(d.SubmitRead(g, e3, Acore::StringFormat("SELECT SLEEP(0.5), money FROM characters WHERE guid = {}", g),
            [&](QueryResult) { slowDelivered = true; }, &slowTicket) == Admit::Queued);
        RunUntil(d, [&] { return d.Queued(g) == 0 && d.StateOf(g) == KeyState::Running; }, 5000);
        CHECK(d.CancelRead(g, slowTicket));
        CHECK(d.SubmitRead(g, e3, LoginSql(g), [&](QueryResult) { nextDelivered = true; }) == Admit::Queued);
        CHECK(Drain(d));
        CHECK(!delivered && !slowDelivered && nextDelivered);
        std::printf("K3 fence timeout: refused=%d queued_read_delivered=%d running_read_delivered=%d next_read_delivered=%d\n",
            refused, delivered, slowDelivered, nextDelivered);
    }

    // K4: definite failure (statement error) -> reconciled NotCommitted; the next
    // whole-state save converges; sequence numbers do not skip a committed write.
    void K4DefiniteFailure()
    {
        uint32 const g = 1104;
        Reset(g, 1000);
        Player p = Load(g);
        Dispatcher d({}, {});
        uint64 const e = d.BeginSession(g);
        p.money = 900;
        p.failNext = true;
        Outcome first = Outcome::Unresolved, second = Outcome::Unresolved;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { first = o; }) == Admit::Queued);
        CHECK(Drain(d));
        DbState s = ReadDb(g);
        CHECK(first == Outcome::NotCommitted && s.money == 1000 && s.markerSeq == 0 && s.tailRows == 0);
        p.money = 800;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { second = o; }) == Admit::Queued);
        CHECK(Drain(d));
        s = ReadDb(g);
        CHECK(second == Outcome::Committed && s.money == 800 && s.markerSeq == 1 && s.tailRows == 1);
        std::printf("K4 definite failure: first=%s second=%s money=%lld seq=%llu\n", Name(first), Name(second),
            (long long)s.money, (unsigned long long)s.markerSeq);
    }

    // K5: lost acknowledgement of a COMMITTED purchase -> Committed after
    // reconciliation; no replay (exactly one transaction, one journal row).
    void K5LostAck()
    {
        uint32 const g = 1105;
        Reset(g, 1000);
        Player p = Load(g);
        Hooks h;
        h.loseAck = [g](uint32 guid, uint64 seq) { return guid == g && seq == 1; };
        Dispatcher d({}, h);
        uint64 const e = d.BeginSession(g);
        StagePurchase(p, 400);
        Outcome o1 = Outcome::Unresolved;
        CHECK(d.SubmitPurchase(g, e, "k5-1105", SaveBuilder(&p), [&](Outcome o) { o1 = o; }) == Admit::Queued);
        CHECK(Drain(d));
        DbState const s = ReadDb(g);
        CHECK(o1 == Outcome::Committed && d.GetStats().reconcileReads == 1);
        CHECK(s.money == 600 && DbPaid(s) && s.journalRows == 1 && s.tailRows == 1 && s.markerSeq == 1);
        std::printf("K5 lost ack: outcome=%s reconcile_reads=%llu tail_rows=%llu journal_rows=%llu\n", Name(o1),
            (unsigned long long)d.GetStats().reconcileReads, (unsigned long long)s.tailRows,
            (unsigned long long)s.journalRows);
    }

    // K6: a replayed purchase id is refused by the journal; DB charged once.
    void K6DuplicatePurchase()
    {
        uint32 const g = 1106;
        Reset(g, 1000);
        Player p = Load(g);
        Dispatcher d({}, {});
        uint64 const e = d.BeginSession(g);
        StagePurchase(p, 400);
        Outcome o1 = Outcome::Unresolved, o2 = Outcome::Unresolved, o3 = Outcome::Unresolved;
        CHECK(d.SubmitPurchase(g, e, "k6-1106", SaveBuilder(&p), [&](Outcome o) { o1 = o; }) == Admit::Queued);
        CHECK(Drain(d));
        // Replay of the same request (e.g. a retried client message): memory is
        // debited again by the buggy path; the journal must stop it.
        p.money -= 400;
        CHECK(d.SubmitPurchase(g, e, "k6-1106", SaveBuilder(&p), [&](Outcome o)
            {
                o2 = o;
                if (o == Outcome::DuplicatePurchase)
                    p.money += 400; // DB proves the earlier charge; undo the replayed debit
            }) == Admit::Queued);
        CHECK(Drain(d));
        DbState s = ReadDb(g);
        CHECK(o1 == Outcome::Committed && o2 == Outcome::DuplicatePurchase);
        CHECK(s.money == 600 && s.journalRows == 1 && s.markerSeq == 1);
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { o3 = o; }) == Admit::Queued);
        CHECK(Drain(d));
        s = ReadDb(g);
        CHECK(o3 == Outcome::Committed && s.money == 600 && DbPaid(s) && s.markerSeq == 2);
        std::printf("K6 duplicate purchase: first=%s replay=%s db_money=%lld journal_rows=%llu\n", Name(o1), Name(o2),
            (long long)s.money, (unsigned long long)s.journalRows);
    }

    // K7: admission lease. Older epoch refused after relog; closed lease refused.
    void K7StaleEpoch()
    {
        uint32 const g = 1107;
        Reset(g, 1000);
        Player p = Load(g);
        Dispatcher d({}, {});
        uint64 const e1 = d.BeginSession(g);
        uint64 const e2 = d.BeginSession(g);
        Admit const stale = d.SubmitSave(g, e1, SaveBuilder(&p), [](Outcome) {});
        CHECK(stale == Admit::StaleEpoch);
        CHECK(d.SubmitSave(g, e2, SaveBuilder(&p), [](Outcome) {}) == Admit::Queued);
        d.EndSession(g, e2);
        Admit const closed = d.SubmitSave(g, e2, SaveBuilder(&p), [](Outcome) {});
        CHECK(closed == Admit::NoSession);
        CHECK(d.SubmitSave(9999, 1, SaveBuilder(&p), [](Outcome) {}) == Admit::NoSession);
        CHECK(Drain(d));
        CHECK(ReadDb(g).tailRows == 1);
        std::printf("K7 lease: old_epoch=%s closed_lease=%s\n", Name(stale), Name(closed));
    }

    // K8: independent keys run in parallel; one key is strictly serial.
    void K8Parallelism(unsigned workers)
    {
        Dispatcher d({}, {});
        Player ps[4];
        for (uint32 i = 0; i < 4; ++i)
        {
            Reset(1111 + i, 1000);
            ps[i] = Load(1111 + i);
        }
        uint64 es[4];
        for (uint32 i = 0; i < 4; ++i)
            es[i] = d.BeginSession(1111 + i);
        CHECK(Drain(d));
        Clock::time_point t0 = Clock::now();
        for (uint32 i = 0; i < 4; ++i)
        {
            ps[i].sleepSec = 1.0;
            d.SubmitSave(1111 + i, es[i], SaveBuilder(&ps[i]), [](Outcome) {});
        }
        CHECK(Drain(d));
        double const parallelMs = Ms(t0, Clock::now());
        t0 = Clock::now();
        ps[0].sleepSec = 1.0;
        d.SubmitSave(1111, es[0], SaveBuilder(&ps[0]), [](Outcome) {});
        RunUntil(d, [&] { return d.StateOf(1111) == KeyState::Running && d.Queued(1111) == 0; }, 1000);
        ps[0].sleepSec = 1.0; // the second save is built only when the first finished
        d.SubmitSave(1111, es[0], SaveBuilder(&ps[0]), [](Outcome) {});
        CHECK(Drain(d));
        double const serialMs = Ms(t0, Clock::now());
        CHECK(serialMs >= 1900.0);
        if (workers >= 4)
            CHECK(parallelMs < 1800.0);
        std::printf("K8 workers=%u: 4 keys x 1s = %.0f ms (parallel), 1 key x 2 saves x 1s = %.0f ms (serial)\n",
            workers, parallelMs, serialMs);
    }

    // K9: coalescing and backpressure; no admitted write is lost.
    void K9CoalesceBackpressure()
    {
        uint32 const g = 1121;
        for (uint32 x : {1121u, 1122u, 1123u, 1124u})
            Reset(x, 1000);
        Limits lim;
        lim.maxPendingPerKey = 2;
        lim.maxTotalPending = 2;
        Dispatcher d(lim, {});
        Player p = Load(g), q = Load(1122);
        uint64 const e = d.BeginSession(g);
        uint64 const eq = d.BeginSession(1122);
        CHECK(Drain(d));
        unsigned committed = 0, queued = 0, coalesced = 0;
        p.sleepSec = 1.0;
        p.money = 1;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { committed += o == Outcome::Committed; }) == Admit::Queued);
        RunUntil(d, [&] { return d.StateOf(g) == KeyState::Running && d.Queued(g) == 0; }, 1000);
        for (int i = 2; i <= 50; ++i)
        {
            p.money = i;
            Admit const a = d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { committed += o == Outcome::Committed; });
            queued += a == Admit::Queued;
            coalesced += a == Admit::Coalesced;
        }
        CHECK(queued == 1 && coalesced == 48 && d.Queued(g) == 1);
        CHECK(d.SubmitRead(g, e, LoginSql(g), [](QueryResult) {}) == Admit::Queued);  // depth 2
        CHECK(d.SubmitRead(g, e, LoginSql(g), [](QueryResult) {}) == Admit::Backpressure);
        p.money = 51;
        Admit const bp = d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { committed += o == Outcome::Committed; });
        CHECK(bp == Admit::Backpressure);        // tail is a read: cannot merge, depth cap
        // Global cap (2 queued over all keys) is now full: another key's write
        // and a new login are refused, though that key is idle.
        Admit const globalBp = d.SubmitSave(1122, eq, SaveBuilder(&q), [&](Outcome o) { committed += o == Outcome::Committed; });
        CHECK(globalBp == Admit::Backpressure);
        uint64 const er = d.BeginSession(1123);
        CHECK(er == 0);
        CHECK(Drain(d));
        // Callers retry the refused writes (memory stayed dirty): admitted now.
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { committed += o == Outcome::Committed; }) == Admit::Queued);
        q.money = 77;
        CHECK(d.SubmitSave(1122, eq, SaveBuilder(&q), [&](Outcome o) { committed += o == Outcome::Committed; }) == Admit::Queued);
        CHECK(d.BeginSession(1123) != 0);
        CHECK(Drain(d));
        DbState const s = ReadDb(g);
        CHECK(committed == 52 && s.money == 51 && s.tailRows == 3 && ReadDb(1122).money == 77);
        // Per-key cap alone (global cap far away): depth 1 refuses a second queued op.
        Limits one;
        one.maxPendingPerKey = 1;
        one.maxTotalPending = 100;
        Dispatcher d1(one, {});
        Player k = Load(1124);
        uint64 const ek = d1.BeginSession(1124);
        CHECK(Drain(d1));
        k.sleepSec = 0.5;
        CHECK(d1.SubmitSave(1124, ek, SaveBuilder(&k), [](Outcome) {}) == Admit::Queued);     // running
        CHECK(d1.SubmitRead(1124, ek, LoginSql(1124), [](QueryResult) {}) == Admit::Queued);   // depth 1
        Admit const perKeyOnly = d1.SubmitRead(1124, ek, LoginSql(1124), [](QueryResult) {});
        CHECK(perKeyOnly == Admit::Backpressure);
        CHECK(Drain(d1));
        std::printf("K9 coalesce/backpressure: per_key_only_cap=%s; saves submitted=51 queued=%u coalesced=%u read_refused=1 per_key_save_refused=%s "
            "global_save_refused=%s login_refused=%d callbacks_committed=%u db_writes=%llu final_money=%lld (all refused writes "
            "admitted on retry)\n", Name(perKeyOnly), queued + 1, coalesced, Name(bp), Name(globalBp), er == 0, committed,
            (unsigned long long)s.tailRows, (long long)s.money);
    }

    // K10: shutdown drains every key, then refuses admission.
    void K10Shutdown()
    {
        Dispatcher d({}, {});
        Player ps[4];
        uint64 es[4];
        unsigned committed = 0;
        for (uint32 i = 0; i < 4; ++i)
        {
            Reset(1131 + i, 1000);
            ps[i] = Load(1131 + i);
            es[i] = d.BeginSession(1131 + i);
        }
        for (uint32 i = 0; i < 4; ++i)
        {
            ps[i].sleepSec = 0.5;
            ps[i].money = 100 + i;
            d.SubmitSave(1131 + i, es[i], SaveBuilder(&ps[i]), [&](Outcome o) { committed += o == Outcome::Committed; });
            ps[i].money = 200 + i; // queued behind (coalesces into a second op)
            d.SubmitSave(1131 + i, es[i], SaveBuilder(&ps[i]), [&](Outcome o) { committed += o == Outcome::Committed; });
        }
        bool const drained = d.Shutdown(10000);
        CHECK(drained && committed == 8);
        CHECK(d.SubmitSave(1131, es[0], SaveBuilder(&ps[0]), [](Outcome) {}) == Admit::ShuttingDown);
        CHECK(d.BeginSession(1131) == 0);
        bool allFinal = true;
        for (uint32 i = 0; i < 4; ++i)
            allFinal = allFinal && ReadDb(1131 + i).money == 200 + i;
        CHECK(allFinal);
        std::printf("K10 shutdown: drained=%d committed_callbacks=%u final_states_persisted=%d\n", drained, committed, allFinal);
    }

    // K12: an Uncertain key blocks its later ops until reconciliation proves the
    // outcome (the server-side transaction is modelled by an external lock
    // holder); an independent key proceeds; a stall refuses new logins.
    void K12UncertainBlocks(unsigned workers)
    {
        uint32 const g = 1141, other = 1142;
        Reset(g, 1000);
        Reset(other, 1000);
        Player p = Load(g), q = Load(other);
        bool holderStarted = false, stalled = false;
        Hooks h;
        h.loseAck = [g](uint32 guid, uint64 seq) { return guid == g && seq == 1; };
        h.beforeReconcile = [&](uint32 guid)
        {
            if (guid == g && !holderStarted)
            {
                holderStarted = true;
                HoldMarkerLock(g, 3.0);
            }
        };
        h.onStalled = [&](uint32 guid) { stalled = stalled || guid == g; };
        Limits lim;
        lim.stallMs = 1000;
        Dispatcher d(lim, h);
        uint64 const e = d.BeginSession(g), eo = d.BeginSession(other);
        CHECK(Drain(d));
        Outcome o1 = Outcome::Unresolved, o2 = Outcome::Unresolved, oOther = Outcome::Unresolved;
        p.money = 700;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { o1 = o; }) == Admit::Queued);
        RunUntil(d, [&] { return holderStarted; }, 3000);
        p.money = 500;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { o2 = o; }) == Admit::Queued);
        q.money = 42;
        Clock::time_point const t0 = Clock::now();
        CHECK(d.SubmitSave(other, eo, SaveBuilder(&q), [&](Outcome o) { oOther = o; }) == Admit::Queued);
        RunUntil(d, [&] { return oOther == Outcome::Committed; }, 2500);
        double const otherMs = Ms(t0, Clock::now());
        bool const otherProceeded = oOther == Outcome::Committed;
        RunUntil(d, [&] { return stalled; }, 2000);
        bool const loginBlockedDuring = d.LoginBlocked(g);
        DbState const mid = ReadDb(g);
        bool const blockedMid = o1 == Outcome::Unresolved && o2 == Outcome::Unresolved && mid.money == 700 && mid.tailRows == 1;
        CHECK(Drain(d, 10000));
        DbState const s = ReadDb(g);
        CHECK(stalled && loginBlockedDuring && blockedMid);
        CHECK(!d.LoginBlocked(g));
        CHECK(o1 == Outcome::Committed && o2 == Outcome::Committed && s.money == 500 && s.tailRows == 2 && s.markerSeq == 2);
        CHECK(otherProceeded);
        std::printf("K12 uncertain workers=%u: blocked_while_uncertain=%d stalled=%d login_refused=%d other_key_committed_in=%.0fms "
            "first=%s queued=%s final_money=%lld writes=%llu reconcile_reads=%llu\n", workers, blockedMid, stalled,
            loginBlockedDuring, otherMs, Name(o1), Name(o2), (long long)s.money, (unsigned long long)s.tailRows,
            (unsigned long long)d.GetStats().reconcileReads);
    }

    // K13: an exception while building a queued write still advances the key:
    // the read and the save queued behind it run.
    void K13BuildException()
    {
        uint32 const g = 1108;
        Reset(g, 1000);
        Player p = Load(g);
        Dispatcher d({}, {});
        uint64 const e = d.BeginSession(g);
        CHECK(Drain(d));
        Outcome o0 = Outcome::Unresolved, o1 = Outcome::Unresolved, o2 = Outcome::Unresolved;
        bool readDelivered = false;
        p.sleepSec = 0.5;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { o0 = o; }) == Admit::Queued); // running
        p.throwNext = true;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { o1 = o; }) == Admit::Queued); // will throw
        CHECK(d.SubmitRead(g, e, LoginSql(g), [&](QueryResult) { readDelivered = true; }) == Admit::Queued);
        p.money = 321;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { o2 = o; }) == Admit::Queued);
        CHECK(Drain(d));
        DbState const s = ReadDb(g);
        CHECK(o0 == Outcome::Committed && o1 == Outcome::BuildFailed && readDelivered && o2 == Outcome::Committed);
        CHECK(s.money == 321 && s.tailRows == 2 && d.GetStats().buildFailures == 1);
        std::printf("K13 build exception: before=%s throwing=%s read_after_delivered=%d save_after=%s money=%lld\n",
            Name(o0), Name(o1), readDelivered, Name(o2), (long long)s.money);
    }

    // K14: the client was told "false" but the server finishes the transaction
    // LATER (modelled: the dispatcher's own attempt fails, an external session
    // holding the marker row commits the same effects after 2.5 s). The locking
    // reconciliation read waits and reports Committed; a plain read would have
    // reported NotCommitted and reused the sequence number.
    void K14LateServerCommit()
    {
        uint32 const g = 1109;
        Reset(g, 1000);
        Player p = Load(g);
        bool holder = false;
        Hooks h;
        h.beforeReconcile = [&](uint32 guid)
        {
            if (guid == g && !holder)
            {
                holder = true;
                HoldMarkerLock(g, 2.5, Acore::StringFormat(
                    "UPDATE harness_write_seq SET seq = 1 WHERE guid = {}; UPDATE characters SET money = 700 WHERE guid = {};", g, g));
            }
        };
        Dispatcher d({}, h);
        uint64 const e = d.BeginSession(g);
        CHECK(Drain(d));
        Outcome o1 = Outcome::Unresolved, o2 = Outcome::Unresolved;
        p.money = 700;
        p.failNext = true;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { o1 = o; }) == Admit::Queued);
        RunUntil(d, [&] { return holder; }, 3000);
        p.money = 650;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome o) { o2 = o; }) == Admit::Queued);
        CHECK(Drain(d, 10000));
        DbState const s = ReadDb(g);
        CHECK(o1 == Outcome::Committed && o2 == Outcome::Committed && s.markerSeq == 2 && s.money == 650);
        std::printf("K14 late server commit: first=%s next=%s marker=%llu money=%lld reconcile_reads=%llu\n", Name(o1),
            Name(o2), (unsigned long long)s.markerSeq, (long long)s.money, (unsigned long long)d.GetStats().reconcileReads);
    }

    // K11 part 1: lost ack of a COMMITTED purchase while the marker row stays
    // locked; shutdown cannot reconcile in time -> Unresolved (fail closed).
    int Restart1()
    {
        uint32 const g = 1151;
        Reset(g, 1000);
        Player p = Load(g);
        Hooks h;
        bool holder = false;
        h.loseAck = [g](uint32 guid, uint64 seq) { return guid == g && seq == 1; };
        h.beforeReconcile = [&](uint32 guid)
        {
            if (guid == g && !holder)
            {
                holder = true;
                // A further transaction of the dying process (seq 2) is still
                // running server-side and commits after this process exits.
                HoldMarkerLock(g, 4.0, Acore::StringFormat("UPDATE harness_write_seq SET seq = 2 WHERE guid = {};", g));
            }
        };
        Dispatcher d({}, h);
        uint64 const e = d.BeginSession(g);
        StagePurchase(p, 400);
        Outcome o = Outcome::Committed;
        bool granted = false;
        CHECK(d.SubmitPurchase(g, e, "k11-1151", SaveBuilder(&p), [&](Outcome out)
            {
                o = out;
                granted = out == Outcome::Committed;
            }) == Admit::Queued);
        RunUntil(d, [&] { return holder; }, 3000);
        bool const drained = d.Shutdown(1500);
        CHECK(!drained && o == Outcome::Unresolved && !granted);
        std::printf("K11a restart part 1: shutdown drained=%d purchase=%s access_granted=%d\n", drained, Name(o), granted);
        return 0;
    }

    // K11 part 2 (NEW process): the locking init read waits for the server-side
    // transaction, the sequence continues, the journal answers the purchase.
    int Restart2()
    {
        uint32 const g = 1151;
        Dispatcher d({}, {});
        Clock::time_point const t0 = Clock::now();
        uint64 const e = d.BeginSession(g);
        CHECK(Drain(d, 10000));
        double const initMs = Ms(t0, Clock::now());
        uint64 const seqAfterInit = d.LastSeq(g);
        QueryResult j = CharacterDatabase.Query("SELECT seq FROM harness_purchase_journal WHERE purchase_id = 'k11-1151'");
        bool const purchaseCommitted = j && j->Fetch()[0].Get<uint64>() <= seqAfterInit;
        Player p = Load(g); // DB truth: the receipt grants access
        Outcome o = Outcome::Unresolved;
        CHECK(d.SubmitSave(g, e, SaveBuilder(&p), [&](Outcome out) { o = out; }) == Admit::Queued);
        CHECK(Drain(d));
        DbState const s = ReadDb(g);
        CHECK(seqAfterInit == 2 && purchaseCommitted && p.tbcBit);
        CHECK(o == Outcome::Committed && s.markerSeq == 3 && s.money == 600 && DbPaid(s) && s.journalRows == 1);
        std::printf("K11b restart part 2: init waited %.0f ms, seq after init=%llu, journal says committed=%d, "
            "relog access=%d, next save seq=%llu money=%lld\n", initMs, (unsigned long long)seqAfterInit,
            purchaseCommitted, p.tbcBit, (unsigned long long)s.markerSeq, (long long)s.money);
        return 0;
    }
}

int main(int argc, char** argv)
{
    std::string const mode = argc >= 3 ? argv[2] : "";
    if (argc < 3 || (mode != "keyed" && mode != "restart1" && mode != "restart2") || (mode == "keyed" && argc != 4))
    {
        std::printf("usage: %s \"host;port;user;password;database\" keyed <workers> | restart1 | restart2\n", argv[0]);
        return 2;
    }
    char const* sock = std::getenv("HARNESS_MYSQL_SOCKET");
    g_socket = sock ? sock : "";
    unsigned const workers = mode == "keyed" ? static_cast<unsigned>(std::stoul(argv[3])) : 2;
    MySQL::Library_Init();
    CharacterDatabase.SetConnectionInfo(argv[1], static_cast<uint8>(workers), 1);
    if (CharacterDatabase.Open() || !CharacterDatabase.PrepareStatements())
    {
        std::printf("cannot open/prepare the throwaway character database\n");
        return 2;
    }
    if (mode == "restart1")
        Restart1();
    else if (mode == "restart2")
        Restart2();
    else
    {
        K1Reverse(workers);
        K2RelogFence(workers);
        K3FenceTimeout();
        K4DefiniteFailure();
        K5LostAck();
        K6DuplicatePurchase();
        K7StaleEpoch();
        K8Parallelism(workers);
        K9CoalesceBackpressure();
        K10Shutdown();
        if (workers >= 2)
            K12UncertainBlocks(workers);
        else
            std::printf("K12 skipped at workers=1: a lock-waiting reconciliation read occupies the only worker (documented)\n");
        K13BuildException();
        K14LateServerCommit();
    }
    CharacterDatabase.Close();
    MySQL::Library_End();
    std::printf("KEYED ORDERING HARNESS mode=%s workers=%u %u/%u passed\n", mode.c_str(), workers, g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
