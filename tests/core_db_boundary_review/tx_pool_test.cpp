// Pool-level typed outcome tests for CS01 Core child fac74c8c (THROWAWAY DB ONLY).
#include "DatabaseEnv.h"
#include "DatabaseWorkerPool.h"
#include "Transaction.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <string>
#include <thread>
#include <vector>
using CharacterDatabaseWorkerPool = DatabaseWorkerPool<CharacterDatabaseConnection>;

static int fails = 0;
static void check(bool ok, std::string const& what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str()); std::fflush(stdout); if (!ok) ++fails; }
static long sql1(std::string const& q) {
    std::string cmd = std::string("mysql -uroot -N --socket=") + std::getenv("HARNESS_MYSQL_SOCKET") + " tx_pool -e \"" + q + "\"";
    FILE* f = popen(cmd.c_str(), "r"); char buf[64] = {0};
    if (!f || !std::fgets(buf, sizeof buf, f)) { if (f) pclose(f); return -1; }
    pclose(f); return std::atol(buf);
}
static long rows(std::string const& k) { return sql1("SELECT COUNT(*) FROM txt WHERE k='" + k + "'"); }
static char const* name(DatabaseTransactionStatus s) {
    switch (s) {
        case DatabaseTransactionStatus::Committed: return "Committed";
        case DatabaseTransactionStatus::Empty: return "Empty";
        case DatabaseTransactionStatus::NotStarted: return "NotStarted";
        case DatabaseTransactionStatus::Ambiguous: return "Ambiguous";
        case DatabaseTransactionStatus::NotEnqueued: return "NotEnqueued";
        case DatabaseTransactionStatus::CancelledBeforeExecution: return "CancelledBeforeExecution";
        case DatabaseTransactionStatus::FailedUnverified: return "FailedUnverified";
        default: return "Other";
    }
}
static CharacterDatabaseTransaction tx(std::initializer_list<std::string> q) {
    auto t = CharacterDatabase.BeginTransaction();
    for (auto const& s : q) t->Append(s);
    return t;
}
static bool openPool(CharacterDatabaseWorkerPool& p, std::string const& info, uint8 async) {
    p.SetConnectionInfo(info, async, 1);
    return !p.Open() && p.PrepareStatements();
}
static void expect(DatabaseTransactionResult r, DatabaseTransactionStatus s, std::string const& what) {
    std::printf("  %s -> %s (%d)\n", what.c_str(), name(r.status), r.executionCode);
    check(r.status == s, what + " is " + name(s));
}

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    std::string info = argv[1], mode = argv[2];
    auto& db = CharacterDatabase;
    if (mode == "direct") {
        if (!openPool(db, info, 1)) { std::puts("cannot open/prepare"); return 2; }
        { auto t = tx({"INSERT INTO txt VALUES ('d-ok', 1)"}); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::Committed, "direct commit"); }
        check(rows("d-ok") == 1, "direct commit row present");
        { auto t = db.BeginTransaction(); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::Empty, "direct empty"); }
        db.DirectExecute("DO 'harness-err-begin'");
        { auto t = tx({"INSERT INTO txt VALUES ('d-nb', 1)"}); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::NotStarted, "direct BEGIN error"); }
        check(rows("d-nb") == 0, "direct BEGIN error wrote nothing");
        { auto t = tx({"DO 'harness-err-commit'", "INSERT INTO txt VALUES ('d-amb', 1)"}); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::Ambiguous, "direct COMMIT error"); }
        check(rows("d-amb") == 0, "direct COMMIT error leftover discarded");
        db.DirectExecute("INSERT INTO txt VALUES ('d-dup', 0)");
        { auto t = tx({"INSERT INTO txt VALUES ('d-dup', 1)"}); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::FailedUnverified, "direct dup-key"); }
        sql1("UPDATE txc SET v = 0 WHERE id = 1");
        { auto t = tx({"DO 'harness-plan-a4'", "UPDATE txc SET v = v + 1 WHERE id = 1"}); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::Ambiguous, "direct deadlock then lost COMMIT"); }
        check(sql1("SELECT v FROM txc WHERE id = 1") == 1, "direct deadlock loop applied exactly once");
        { auto t = tx({"INSERT INTO txt VALUES ('d-after', 1)"}); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::Committed, "direct commit after faults"); }
        db.Close();
    } else if (mode == "async") {
        if (!openPool(db, info, 1)) { std::puts("cannot open/prepare"); return 2; }
        std::vector<std::future<DatabaseTransactionResult>> fs;
        for (int i = 0; i < 5; ++i) fs.push_back(db.AsyncCommitTransactionWithOutcome(tx({"INSERT INTO txt VALUES ('a" + std::to_string(i) + "', 1)", "INSERT INTO txo (k) VALUES ('a" + std::to_string(i) + "')"})));
        auto amb = db.AsyncCommitTransactionWithOutcome(tx({"DO 'harness-err-commit'", "INSERT INTO txt VALUES ('a-amb', 1)"}));
        auto after = db.AsyncCommitTransactionWithOutcome(tx({"INSERT INTO txt VALUES ('a-after', 1)", "INSERT INTO txo (k) VALUES ('a-after')"}));
        for (int i = 0; i < 5; ++i) expect(fs[i].get(), DatabaseTransactionStatus::Committed, "async #" + std::to_string(i));
        expect(amb.get(), DatabaseTransactionStatus::Ambiguous, "async COMMIT error");
        expect(after.get(), DatabaseTransactionStatus::Committed, "async after Ambiguous on same worker");
        check(rows("a-amb") == 0, "async Ambiguous leftover not committed by next task");
        check(sql1("SELECT GROUP_CONCAT(k ORDER BY seq) FROM txo") == -1 || true, "order read");
        { std::string cmd = std::string("mysql -uroot -N --socket=") + std::getenv("HARNESS_MYSQL_SOCKET") + " tx_pool -e \"SELECT GROUP_CONCAT(k ORDER BY seq) FROM txo\"";
          FILE* f = popen(cmd.c_str(), "r"); char b[256] = {0}; std::fgets(b, sizeof b, f); pclose(f);
          std::printf("  commit order: %s", b); check(std::string(b) == "a0,a1,a2,a3,a4,a-after\n", "async one-worker FIFO commit order"); }
        db.Close();
        expect(db.AsyncCommitTransactionWithOutcome(tx({"INSERT INTO txt VALUES ('a-closed', 1)"})).get(), DatabaseTransactionStatus::NotEnqueued, "async after Close");
        check(rows("a-closed") == 0, "rejected transaction not executed");
    } else if (mode == "notready") {
        CharacterDatabaseWorkerPool p; p.SetConnectionInfo(info, 1, 1);
        if (p.Open()) { std::puts("cannot open"); return 2; }
        expect(p.AsyncCommitTransactionWithOutcome(tx({"INSERT INTO txt VALUES ('n-early', 1)"})).get(), DatabaseTransactionStatus::NotEnqueued, "async before PrepareStatements");
        p.PrepareStatements(); p.Close();
    } else if (mode == "drain") {
        if (!openPool(db, info, 1)) { std::puts("cannot open/prepare"); return 2; }
        std::vector<std::future<DatabaseTransactionResult>> fs;
        fs.push_back(db.AsyncCommitTransactionWithOutcome(tx({"DO SLEEP(2)", "INSERT INTO txt VALUES ('dr-0', 1)"})));
        for (int i = 1; i < 6; ++i) fs.push_back(db.AsyncCommitTransactionWithOutcome(tx({"INSERT INTO txt VALUES ('dr-" + std::to_string(i) + "', 1)"})));
        db.Close(); // graceful drain
        for (std::size_t i = 0; i < fs.size(); ++i) expect(fs[i].get(), DatabaseTransactionStatus::Committed, "drain #" + std::to_string(i));
        check(sql1("SELECT COUNT(*) FROM txt WHERE k LIKE 'dr-%'") == 6, "drain committed all queued");
    } else if (mode == "cancel") {
        std::vector<std::future<DatabaseTransactionResult>> fs;
        {
            CharacterDatabaseWorkerPool p;
            if (!openPool(p, info, 1)) { std::puts("cannot open/prepare"); return 2; }
            fs.push_back(p.AsyncCommitTransactionWithOutcome(tx({"DO SLEEP(2)", "INSERT INTO txt VALUES ('c-0', 1)"})));
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            for (int i = 1; i < 6; ++i) fs.push_back(p.AsyncCommitTransactionWithOutcome(tx({"INSERT INTO txt VALUES ('c-" + std::to_string(i) + "', 1)"})));
        } // destructor: Cancel() without Close()
        expect(fs[0].get(), DatabaseTransactionStatus::Committed, "cancel: in-flight task");
        for (int i = 1; i < 6; ++i) expect(fs[i].get(), DatabaseTransactionStatus::CancelledBeforeExecution, "cancel: queued #" + std::to_string(i));
        check(sql1("SELECT COUNT(*) FROM txt WHERE k LIKE 'c-%'") == 1, "cancelled tasks wrote nothing");
    } else if (mode == "lateclose") {
        // N1: what does the legacy path do with a save submitted after Close()?
        if (!openPool(db, info, 1)) { std::puts("cannot open/prepare"); return 2; }
        db.Close();
        db.CommitTransaction(tx({"INSERT INTO txt VALUES ('late-legacy', 1)"}));
        auto r = db.AsyncCommitTransactionWithOutcome(tx({"INSERT INTO txt VALUES ('late-typed', 1)"})).get();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        long legacy = rows("late-legacy"), typed = rows("late-typed");
        std::printf("  legacy after Close: rows=%ld; typed after Close: %s rows=%ld\n", legacy, name(r.status), typed);
        check(legacy == 0, "legacy CommitTransaction after Close is silently not executed");
        check(r.status == DatabaseTransactionStatus::NotEnqueued && typed == 0, "typed after Close is reported NotEnqueued");
    } else if (mode == "outage") {
        // argv[3]: refuse | dns | dead
        if (!openPool(db, info, 1)) { std::puts("cannot open/prepare"); return 2; }
        std::string tag = std::string("DO 'harness-") + (std::string(argv[3]) == "dns" ? "dns" : std::string(argv[3]) == "dead" ? "dead" : "refuse") + "-after'";
        auto t0 = std::chrono::steady_clock::now();
        { auto t = tx({tag, "INSERT INTO txt VALUES ('o-amb', 1)"}); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::Ambiguous, std::string("COMMIT error then ") + argv[3] + " outage"); }
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0).count();
        std::printf("  reconnect took %llds\n", (long long)secs);
        check(rows("o-amb") == 0, "outage: leftover discarded");
        { auto t = tx({"INSERT INTO txt VALUES ('o-after', 1)"}); expect(db.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::Committed, "commit after reconnect"); }
        db.Close();
    }
    std::printf("%s fails=%d\n", mode.c_str(), fails);
    return fails ? 1 : 0;
}
