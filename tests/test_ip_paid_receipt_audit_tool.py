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


class Fixtures(unittest.TestCase):
    def tsv(self, rows):
        return [f"{g}\t{a}\t{p}\t{h}\t{m}\n" for g, a, p, h, m in rows]

    def test_categories_and_report(self):
        empty = audit.serialize([0, 0, 0, 1, 0, 0, 0, 0]).hex()
        paid = audit.serialize([1, 0, 1, 1, 400, 1800000000, 0, 0]).hex()
        gf = audit.serialize([0, 2, 1, 1, 0, 0, 0, 0]).hex()
        rows = audit.parse_tsv(self.tsv([
            (1, 10, 0, "", 66007),                    # absent
            (2, 10, 1, empty, 66007),                 # valid empty
            (3, 11, 1, paid, 66008),                  # valid paid TBC
            (4, 11, 1, gf, 66013),                    # grandfathered Wrath
            (5, 12, 1, b"1 0 0 ".hex(), 66007),        # legacy 3-slot
            (6, 12, 1, "", 0),                        # present but empty/NULL
            (7, 12, 1, b"1 0 0 1 0 0 0 0 ".hex(), 7 + 66000),  # inconsistent
            (8, 13, 1, b"0 0 0 2 0 0 0 0 ".hex(), 0),  # unknown version
            (9, 13, 1, paid, 66003),                  # paid but below stage 7
            (10, 13, 1, "NULL", 0),                   # mysql NULL text
            (11, 14, 1, b"0 0 0 1 0 0 0 0".hex(), 66007),  # non-canonical: malformed
        ]))
        r = audit.audit(rows)
        self.assertEqual(r["characters"], 11)
        self.assertEqual(r["categories"], {"valid": 4, "absent": 1, "malformed": 1, "wrong_slot_count": 3,
                                           "unknown_version": 1, "inconsistent": 1})
        self.assertEqual(r["commission_candidates"], [1])
        self.assertEqual(r["needs_staff_repair"], [5, 6, 7, 8, 10, 11])
        items = {i["guid"]: i for i in r["items"]}
        self.assertEqual((items[3]["tbc"], items[3]["wrath"], items[3]["stage"]), ("paid", "none", 8))
        self.assertEqual(items[4]["wrath"], "grandfathered")
        self.assertEqual(items[9]["note"], "receipt flags ignored below stage 7")
        self.assertEqual(r["valid_receipts"]["tbc_paid"], 2)

    def test_bad_input_rejected(self):
        for line in ("1\t2\t1\tzz\t0\n", "1\t2\t3\t\t0\n", "1\t2\t1\n", "1\t2\t1\tabc\t0\n"):
            with self.assertRaises(ValueError):
                audit.parse_tsv([line])

    def test_cli_runs_offline(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / "r.tsv"
            src.write_text("1\t10\t0\t\t66007\n2\t10\t1\t" + b"1 0 0 ".hex() + "\t66007\n", encoding="ascii")
            out = subprocess.run([sys.executable, str(ROOT / "tools/ip_paid_receipt_audit.py"), str(src),
                                  "--json", str(Path(tmp) / "r.json")], capture_output=True, text=True, check=True)
            self.assertIn("absent", out.stdout)
            self.assertIn("needs staff repair (never commissioned): 1", out.stdout)
            self.assertTrue((Path(tmp) / "r.json").exists())


class ReadOnlyContract(unittest.TestCase):
    def test_sql_is_one_select(self):
        body = re.sub(r"--[^\n]*", "", SQL)
        statements = [s.strip() for s in body.split(";") if s.strip()]
        self.assertEqual(len(statements), 1)
        self.assertTrue(statements[0].upper().startswith("SELECT"))
        for verb in ("INSERT", "UPDATE", "DELETE", "REPLACE", "DROP", "ALTER", "CREATE", "TRUNCATE",
                     "GRANT", "SET ", "CALL", "LOCK", "HANDLER", "LOAD", "INTO OUTFILE", "INTO DUMPFILE"):
            self.assertNotIn(verb, body.upper(), verb)
        self.assertIn("HEX(s.data)", SQL)
        self.assertIn("s.source = 'ip_paid_expansion'", SQL)
        self.assertIn("q.quest BETWEEN 66001 AND 66018", SQL)

    def test_tool_never_connects(self):
        tool = (ROOT / "tools/ip_paid_receipt_audit.py").read_text(encoding="utf-8")
        for token in ("mysql", "MySQLdb", "pymysql", "socket", "subprocess", "connect("):
            self.assertNotIn(token, tool.replace("mysql -B", ""), token)

    def test_port_constants_match_header(self):
        header = (ROOT / "src/IndividualProgressionReceiptPolicy.h").read_text(encoding="utf-8")
        self.assertIn("Valid, Missing, Malformed, WrongSlotCount, UnknownVersion, Inconsistent", header)
        self.assertIn("constexpr std::size_t Slots = 8;", header)


if __name__ == "__main__":
    unittest.main()
