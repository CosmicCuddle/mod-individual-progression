"""Regression for the real IP134 nested lease declaration, not a Worldserver build.

Compile the exact nested struct / method declarations extracted from the owned
header, including the legacy two-argument cleanup call and three-argument
verified RandomBot call. Also require the immutable parent to fail compilation.
No runtime player objects, feature activation, DB or host operations.
"""

import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "IndividualProgression.h"
PARENT = "3a0d5e6fea676ac22601f82648de72ce20fa5e1b"


def declaration_region(header_text):
    start = header_text.index("    struct F14ActorPublicationLoginLease")
    end = header_text.index("    void ForgetF14VerifiedRandomBotGroupIdentity(", start)
    region = header_text[start:end]
    assert "F14ActorPublicationLoginLease sourceLease" in region
    assert "RememberF14VerifiedRandomBotGroupIdentity" in region
    return region


def compile_source(header_text, compiler, standard, opt_in):
    # The extraction includes the actual IP declaration, not a retyped
    # substitute. Minimal mock Player suffices for syntax/overload testing.
    code = (
        "#include <cstdint>\n"
        "struct Player {};\n"
        "class IndividualProgression { public:\n"
        + declaration_region(header_text)
        + "\n};\n"
        "void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity("
        "Player*, bool, F14ActorPublicationLoginLease) {}\n"
        "int main() { Player player; IndividualProgression ip;"
        " ip.RememberF14VerifiedRandomBotGroupIdentity(&player, false);"
        " IndividualProgression::F14ActorPublicationLoginLease lease{};"
        " ip.RememberF14VerifiedRandomBotGroupIdentity(&player, true, lease);"
        " return 0; }\n"
    )
    return subprocess.run(
        [compiler, "-x", "c++", "-std=" + standard,
         "-Wall", "-Wextra", "-Werror", "-fsyntax-only",
         "-DCOSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT=" + str(opt_in),
         "-"],
        input=code, text=True, capture_output=True, check=False,
    )


class NestedLeaseHeaderRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.current = HEADER.read_text(encoding="utf-8")
        original = subprocess.run(
            ["git", "show", PARENT + ":src/IndividualProgression.h"],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        if original.returncode:
            raise AssertionError(
                "Cannot obtain immutable parent header; CI must checkout full ancestry: "
                + original.stderr
            )
        cls.original = original.stdout

    def test_original_bug_is_exactly_present_only_in_parent(self):
        old = declaration_region(self.original)
        new = declaration_region(self.current)
        self.assertIn("F14ActorPublicationLoginLease sourceLease = {}", old)
        self.assertNotIn("F14ActorPublicationLoginLease sourceLease = {}", new)
        self.assertIn("F14ActorPublicationLoginLease sourceLease);", new)
        self.assertIn(
            "RememberF14VerifiedRandomBotGroupIdentity(Player* player, bool verified)", new
        )
        self.assertIn("player, verified, F14ActorPublicationLoginLease{}", new)

    def test_old_red_new_green_real_header_declarations(self):
        for compiler in ("g++", "clang++"):
            if not shutil.which(compiler):
                self.fail(compiler + " is required for this gate")
            for standard in ("c++17", "c++20"):
                for opt_in in (0, 1):
                    with self.subTest(compiler=compiler, std=standard, opt_in=opt_in):
                        old = compile_source(self.original, compiler, standard, opt_in)
                        self.assertNotEqual(
                            old.returncode, 0,
                            "Parent unexpectedly compiled; old-RED proof lost",
                        )
                        self.assertRegex(
                            old.stderr,
                            r"(could not convert|default member initializer)",
                        )
                        new = compile_source(self.current, compiler, standard, opt_in)
                        self.assertEqual(
                            new.returncode, 0,
                            "New header syntax/legacy overload failed:\n" + new.stderr,
                        )

    def test_login_keeps_real_bot_proof_and_entry_lease(self):
        login = (ROOT / "src" / "IndividualProgressionPlayer.cpp").read_text(
            encoding="utf-8"
        )
        identity = login.index("sRandomPlayerbotMgr.IsRandomBot(")
        capture = login.index("CaptureF14ActorPublicationLoginLease()")
        self.assertLess(capture, identity)
        self.assertRegex(
            login[identity - 200:identity + 300],
            r"IsRandomBot\(player->GetGUID\(\)\.GetCounter\(\)\),\s*f14SourceLease",
        )
        implementation = (ROOT / "src" / "IndividualProgression.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "RememberF14VerifiedRandomBotGroupIdentity(player, false)",
            implementation,
        )


if __name__ == "__main__":
    unittest.main()
