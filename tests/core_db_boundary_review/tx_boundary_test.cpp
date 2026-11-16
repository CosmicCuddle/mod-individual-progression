// Focused transaction-boundary test for Core MySQLConnection (THROWAWAY DB ONLY).
#include "MySQLConnection.h"
#include "Transaction.h"
#include "QueryResult.h"
#include "PreparedStatement.h"
#include "MySQLPreparedStatement.h"
#include "Field.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

struct Conn : MySQLConnection {
    using MySQLConnection::MySQLConnection;
    void DoPrepareStatements() override {
        if (m_stmts.empty()) m_stmts.resize(1);
        PrepareStatement(0, "INSERT INTO txt (k, v) VALUES (?, ?)", CONNECTION_SYNCH);
    }
};
struct Task : TransactionTask { using TransactionTask::TransactionTask; using TransactionTask::Execute; };

static int fails = 0;
static void check(bool ok, char const* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++fails; }
static long sql1(std::string const& q) {
    std::string cmd = std::string("mysql -uroot -N --socket=") + std::getenv("HARNESS_MYSQL_SOCKET") + " tx_boundary -e \"" + q + "\"";
    FILE* f = popen(cmd.c_str(), "r"); char buf[64] = {0};
    if (!f || !std::fgets(buf, sizeof buf, f)) { if (f) pclose(f); return -1; }
    pclose(f); return std::atol(buf);
}
static long count(Conn&, std::string const& where) { return sql1("SELECT COUNT(*) FROM txt WHERE " + where); }
static long counter(Conn&) { return sql1("SELECT v FROM txc WHERE id = 1"); }
static std::shared_ptr<TransactionBase> tx(std::initializer_list<char const*> q) {
    auto t = std::make_shared<TransactionBase>();
    for (auto s : q) t->Append(s);
    return t;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: conninfo-direct conninfo-proxy\n"); return 2; }
    MySQLConnectionInfo di(argv[1]), pi(argv[2]);
    Conn d(di); if (d.Open()) { std::puts("cannot open direct"); return 2; }
    Conn p(pi); if (p.Open() || !p.PrepareStatements()) { std::puts("cannot open/prepare proxy"); return 2; }
    d.Execute("DELETE FROM txt"); d.Execute("UPDATE txc SET v = 0 WHERE id = 1");
    bool draft = argc > 3 && std::string(argv[3]) == "draft";

    // R1: ERR on COMMIT, connection kept; then T2 on the same connection.
    int rc1 = p.ExecuteTransaction(tx({"DO 'harness-err-commit'", "INSERT INTO txt VALUES ('r1', 1)"}));
    int rc1b = p.ExecuteTransaction(tx({"INSERT INTO txt VALUES ('r1-t2', 1)"}));
    std::printf("R1 rc=%d t2rc=%d r1rows=%ld t2rows=%ld\n", rc1, rc1b, count(d, "k='r1'"), count(d, "k='r1-t2'"));
    if (draft) { check(rc1 != 0, "R1 failed COMMIT not reported as success");
                 check(count(d, "k='r1'") == 0, "R1 uncommitted work NOT committed by next task");
                 check(rc1b == 0 && count(d, "k='r1-t2'") == 1, "R1 next task commits normally"); }
    else       { check(rc1 == 0 && count(d, "k='r1'") == 1, "R1 baseline defect reproduced (true + leftover committed by next BEGIN)"); }

    // R5: ERR on START TRANSACTION.
    p.Execute("DO 'harness-err-begin'");
    int rc5 = p.ExecuteTransaction(tx({"INSERT INTO txt VALUES ('r5a', 1)", "INSERT INTO txt VALUES ('r5a', 2)"}));
    std::printf("R5 rc=%d rows=%ld\n", rc5, count(d, "k='r5a'"));
    if (draft) { check(rc5 != 0 && count(d, "k='r5a'") == 0, "R5 body not executed after BEGIN failure"); }
    else       { check(count(d, "k='r5a'") == 1, "R5 baseline defect reproduced (partial autocommit write)"); }

    // R8: statement error (duplicate key) then ERR on ROLLBACK; then T2.
    d.Execute("INSERT INTO txt VALUES ('dup', 0)");
    int rc8 = p.ExecuteTransaction(tx({"DO 'harness-err-rollback'", "INSERT INTO txt VALUES ('r8', 1)", "INSERT INTO txt VALUES ('dup', 1)"}));
    int rc8b = p.ExecuteTransaction(tx({"INSERT INTO txt VALUES ('r8-t2', 1)"}));
    std::printf("R8 rc=%d t2rc=%d r8rows=%ld\n", rc8, rc8b, count(d, "k='r8'"));
    if (draft) { check(count(d, "k='r8'") == 0, "R8 failed ROLLBACK leftover not committed by next task");
                 check(rc8b == 0 && count(d, "k='r8-t2'") == 1, "R8 next task commits normally"); }
    else       { check(count(d, "k='r8'") == 1, "R8 baseline defect reproduced (leftover committed)"); }

    // Prepared statement handle usable after the resets above.
    {
        auto* s = new PreparedStatementBase(0, 2);
        s->SetData(0, std::string("prep")); s->SetData(1, uint32(7));
        bool ok = p.Execute(s); delete s;
        check(ok && count(d, "k='prep'") == 1, "prepared statement usable after reset");
    }

    // R4/A4: fake deadlock on attempt 1, COMMIT reply lost on attempt 2 (server committed).
    {
        auto t = tx({"DO 'harness-plan-a4'", "UPDATE txc SET v = v + 1 WHERE id = 1"});
        Task task(t); task.SetConnection(&p);
        bool r = task.Execute();
        long c = counter(d);
        std::printf("A4 result=%d counter=%ld\n", int(r), c);
        if (draft) check(!r && c == 1, "A4 no replay after uncertain COMMIT (counter exactly 1)");
        else       check(c >= 2, "A4 baseline defect reproduced (double apply)");
    }
    std::printf("%s fails=%d\n", draft ? "draft" : "baseline", fails);
    return fails ? 1 : 0;
}
