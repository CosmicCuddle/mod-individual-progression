// ModuleDatabasePool strategy-save outcomes (Core 10b0ec9a), THROWAWAY DB ONLY.
// Uses the exact statement shapes of PlayerbotRepository::Save (DELETE by guid + INSERT rows).
#include "ModuleDatabasePool.h"
#include "MySQLConnection.h"
#include "Transaction.h"
#include "MySQLPreparedStatement.h"
#include <cstdio>
#include <cstdlib>
#include <string>

struct Conn : MySQLConnection {
    using MySQLConnection::MySQLConnection;
    void DoPrepareStatements() override { if (m_stmts.empty()) m_stmts.resize(1); }
};
struct Pool : ModuleDatabasePool {
protected:
    MySQLConnection* CreateConnection(MySQLConnectionInfo& i) override { return new Conn(i); }
    MySQLConnection* CreateAsyncConnection(ProducerConsumerQueue<SQLOperation*>* q, MySQLConnectionInfo& i) override { return new Conn(q, i); }
};
static int fails = 0;
static void check(bool ok, std::string const& w) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", w.c_str()); std::fflush(stdout); if (!ok) ++fails; }
static long sql1(std::string const& q) {
    std::string cmd = std::string("mysql -uroot -N --socket=") + std::getenv("HARNESS_MYSQL_SOCKET") + " tx_pb -e \"" + q + "\"";
    FILE* f = popen(cmd.c_str(), "r"); char b[64] = {0};
    if (!f || !std::fgets(b, sizeof b, f)) { if (f) pclose(f); return -1; }
    pclose(f); return std::atol(b);
}
static std::shared_ptr<TransactionBase> save(int guid, std::string const& tag, std::string const& value) {
    auto t = std::make_shared<TransactionBase>();
    if (!tag.empty()) t->Append("DO '" + tag + "'");
    t->Append("DELETE FROM `playerbots_db_store` WHERE `guid` = " + std::to_string(guid));
    for (char const* k : {"co", "nc", "dead"})
        t->Append("INSERT INTO `playerbots_db_store` (`guid`, `key`, `value`) VALUES (" + std::to_string(guid) + ", '" + k + "', '" + value + "')");
    return t;
}
static char const* name(DatabaseTransactionStatus s) {
    switch (s) { case DatabaseTransactionStatus::Committed: return "Committed"; case DatabaseTransactionStatus::NotStarted: return "NotStarted";
        case DatabaseTransactionStatus::Ambiguous: return "Ambiguous"; case DatabaseTransactionStatus::NotEnqueued: return "NotEnqueued";
        case DatabaseTransactionStatus::FailedUnverified: return "FailedUnverified"; default: return "Other"; }
}
static void expect(DatabaseTransactionResult r, DatabaseTransactionStatus s, std::string const& w) {
    std::printf("  %s -> %s (%d)\n", w.c_str(), name(r.status), r.executionCode); check(r.status == s, w + " is " + name(s));
}
int main(int argc, char** argv) {
    Pool p; p.SetConnectionInfo(argv[1], 0, 1);
    if (p.Open() || !p.PrepareStatements()) { std::puts("cannot open"); return 2; }
    expect(p.DirectCommitTransactionWithResult(save(1, "", "+dps")), DatabaseTransactionStatus::Committed, "strategy save normal");
    check(sql1("SELECT COUNT(*) FROM playerbots_db_store WHERE guid=1 AND value='+dps'") == 3, "normal: 3 rows written");
    expect(p.DirectCommitTransactionWithResult(save(1, "harness-err-commit", "+tank")), DatabaseTransactionStatus::Ambiguous, "strategy save COMMIT error");
    check(sql1("SELECT COUNT(*) FROM playerbots_db_store WHERE guid=1 AND value='+dps'") == 3, "COMMIT error: previous snapshot intact, no partial");
    p.DirectExecute("DO 'harness-err-begin'");
    expect(p.DirectCommitTransactionWithResult(save(1, "", "+heal")), DatabaseTransactionStatus::NotStarted, "strategy save BEGIN error");
    check(sql1("SELECT COUNT(*) FROM playerbots_db_store WHERE guid=1 AND value='+dps'") == 3, "BEGIN error: no DELETE ran (no strategy wipe)");
    // deadlock on attempt 1, COMMIT reply lost on attempt 2: count commits via a trigger counter
    sql1("UPDATE txc SET v = 0 WHERE id = 1");
    { auto t = std::make_shared<TransactionBase>(); t->Append("DO 'harness-plan-a4'"); t->Append("UPDATE txc SET v = v + 1 WHERE id = 1");
      t->Append("DELETE FROM `playerbots_db_store` WHERE `guid` = 2"); t->Append("INSERT INTO `playerbots_db_store` (`guid`, `key`, `value`) VALUES (2, 'co', '+a4')");
      expect(p.DirectCommitTransactionWithResult(t), DatabaseTransactionStatus::Ambiguous, "strategy deadlock then lost COMMIT"); }
    check(sql1("SELECT v FROM txc WHERE id = 1") == 1, "deadlock+lost COMMIT applied exactly once (no replay)");
    expect(p.DirectCommitTransactionWithResult(save(1, "", "+after")), DatabaseTransactionStatus::Committed, "strategy save after faults");
    p.Close();
    std::printf("module fails=%d\n", fails); return fails ? 1 : 0;
}
