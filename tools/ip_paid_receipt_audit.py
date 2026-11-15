#!/usr/bin/env python3
"""Offline, read-only audit of Individual Progression paid expansion receipts.

Input: the TSVs produced by ip_paid_receipt_audit.sql (main),
ip_paid_receipt_anomalies.sql and ip_paid_receipt_schema.sql (run against the
same backup or replica). No database connection is made and nothing is written except the
optional --json report. The classifier is an exact port of
IndividualProgressionReceiptPolicy::DecodeRaw (src/IndividualProgressionReceiptPolicy.h);
tests/test_ip_paid_receipt_audit_tool.py checks it against the real C++.

Categories (what the server would do at login with PaidExpansion ON):
  absent        no row: cannot buy unless CommissionMissingReceipt=1 commissions it
  valid         canonical v1 receipt (empty, paid and/or grandfathered)
  malformed / wrong_slot_count / unknown_version / inconsistent
                refused (fail closed); never commissioned or overwritten:
                needs staff repair before that character can buy
  duplicate     more than one matching row for the character (never commissioned)
  source_variant  the row matched only through the collation (case/space)

--release prints TWO separate verdicts:
  STRUCTURAL AUDIT        PASS/FAIL: no duplicate, orphan or source-variant row
                          and a primary key of exactly (guid, source). FAIL -> exit 2.
  COMMISSIONING READINESS READY/BLOCKED: every row the server refuses
                          (malformed, wrong slot count, unknown version,
                          inconsistent) needs an explicit OWNER DISPOSITION,
                          recorded in --owner-dispositions (guid<TAB>text).
                          BLOCKED -> exit 3. The tool never repairs anything.
A structural PASS alone is NOT permission to enable commissioning.
Raw reports stay local; nothing is uploaded.
"""
import argparse
import json
import sys
from collections import Counter

SLOTS = 8
VERSION1 = 1
FLAG_NONE, FLAG_PAID, FLAG_GRANDFATHERED = 0, 1, 2
MAX_CHARGED = 2**31 - 1
U32_MAX = 2**32 - 1
TBC_FLAG, WRATH_FLAG, GF_EVALUATED, VERSION, TBC_COPPER, TBC_EPOCH, WRATH_COPPER, WRATH_EPOCH = range(8)

VALID, MISSING, MALFORMED, WRONG_SLOT_COUNT, UNKNOWN_VERSION, INCONSISTENT = range(6)
STATUS_NAMES = ["valid", "absent", "malformed", "wrong_slot_count", "unknown_version", "inconsistent"]
CATEGORIES = STATUS_NAMES + ["duplicate", "source_variant"]
CANONICAL_SOURCE_HEX = b"ip_paid_expansion".hex().upper()
EXPECTED_PRIMARY_KEY = "guid,source"


def _decimal_u32(token):
    if not token or any(b < 0x30 or b > 0x39 for b in token):
        return None
    value = int(token)
    return value if value <= U32_MAX else None


def _pair_valid(flag, copper, epoch):
    if flag in (FLAG_NONE, FLAG_GRANDFATHERED):
        return copper == 0 and epoch == 0
    return flag == FLAG_PAID and 0 < copper <= MAX_CHARGED and epoch > 0


def valid_ledger(f):
    return (f[TBC_FLAG] in (FLAG_NONE, FLAG_PAID, FLAG_GRANDFATHERED) and
            f[WRATH_FLAG] in (FLAG_NONE, FLAG_PAID, FLAG_GRANDFATHERED) and
            f[GF_EVALUATED] <= 1 and f[VERSION] == VERSION1 and
            _pair_valid(f[TBC_FLAG], f[TBC_COPPER], f[TBC_EPOCH]) and
            _pair_valid(f[WRATH_FLAG], f[WRATH_COPPER], f[WRATH_EPOCH]))


def serialize(fields):
    return b"".join(str(v).encode() + b" " for v in fields)


def decode_raw(data, row_present):
    """Exact port of DecodeRaw. data: bytes. Returns (status, fields|None)."""
    original = data
    if not row_present:
        return MISSING, None
    if not data:
        return WRONG_SLOT_COUNT, None
    if data.endswith(b" "):
        data = data[:-1]
    if not data:
        return MALFORMED, None
    fields = [0] * SLOTS
    i = 0
    while data:
        if i == SLOTS:
            return WRONG_SLOT_COUNT, None
        sep = data.find(b" ")
        token = data if sep < 0 else data[:sep]
        value = _decimal_u32(token)
        if value is None:
            return MALFORMED, None
        fields[i] = value
        i += 1
        if sep < 0:
            break
        data = data[sep + 1:]
        if not data:
            return MALFORMED, None
    if i != SLOTS:
        return WRONG_SLOT_COUNT, None
    if fields[VERSION] != VERSION1:
        return UNKNOWN_VERSION, None
    if not valid_ledger(fields):
        return INCONSISTENT, None
    if serialize(fields) != original:
        return MALFORMED, None
    return VALID, fields


def classify_row(guid, account, row_present, source_hex, data_hex, max_marker):
    data = bytes.fromhex(data_hex) if data_hex else b""
    status, fields = decode_raw(data, row_present == 1)
    stage = max_marker - 66000 if 66001 <= max_marker <= 66018 else 0
    item = {"guid": guid, "account": account, "category": STATUS_NAMES[status], "stage": stage}
    if row_present == 1 and source_hex.upper() != CANONICAL_SOURCE_HEX:
        item["category"] = "source_variant"
        item["source_hex"] = source_hex.upper()
        return item
    if fields is not None:
        item["tbc"] = ["none", "paid", "grandfathered"][fields[TBC_FLAG]]
        item["wrath"] = ["none", "paid", "grandfathered"][fields[WRATH_FLAG]]
        item["grandfather_evaluated"] = bool(fields[GF_EVALUATED])
        if stage < 7 and (fields[TBC_FLAG] or fields[WRATH_FLAG]):
            # LoadPaidExpansionEntitlement ignores purchase flags below stage 7.
            item["note"] = "receipt flags ignored below stage 7"
    return item


def _hex(value, number, name):
    if value.upper() == "NULL":
        value = ""
    if len(value) % 2 or any(c not in "0123456789abcdefABCDEF" for c in value):
        raise ValueError(f"line {number}: {name} is not hexadecimal")
    return value


def parse_tsv(lines):
    rows = []
    for number, line in enumerate(lines, 1):
        line = line.rstrip("\n").rstrip("\r")
        if not line:
            continue
        parts = line.split("\t")
        if len(parts) != 6:
            raise ValueError(f"line {number}: expected 6 tab-separated columns, got {len(parts)}")
        guid, account, present, source_hex, data_hex, marker = parts
        if present not in ("0", "1"):
            raise ValueError(f"line {number}: row_present must be 0 or 1")
        source_hex = _hex(source_hex, number, "source_hex")
        data_hex = _hex(data_hex, number, "data_hex")
        if present == "0" and (source_hex or data_hex):
            raise ValueError(f"line {number}: row_present 0 with row data")
        if present == "1" and not source_hex:
            raise ValueError(f"line {number}: row_present 1 without a source")
        rows.append((int(guid), int(account), int(present), source_hex, data_hex, int(marker)))
    return rows


def parse_anomalies(lines):
    found = []
    for number, line in enumerate(lines, 1):
        line = line.rstrip("\n").rstrip("\r")
        if not line:
            continue
        parts = line.split("\t")
        if len(parts) != 4 or parts[0] not in ("duplicate", "orphan", "source_variant"):
            raise ValueError(f"anomalies line {number}: unexpected row")
        found.append({"kind": parts[0], "guid": int(parts[1]), "rows": int(parts[2]), "source_hex": parts[3]})
    return found


def parse_schema(lines):
    schema = {"primary_key": None, "collation": {}}
    for number, line in enumerate(lines, 1):
        line = line.rstrip("\n").rstrip("\r")
        if not line:
            continue
        parts = line.split("\t")
        if len(parts) != 3 or parts[0] not in ("primary_key", "collation"):
            raise ValueError(f"schema line {number}: unexpected row")
        if parts[0] == "primary_key":
            schema["primary_key"] = parts[2]
        else:
            schema["collation"][parts[1]] = parts[2]
    return schema


def parse_dispositions(lines):
    found = {}
    for number, line in enumerate(lines, 1):
        line = line.rstrip("\n").rstrip("\r")
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t", 1)
        if len(parts) != 2 or not parts[0].isdigit() or not parts[1].strip():
            raise ValueError(f"dispositions line {number}: expected guid<TAB>non-empty owner disposition")
        guid = int(parts[0])
        if guid in found:
            raise ValueError(f"dispositions line {number}: guid {guid} listed twice")
        found[guid] = parts[1].strip()
    return found


def readiness(report, dispositions=None):
    """Commissioning readiness, separate from the structural verdict."""
    dispositions = dispositions or {}
    unresolved = [g for g in report["needs_staff_repair"] if g not in dispositions]
    unknown = sorted(g for g in dispositions if g not in report["needs_staff_repair"])
    return {
        "needs_owner_disposition": len(report["needs_staff_repair"]),
        "unresolved": unresolved,
        "dispositions_for_other_guids": unknown,
        "ready": not unresolved and not unknown and not report["release_failures"],
    }


def audit(rows, anomalies=None, schema=None):
    items = [classify_row(*r) for r in rows]
    # A character with more than one main row (duplicate or collation
    # variants that the backup still holds) is never valid or commissionable.
    per_guid = Counter(i["guid"] for i in items)
    for i in items:
        if per_guid[i["guid"]] > 1:
            i["category"] = "duplicate"
    seen = set()
    unique = []
    for i in items:
        if i["guid"] not in seen:
            seen.add(i["guid"])
            unique.append(i)
    items = unique
    counts = Counter(i["category"] for i in items)
    paid = Counter()
    for i in items:
        if i["category"] == "valid":
            paid["tbc_" + i["tbc"]] += 1
            paid["wrath_" + i["wrath"]] += 1
    failures = []
    if anomalies:
        for a in anomalies:
            failures.append(f"{a['kind']} receipt row(s) for guid {a['guid']} ({a['rows']})")
    for guid, count in sorted(per_guid.items()):
        if count > 1:
            failures.append(f"duplicate main rows for guid {guid} ({count})")
    for i in items:
        if i["category"] == "source_variant":
            failures.append(f"source_variant for guid {i['guid']}")
    if schema is not None and schema.get("primary_key") != EXPECTED_PRIMARY_KEY:
        failures.append(f"character_settings primary key is {schema.get('primary_key')!r}, expected {EXPECTED_PRIMARY_KEY!r}")
    return {
        "characters": len(items),
        "categories": {name: counts.get(name, 0) for name in CATEGORIES},
        "valid_receipts": dict(sorted(paid.items())),
        "needs_staff_repair": [i["guid"] for i in items
                               if i["category"] not in ("valid", "absent")],
        "commission_candidates": [i["guid"] for i in items if i["category"] == "absent"],
        "anomalies": anomalies or [],
        "schema": schema,
        "release_failures": failures,
        "items": items,
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description="Offline read-only IP paid receipt audit")
    ap.add_argument("tsv", help="output of ip_paid_receipt_audit.sql (mysql -B --skip-column-names)")
    ap.add_argument("--anomalies", help="output of ip_paid_receipt_anomalies.sql")
    ap.add_argument("--schema", help="output of ip_paid_receipt_schema.sql")
    ap.add_argument("--release", action="store_true",
                    help="release audit: require --anomalies and --schema; exit 2 on any anomaly")
    ap.add_argument("--owner-dispositions",
                    help="guid<TAB>owner disposition for each refused row (recorded only; nothing is repaired)")
    ap.add_argument("--json", help="also write the full report to this local file")
    args = ap.parse_args(argv)
    if args.release and (not args.anomalies or not args.schema):
        print("RELEASE AUDIT: FAIL (--release requires --anomalies and --schema)")
        return 2
    with open(args.tsv, encoding="ascii") as handle:
        rows = parse_tsv(handle)
    anomalies = schema = None
    if args.anomalies:
        with open(args.anomalies, encoding="ascii") as handle:
            anomalies = parse_anomalies(handle)
    if args.schema:
        with open(args.schema, encoding="ascii") as handle:
            schema = parse_schema(handle)
    report = audit(rows, anomalies, schema)
    dispositions = None
    if args.owner_dispositions:
        with open(args.owner_dispositions, encoding="utf-8") as handle:
            dispositions = parse_dispositions(handle)
    report["readiness"] = readiness(report, dispositions)
    print(f"characters: {report['characters']}")
    for name, count in report["categories"].items():
        print(f"  {name:17s} {count}")
    for name, count in report["valid_receipts"].items():
        print(f"  valid {name:15s} {count}")
    print(f"needs staff repair (never commissioned): {len(report['needs_staff_repair'])}")
    for guid in report["needs_staff_repair"]:
        print(f"  guid {guid}")
    if schema is not None:
        print(f"character_settings primary key: {schema.get('primary_key')}; collation: {schema.get('collation')}")
    for failure in report["release_failures"]:
        print(f"ANOMALY: {failure}")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as handle:
            json.dump(report, handle, indent=2)
    ready = report["readiness"]
    print(f"refused rows needing an owner disposition: {ready['needs_owner_disposition']} "
          f"(unresolved: {len(ready['unresolved'])})")
    for guid in ready["dispositions_for_other_guids"]:
        print(f"DISPOSITION ERROR: guid {guid} is not a refused row")
    if args.release:
        structural_ok = not report["release_failures"]
        print("STRUCTURAL AUDIT: " + ("PASS" if structural_ok else "FAIL"))
        print("COMMISSIONING READINESS: " + ("READY" if ready["ready"] else
              f"BLOCKED ({len(ready['unresolved'])} unresolved refused row(s) need an owner disposition)"
              if structural_ok and not ready["dispositions_for_other_guids"] else
              "BLOCKED" if structural_ok else "BLOCKED (structural audit failed)"))
        if not structural_ok:
            return 2
        return 0 if ready["ready"] else 3
    return 0


if __name__ == "__main__":
    sys.exit(main())
