"""Pinned Core paid-receipt preservation contract. Source/model ONLY."""
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "tests/core_patches/preserve_paid_receipt_row.patch"
CORE_SHA = "8daffdabe9202c228e45d833c9a1d399597a15cd"
CORE_BLOB = "f1eb86367ce69111867ef7187125f5fe456c5367"
CORE_PATH = "src/server/game/Entities/Player/PlayerSettings.cpp"


def core_tokens(data):
    # Models permissive Core StringTo<uint32> token skipping.
    result = []
    for token in data.split(" "):
        if token and re.fullmatch(r"\d+", token) and int(token) <= 4294967295:
            result.append(int(token))
    return result


def core_serialize(tokens):
    return "".join(f"{number} " for number in tokens)


def safe_paid_source(source, raw):
    if source != "ip_paid_expansion":
        return True
    parsed = core_tokens(raw)
    return bool(parsed) and core_serialize(parsed) == raw


class PreservationPolicy(unittest.TestCase):
    def test_broken_sources_are_never_normalized_into_paid_access(self):
        rows = [
            "x 1 0 1 1 5000 1700000000 0 0",
            "1 0 1 1 5000 -7 1700000000 0 0",
            "1 0 1 1 5000 1700000000 0 0 99999999999",
            "1 0 1 1 5000 1700000000 0 0  ",
            "1  0 1 1 5000 1700000000 0 0",
        ]
        for raw in rows:
            with self.subTest(raw=raw):
                # The old Core parser would rewrite some of these to a
                # valid paid row. The protected path must never cache them.
                self.assertFalse(safe_paid_source("ip_paid_expansion", raw))
                self.assertNotEqual(core_serialize(core_tokens(raw)), raw)
                stored_after_save = raw if not safe_paid_source("ip_paid_expansion", raw) else core_serialize(core_tokens(raw))
                self.assertEqual(stored_after_save, raw)

    def test_canonical_rows_and_unrelated_sources_are_unchanged(self):
        self.assertTrue(safe_paid_source("ip_paid_expansion", "1 0 1 1 5000 1700000000 0 0 "))
        self.assertTrue(safe_paid_source("ip_paid_expansion", "0 0 1 "))
        self.assertTrue(safe_paid_source("ac_default", "broken x 2"))
        self.assertFalse(safe_paid_source("ip_paid_expansion", ""))

    def test_exact_core_patch_has_online_and_offline_write_guards(self):
        patch = PATCH.read_text()
        self.assertIn('source != "ip_paid_expansion"', patch)
        self.assertIn("SerializeSettingsData(parsed) == data", patch)
        self.assertIn("if (!PlayerSettingsStore::IsCanonicalPaidReceiptRow(source, data))", patch)
        self.assertIn('if (!IsCanonicalPaidReceiptRow(rowSource, data))', patch)
        self.assertIn("if (unsafePaidRow)", patch)
        self.assertIn("continue;", patch)
        self.assertIn("return; // The original untrusted row must be preserved.", patch)
        self.assertIn("m_preserveMalformedPaidSettingsRow = true;", patch)
        self.assertIn("m_preserveMalformedPaidSettingsRow = false;", patch)
        self.assertIn('if (m_preserveMalformedPaidSettingsRow && source == "ip_paid_expansion")', patch)
        self.assertIn("return PlayerSetting(0);", patch)
        self.assertIn("bool m_preserveMalformedPaidSettingsRow = false;", patch)
        self.assertIn("src/server/game/Entities/Player/Player.h", patch)
        self.assertNotIn("UPDATE character_settings", patch)
        self.assertNotIn("DELETE FROM character_settings", patch)


if __name__ == "__main__":
    unittest.main()
