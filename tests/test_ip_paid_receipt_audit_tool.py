"""Offline paid receipt audit tool: exact DecodeRaw parity (differential test
against the real C++ header), fixtures, and a read-only SQL contract."""
import random
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import ip_paid_receipt_audit as audit  # noqa: E402

SQL = (ROOT / "tools/ip_paid_receipt_audit.sql").read_text(encoding="utf-8")


def corpus(seed=20261114, count=30000):
    rng = random.Random(seed)
    out = [(False, b""), (True, b""), (True, b" "), (True, b"  ")]
    canonical = [
        [0, 0, 0, 1, 0, 0, 0, 0], [1, 0, 1, 1, 400, 1800000000, 0, 0],
        [1, 1, 1, 1, 400, 1800000000, 900, 1800000001], [2, 2, 1, 1, 0, 0, 0, 0],
        [0, 2, 1, 1, 0, 0, 0, 0], [1, 0, 0, 1, 2**31 - 1, 2**32 - 1, 0, 0],
    ]
    for f in canonical:
        s = audit.serialize(f)
        out += [(True, s), (True, s[:-1]), (True, s + b" "), (True, b" " + s), (True, s.replace(b" ", b"  ", 1)),
                (False, s), (True, s + b"0 "), (True, s.replace(b" ", b"\t", 1))]
    alphabet = [b"0", b"1", b"2", b"3", b"9", b" ", b"-", b"x", b"\t", b"\xff", b"01", b"4294967295",
                b"4294967296", b"2147483647", b"2147483648", b"00"]
    for _ in range(count):
        mode = rng.random()
        if mode < 0.45:
            # mutate a near-valid ledger
            f = list(rng.choice(canonical))
            f[rng.randrange(8)] = rng.choice([0, 1, 2, 3, 7, 2**31 - 1, 2**31, 2**32 - 1])
            s = audit.serialize(f)
            if rng.random() < 0.3:
                s = s[:-1]
            if rng.random() < 0.2:
                s = s[:rng.randrange(len(s) + 1)]
            out.append((rng.random() < 0.97, s))
        else:
            n = rng.randrange(0, 20)
            out.append((rng.random() < 0.97, b"".join(rng.choice(alphabet) for _ in range(n))))
    return out


class DifferentialParity(unittest.TestCase):
    def test_python_port_matches_real_cpp_decoder(self):
        cxx = shutil.which("g++") or shutil.which("clang++")
        if not cxx:
            self.skipTest("no C++ compiler")
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / "decode"
            subprocess.run([cxx, "-std=c++17", "-O1", "-I", str(ROOT / "src"),
                            str(ROOT / "tests/receipt_decode_cli.cpp"), "-o", str(exe)], check=True)
            cases = corpus()
            stdin = "".join(f"{1 if p else 0} {d.hex() if d else '-'}\n" for p, d in cases)
            result = subprocess.run([str(exe)], input=stdin, capture_output=True, text=True, check=True)
            native = [int(x) for x in result.stdout.split()]
        self.assertEqual(len(native), len(cases))
        python = [audit.decode_raw(d, p)[0] for p, d in cases]
        mismatches = [(cases[i], native[i], python[i]) for i in range(len(cases)) if native[i] != python[i]]
        self.assertEqual(mismatches[:5], [])
        # The corpus must exercise every status.
        self.assertEqual(set(native), set(range(6)))


SRC = audit.CANONICAL_SOURCE_HEX
EMPTY = audit.serialize([0, 0, 0, 1, 0, 0, 0, 0]).hex()
PAID = audit.serialize([1, 0, 1, 1, 400, 1800000000, 0, 0]).hex()
GF = audit.serialize([0, 2, 1, 1, 0, 0, 0, 0]).hex()


def tsv(rows):
    return [f"{g}\t{a}\t{p}\t{src}\t{h}\t{m}\n" for g, a, p, src, h, m in rows]


class Fixtures(unittest.TestCase):
    def test_categories_and_report(self):
        rows = audit.parse_tsv(tsv([
            (1, 10, 0, "", "", 66007),                    # absent
            (2, 10, 1, SRC, EMPTY, 66007),                # valid empty
            (3, 11, 1, SRC, PAID, 66008),                 # valid paid TBC
            (4, 11, 1, SRC, GF, 66013),                   # grandfathered Wrath
            (5, 12, 1, SRC, b"1 0 0 ".hex(), 66007),       # legacy 3-slot
            (6, 12, 1, SRC, "", 0),                       # present but empty/NULL
            (7, 12, 1, SRC, b"1 0 0 1 0 0 0 0 ".hex(), 66007),  # inconsistent
            (8, 13, 1, SRC, b"0 0 0 2 0 0 0 0 ".hex(), 0),  # unknown version
            (9, 13, 1, SRC, PAID, 66003),                 # paid but below stage 7
            (10, 13, 1, SRC, "NULL", 0),                  # mysql NULL text
            (11, 14, 1, SRC, b"0 0 0 1 0 0 0 0".hex(), 66007),  # non-canonical: malformed
        ]))
        r = audit.audit(rows)
        self.assertEqual(r["characters"], 11)
        self.assertEqual(r["categories"], {"valid": 4, "absent": 1, "malformed": 1, "wrong_slot_count": 3,
                                           "unknown_version": 1, "inconsistent": 1, "duplicate": 0,
                                           "source_variant": 0})
        self.assertEqual(r["commission_candidates"], [1])
        self.assertEqual(r["needs_staff_repair"], [5, 6, 7, 8, 10, 11])
        self.assertEqual(r["release_failures"], [])
        items = {i["guid"]: i for i in r["items"]}
        self.assertEqual((items[3]["tbc"], items[3]["wrath"], items[3]["stage"]), ("paid", "none", 8))
        self.assertEqual(items[4]["wrath"], "grandfathered")
        self.assertEqual(items[9]["note"], "receipt flags ignored below stage 7")
        self.assertEqual(r["valid_receipts"]["tbc_paid"], 2)

    def test_duplicates_never_collapse_or_commission(self):
        variant = b"IP_PAID_EXPANSION".hex().upper()
        rows = audit.parse_tsv(tsv([
            (20, 1, 1, SRC, EMPTY, 66007),
            (20, 1, 1, SRC, EMPTY, 66007),        # identical duplicate: must not collapse
            (21, 1, 1, SRC, PAID, 66008),
            (21, 1, 1, variant, EMPTY, 66008),    # canonical + collation variant
            (22, 1, 1, variant, PAID, 66008),     # variant only
            (23, 1, 0, "", "", 0),
        ]))
        r = audit.audit(rows)
        items = {i["guid"]: i for i in r["items"]}
        self.assertEqual(r["characters"], 4)
        self.assertEqual(items[20]["category"], "duplicate")
        self.assertEqual(items[21]["category"], "duplicate")
        self.assertEqual(items[22]["category"], "source_variant")
        self.assertEqual(r["commission_candidates"], [23])
        self.assertEqual(r["needs_staff_repair"], [20, 21, 22])
        self.assertNotIn("valid", {items[g]["category"] for g in (20, 21, 22)})
        self.assertEqual(len(r["release_failures"]), 3)

    def test_anomaly_and_schema_reports(self):
        anomalies = audit.parse_anomalies([
            "duplicate\t30\t2\t" + SRC + "|" + SRC + "\n",
            "orphan\t31\t1\t" + SRC + "\n",
            "source_variant\t32\t1\t" + b"ip_paid_expansion ".hex().upper() + "\n",
        ])
        good = audit.parse_schema(["primary_key\tcharacter_settings\tguid,source\n",
                                   "collation\tdata\tutf8mb4_unicode_ci\n",
                                   "collation\tsource\tutf8mb4_unicode_ci\n"])
        rows = audit.parse_tsv(tsv([(1, 1, 1, SRC, EMPTY, 66007)]))
        self.assertEqual(audit.audit(rows, [], good)["release_failures"], [])
        failures = audit.audit(rows, anomalies, good)["release_failures"]
        self.assertEqual([f.split(" ")[0] for f in failures], ["duplicate", "orphan", "source_variant"])
        for pk in ("", "guid", "source,guid", "guid,source,data"):
            bad = audit.parse_schema([f"primary_key\tcharacter_settings\t{pk}\n"])
            self.assertTrue(audit.audit(rows, [], bad)["release_failures"], pk)
        # Orphans are not characters: never commission candidates.
        self.assertEqual(audit.audit(rows, anomalies, good)["commission_candidates"], [])

    def test_bad_input_rejected(self):
        for line in ("1\t2\t1\t" + SRC + "\tzz\t0\n", "1\t2\t3\t\t\t0\n", "1\t2\t1\n",
                     "1\t2\t1\t" + SRC + "\tabc\t0\n", "1\t2\t0\t" + SRC + "\t\t0\n",
                     "1\t2\t1\t\t" + EMPTY + "\t0\n", "1\t2\t0\t" + SRC + "\t" + EMPTY + "\t66007\n"):
            with self.assertRaises(ValueError):
                audit.parse_tsv([line])
        for line in ("weird\t1\t1\tAA\n", "orphan\t1\t1\n"):
            with self.assertRaises(ValueError):
                audit.parse_anomalies([line])
        with self.assertRaises(ValueError):
            audit.parse_schema(["pk\tx\ty\n"])

    def run_cli(self, tmp, main, anomalies=None, schema=None, release=False, dispositions=None):
        args = [sys.executable, str(ROOT / "tools/ip_paid_receipt_audit.py"), str(main)]
        if dispositions is not None:
            args += ["--owner-dispositions", str(dispositions)]
        if anomalies is not None:
            args += ["--anomalies", str(anomalies)]
        if schema is not None:
            args += ["--schema", str(schema)]
        if release:
            args.append("--release")
        return subprocess.run(args, capture_output=True, text=True)

    def test_cli_release_gate(self):
        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            main = t / "r.tsv"
            main.write_text("".join(tsv([(1, 10, 0, "", "", 66007),
                                         (2, 10, 1, SRC, b"1 0 0 ".hex(), 66007)])), encoding="ascii")
            empty = t / "a.tsv"
            empty.write_text("", encoding="ascii")
            orphan = t / "o.tsv"
            orphan.write_text("orphan\t99\t1\t" + SRC + "\n", encoding="ascii")
            schema = t / "s.tsv"
            schema.write_text("primary_key\tcharacter_settings\tguid,source\n", encoding="ascii")
            badschema = t / "b.tsv"
            badschema.write_text("primary_key\tcharacter_settings\t\n", encoding="ascii")
            # Structurally clean, but one legacy row is unresolved: BLOCKED (3).
            blocked = self.run_cli(t, main, empty, schema, release=True)
            self.assertEqual(blocked.returncode, 3, blocked.stdout + blocked.stderr)
            self.assertIn("STRUCTURAL AUDIT: PASS", blocked.stdout)
            self.assertIn("COMMISSIONING READINESS: BLOCKED (1 unresolved refused row(s) need an owner disposition)", blocked.stdout)
            self.assertIn("refused rows needing an owner disposition: 1 (unresolved: 1)", blocked.stdout)
            self.assertIn("needs staff repair (never commissioned): 1", blocked.stdout)
            # Owner disposition recorded for exactly that row: READY (0).
            disp = t / "disp.tsv"
            disp.write_text("# owner decisions\n2\tkeep refused; staff repair after backup\n", encoding="utf-8")
            ok = self.run_cli(t, main, empty, schema, release=True, dispositions=disp)
            self.assertEqual(ok.returncode, 0, ok.stdout + ok.stderr)
            self.assertIn("STRUCTURAL AUDIT: PASS", ok.stdout)
            self.assertIn("COMMISSIONING READINESS: READY", ok.stdout)
            self.assertEqual(main.read_text(encoding="ascii").count(b"1 0 0 ".hex()), 1)  # never repaired
            # A disposition for a guid that is not a refused row: still BLOCKED.
            wrong = t / "wrong.tsv"
            wrong.write_text("2\tok\n1\tnot refused\n", encoding="utf-8")
            self.assertEqual(self.run_cli(t, main, empty, schema, release=True, dispositions=wrong).returncode, 3)
            for anomalies, sch in ((orphan, schema), (empty, badschema)):
                bad = self.run_cli(t, main, anomalies, sch, release=True, dispositions=disp)
                self.assertEqual(bad.returncode, 2)
                self.assertIn("STRUCTURAL AUDIT: FAIL", bad.stdout)
                self.assertIn("COMMISSIONING READINESS: BLOCKED (structural audit failed)", bad.stdout)
                self.assertNotIn("READINESS: READY", bad.stdout)
            missing = self.run_cli(t, main, None, schema, release=True)
            self.assertEqual(missing.returncode, 2)
            plain = self.run_cli(t, main)
            self.assertEqual(plain.returncode, 0)
            dup = t / "d.tsv"
            dup.write_text("".join(tsv([(5, 1, 1, SRC, EMPTY, 66007)] * 2)), encoding="ascii")
            self.assertEqual(self.run_cli(t, dup, empty, schema, release=True).returncode, 2)
            clean = t / "clean.tsv"
            clean.write_text("".join(tsv([(7, 1, 0, "", "", 66007), (8, 1, 1, SRC, EMPTY, 66007)])), encoding="ascii")
            r = self.run_cli(t, clean, empty, schema, release=True)
            self.assertEqual(r.returncode, 0)
            self.assertIn("COMMISSIONING READINESS: READY", r.stdout)

    def test_structural_pass_is_not_readiness(self):
        rows = audit.parse_tsv(tsv([(1, 1, 1, SRC, b"1 0 0 ".hex(), 66007), (2, 1, 0, "", "", 66007)]))
        report = audit.audit(rows, [], audit.parse_schema(["primary_key\tcharacter_settings\tguid,source\n"]))
        self.assertEqual(report["release_failures"], [])            # structurally clean
        ready = audit.readiness(report)
        self.assertFalse(ready["ready"])                              # but not ready
        self.assertEqual((ready["needs_owner_disposition"], ready["unresolved"]), (1, [1]))
        self.assertTrue(audit.readiness(report, {1: "owner: refuse until repaired"})["ready"])
        self.assertFalse(audit.readiness(report, {1: "x", 2: "y"})["ready"])   # 2 is absent, not refused
        for bad in (["1\t\n"], ["x\tok\n"], ["1\tok\n", "1\tagain\n"], ["1\n"]):
            with self.assertRaises(ValueError):
                audit.parse_dispositions(bad)


def statements(path):
    body = re.sub(r"--[^\n]*", "", (ROOT / "tools" / path).read_text(encoding="utf-8"))
    return [s.strip() for s in body.split(";") if s.strip()]


class ReadOnlyContract(unittest.TestCase):
    FORBIDDEN = ("INSERT", "UPDATE", "DELETE", "REPLACE", "DROP", "ALTER", "CREATE", "TRUNCATE", "GRANT",
                 "SET ", "CALL", "LOCK", "HANDLER", "LOAD", "INTO OUTFILE", "INTO DUMPFILE", "RENAME", "INTO @")

    def test_every_statement_is_a_select(self):
        expected = {"ip_paid_receipt_audit.sql": 1, "ip_paid_receipt_anomalies.sql": 3, "ip_paid_receipt_schema.sql": 2}
        for path, count in expected.items():
            stmts = statements(path)
            self.assertEqual(len(stmts), count, path)
            for stmt in stmts:
                self.assertTrue(stmt.upper().startswith("SELECT"), (path, stmt[:40]))
                for verb in self.FORBIDDEN:
                    self.assertNotIn(verb, stmt.upper(), (path, verb))

    def test_main_query_never_collapses_rows(self):
        main = statements("ip_paid_receipt_audit.sql")[0]
        self.assertNotIn("GROUP BY", main.upper())
        self.assertNotIn("DISTINCT", main.upper())
        self.assertIn("HEX(s.data)", main)
        self.assertIn("HEX(s.source)", main)
        self.assertIn("s.source = 'ip_paid_expansion'", main)
        self.assertIn("q.quest BETWEEN 66001 AND 66018", main)

    def test_anomaly_queries(self):
        dup, orphan, variant = statements("ip_paid_receipt_anomalies.sql")
        self.assertTrue(dup.startswith("SELECT 'duplicate'"))
        self.assertIn("GROUP BY s.guid\nHAVING COUNT(*) > 1", dup)   # integer key only
        self.assertTrue(orphan.startswith("SELECT 'orphan'"))
        self.assertIn("LEFT JOIN characters c ON c.guid = s.guid", orphan)
        self.assertIn("c.guid IS NULL", orphan)
        self.assertTrue(variant.startswith("SELECT 'source_variant'"))
        self.assertIn("BINARY s.source <> BINARY 'ip_paid_expansion'", variant)
        schema = statements("ip_paid_receipt_schema.sql")
        self.assertIn("information_schema.KEY_COLUMN_USAGE", schema[0])
        self.assertIn("k.CONSTRAINT_NAME = 'PRIMARY'", schema[0])
        self.assertIn("information_schema.COLUMNS", schema[1])
        self.assertEqual(audit.EXPECTED_PRIMARY_KEY, "guid,source")

    def test_tool_never_connects(self):
        tool = (ROOT / "tools/ip_paid_receipt_audit.py").read_text(encoding="utf-8")
        for token in ("mysql", "MySQLdb", "pymysql", "socket", "subprocess", "connect(", "urllib", "http"):
            self.assertNotIn(token, tool.replace("mysql -B", ""), token)

    def test_port_constants_match_header(self):
        header = (ROOT / "src/IndividualProgressionReceiptPolicy.h").read_text(encoding="utf-8")
        self.assertIn("Valid, Missing, Malformed, WrongSlotCount, UnknownVersion, Inconsistent", header)
        self.assertIn("constexpr std::size_t Slots = 8;", header)
        self.assertEqual(audit.CANONICAL_SOURCE_HEX, "69705F706169645F657870616E73696F6E")


if __name__ == "__main__":
    unittest.main()
