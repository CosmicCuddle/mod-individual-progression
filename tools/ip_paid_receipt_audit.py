#!/usr/bin/env python3
"""Offline, read-only audit of Individual Progression paid expansion receipts.

Input: the TSV produced by ip_paid_receipt_audit.sql (run against a backup or
replica). No database connection is made and nothing is written except the
optional --json report. The classifier is an exact port of
IndividualProgressionReceiptPolicy::DecodeRaw (src/IndividualProgressionReceiptPolicy.h);
tests/test_ip_paid_receipt_audit_tool.py checks it against the real C++.

Categories (what the server would do at login with PaidExpansion ON):
  absent        no row: cannot buy unless CommissionMissingReceipt=1 commissions it
  valid         canonical v1 receipt (empty, paid and/or grandfathered)
  malformed / wrong_slot_count / unknown_version / inconsistent
                refused (fail closed); never commissioned or overwritten:
                needs staff repair before that character can buy
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


def classify_row(guid, account, row_present, data_hex, max_marker):
    data = bytes.fromhex(data_hex) if data_hex else b""
    status, fields = decode_raw(data, row_present == 1)
    stage = max_marker - 66000 if 66001 <= max_marker <= 66018 else 0
    item = {"guid": guid, "account": account, "category": STATUS_NAMES[status], "stage": stage}
    if fields is not None:
        item["tbc"] = ["none", "paid", "grandfathered"][fields[TBC_FLAG]]
        item["wrath"] = ["none", "paid", "grandfathered"][fields[WRATH_FLAG]]
        item["grandfather_evaluated"] = bool(fields[GF_EVALUATED])
        if stage < 7 and (fields[TBC_FLAG] or fields[WRATH_FLAG]):
            # LoadPaidExpansionEntitlement ignores purchase flags below stage 7.
            item["note"] = "receipt flags ignored below stage 7"
    return item


def parse_tsv(lines):
    rows = []
    for number, line in enumerate(lines, 1):
        line = line.rstrip("\n").rstrip("\r")
        if not line:
            continue
        parts = line.split("\t")
        if len(parts) != 5:
            raise ValueError(f"line {number}: expected 5 tab-separated columns, got {len(parts)}")
        guid, account, present, data_hex, marker = parts
        if present not in ("0", "1"):
            raise ValueError(f"line {number}: row_present must be 0 or 1")
        if data_hex.upper() == "NULL":
            data_hex = ""
        if len(data_hex) % 2 or any(c not in "0123456789abcdefABCDEF" for c in data_hex):
            raise ValueError(f"line {number}: data_hex is not hexadecimal")
        rows.append((int(guid), int(account), int(present), data_hex, int(marker)))
    return rows


def audit(rows):
    items = [classify_row(*r) for r in rows]
    counts = Counter(i["category"] for i in items)
    paid = Counter()
    for i in items:
        if i["category"] == "valid":
            paid["tbc_" + i["tbc"]] += 1
            paid["wrath_" + i["wrath"]] += 1
    return {
        "characters": len(items),
        "categories": {name: counts.get(name, 0) for name in STATUS_NAMES},
        "valid_receipts": dict(sorted(paid.items())),
        "needs_staff_repair": [i["guid"] for i in items
                               if i["category"] not in ("valid", "absent")],
        "commission_candidates": [i["guid"] for i in items if i["category"] == "absent"],
        "items": items,
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description="Offline read-only IP paid receipt audit")
    ap.add_argument("tsv", help="output of ip_paid_receipt_audit.sql (mysql -B --skip-column-names)")
    ap.add_argument("--json", help="also write the full report to this file")
    args = ap.parse_args(argv)
    with open(args.tsv, encoding="ascii") as handle:
        report = audit(parse_tsv(handle))
    print(f"characters: {report['characters']}")
    for name, count in report["categories"].items():
        print(f"  {name:17s} {count}")
    for name, count in report["valid_receipts"].items():
        print(f"  valid {name:15s} {count}")
    print(f"needs staff repair (never commissioned): {len(report['needs_staff_repair'])}")
    for guid in report["needs_staff_repair"]:
        print(f"  guid {guid}")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as handle:
            json.dump(report, handle, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
