#!/usr/bin/env python3
"""Exact old/current policy C++ execution: unknown cap must never grant TBC.

Old IP136 must compile and execute RED at invalid-cap cases. New child must
compile and execute GREEN. No Playerbots/Worldserver/client/DB or stage writes.
The 0=uncapped case remains a deliberate NON-authoritative calculation.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
POLICY = ROOT / "src" / "IndividualProgressionF14LoginPolicy.h"
PARENT = "29fa9a179039ebf292ee137005cbdd112407581a"

CXX = r"""
#include "IndividualProgressionF14LoginPolicy.h"
#include <iostream>
namespace P = IndividualProgressionF14LoginPolicy;

int main()
{
    int failed = 0;
    auto expect = [&](bool actual, bool wanted, char const* what) {
        if (actual != wanted)
        {
            ++failed;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    // These three compile and EXECUTE as failures in pristine IP136.
    expect(P::CanSetVerifiedTbcRaceStage(0, 60, -1), false,
           "negative unknown cap must not count as uncapped");
    expect(P::CanSetVerifiedTbcRaceStage(0, 60, -2), false,
           "other negative cap must be rejected");
    expect(P::CanSetVerifiedTbcRaceStage(0, 60, 19), false,
           "cap above final supported stage18 must be rejected");

    // Preserve existing valid choices. Cap=0 is a PURE calculation, NOT
    // trusted first-birth, world TBC permission or paid entitlement.
    expect(P::CanSetVerifiedTbcRaceStage(0, 60, 0), true,
           "cap0 is only documented no-IP-stage-cap semantics");
    expect(P::CanSetVerifiedTbcRaceStage(0, 60, 7), false,
           "positive Vanilla cap cannot grant stage8");
    expect(P::CanSetVerifiedTbcRaceStage(0, 60, 8), true,
           "numeric cap8 permits pure stage8 arithmetic only");
    expect(P::CanSetVerifiedTbcRaceStage(0, 60, 18), true,
           "numeric cap18 permits pure stage8 arithmetic only");
    expect(P::CanSetVerifiedTbcRaceStage(19, 60, 18), false,
           "invalid saved stage never grants");
    expect(P::CanSetVerifiedTbcRaceStage(8, 71, 12), false,
           "71+ level requires 13, limit12 must refuse");
    expect(P::CanSetVerifiedTbcRaceStage(8, 71, 13), true,
           "71+ level13 within numeric cap13");
    expect(P::DesiredVerifiedTbcRaceStage(0, 60) == 8, true,
           "starting arithmetic still targets 8");
    expect(P::DesiredVerifiedTbcRaceStage(13, 60) == 13, true,
           "earned stage13 never numerically downgraded");
    if (!failed)
        std::cout << "PASS invalid-cap denial, 12 source-policy cases\n";
    return failed ? 1 : 0;
}
"""


def compile_run(header: str, compiler: str, std: str, ndebug: bool):
    with tempfile.TemporaryDirectory() as tmp:
        dirpath = Path(tmp)
        (dirpath / POLICY.name).write_text(header, encoding="utf-8")
        (dirpath / "test.cpp").write_text(CXX, encoding="utf-8")
        args = [compiler, "-std=" + std, "-Wall", "-Wextra", "-Werror",
                "-pedantic", "-O2", "-I", tmp]
        if ndebug:
            args.append("-DNDEBUG")
        exe = dirpath / "policy-test"
        args += [str(dirpath / "test.cpp"), "-o", str(exe)]
        p = subprocess.run(args, capture_output=True, text=True, check=False)
        if p.returncode:
            raise AssertionError("Real policy C++ build failed:\n" + p.stderr)
        return subprocess.run([str(exe)], capture_output=True, text=True,
                              check=False)


class InvalidCapRefusal(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.current = POLICY.read_text(encoding="utf-8")
        old = subprocess.run(
            ["git", "show", PARENT + ":src/IndividualProgressionF14LoginPolicy.h"],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        if old.returncode:
            raise AssertionError("Need pristine immutable IP136 parent with history")
        cls.old = old.stdout

    def test_source_contract_does_not_turn_cap0_into_tbc_authority(self):
        self.assertIn("progressionLimit <= 0 ||", self.old)
        self.assertIn("progressionLimit < 0 || progressionLimit > 18", self.current)
        self.assertIn("progressionLimit == 0 ||", self.current)
        self.assertNotIn("progressionLimit <= 0 ||", self.current)
        self.assertIn("DesiredVerifiedTbcRaceStage(savedStage, level)", self.current)
        self.assertNotIn("ForceUpdateProgressionState(", self.current)
        self.assertNotIn("WorldDatabase", self.current)
        self.assertNotIn("Playerbots", self.current)

    def test_original_old_red_new_green_compiles_and_executes(self):
        for compiler in ("g++", "clang++"):
            self.assertIsNotNone(shutil.which(compiler),
                                 "Both compilers required: " + compiler)
            for std in ("c++17", "c++20"):
                for ndebug in (False, True):
                    with self.subTest(compiler=compiler, std=std, ndebug=ndebug):
                        before = compile_run(self.old, compiler, std, ndebug)
                        self.assertNotEqual(before.returncode, 0,
                                            "Old policy unexpectedly rejected invalid cap")
                        self.assertIn("negative unknown cap", before.stderr)
                        self.assertIn("cap above final supported stage18", before.stderr)
                        after = compile_run(self.current, compiler, std, ndebug)
                        self.assertEqual(after.returncode, 0, after.stderr)
                        self.assertIn("PASS invalid-cap denial", after.stdout)


if __name__ == "__main__":
    unittest.main()
