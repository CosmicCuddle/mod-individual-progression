#!/usr/bin/env python3
"""Native pure-header proof: raw Vanilla <8 iff effective stage is Vanilla.

The real paid-expansion header (not a copy of the function) is compiled and
executed across every uint8 raw stage and representative paid/start facts.
This does NOT compile Core or prove thread scheduling, refresh or client ACK.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

PROGRAM = r"""
#include "IndividualProgressionExpansionPolicy.h"
#include "IndividualProgressionF14Core6PositiveRndbotViewerPolicy.h"
#include <cstdint>
#include <initializer_list>
#include <cstdio>
namespace E = IndividualProgressionExpansionPolicy;
namespace V = CosmicIPF14Core6PositiveRndbot;

static_assert(E::TbcEntryStage == V::FirstTbcStage, "TBC boundary changed");
static_assert(E::LastSupportedStage == V::LastKnownStage, "IP supported stage boundary changed");
static_assert(E::UnpaidWrathAccessCap >= E::TbcEntryStage, "Wrath cap may enter Vanilla");

int main()
{
    unsigned checks = 0;
    unsigned failures = 0;
    for (unsigned raw = 0; raw <= 255; ++raw)
    {
        for (unsigned bits = 0; bits < 16; ++bits)
        {
            for (unsigned start : {0U, 8U, 13U, 18U})
            {
                E::CharacterFacts facts{};
                facts.rawStage = static_cast<std::uint8_t>(raw);
                facts.featureActive = !!(bits & 1);
                facts.gated = !!(bits & 2);
                facts.tbcPaid = !!(bits & 4);
                facts.wrathPaid = !!(bits & 8);
                facts.startStage = static_cast<std::uint8_t>(start);
                std::uint8_t const effective = E::EffectiveStage(facts);
                bool const priorViewerVanilla =
                    effective <= V::LastKnownStage && effective < V::FirstTbcStage;
                // The new guarded adapter never requests paid facts after
                // a positive cached ordinary-human account result.
                bool const newViewerVanilla =
                    raw <= V::LastKnownStage && raw < V::FirstTbcStage;
                ++checks;
                if (priorViewerVanilla != newViewerVanilla)
                {
                    ++failures;
                    std::printf("MISMATCH raw=%u bits=%u start=%u effective=%u\n",
                                raw, bits, start, effective);
                }
            }
        }
    }
    std::printf("F14 RAW VANILLA EQUIVALENCE %u checks; %u failures; no Auth DB or runtime\n",
                checks, failures);
    return failures ? 1 : 0;
}
"""

class RealHeaderVanillaEquivalence(unittest.TestCase):
    def test_compiled_paid_policy_never_moves_tbc_or_wrath_to_vanilla(self):
        with tempfile.TemporaryDirectory(prefix="f14-vanilla-raw-") as work:
            source = Path(work) / "raw_equivalence.cpp"
            source.write_text(PROGRAM, encoding="utf-8")
            for compiler in ("g++", "clang++"):
                for standard in ("c++17", "c++20"):
                    for release in (False, True):
                        with self.subTest(compiler=compiler, std=standard, ndebug=release):
                            exe = Path(work) / "raw_equivalence"
                            cmd = [compiler, "-std=" + standard, "-O2", "-Wall", "-Wextra",
                                   "-Werror", "-pedantic", "-Isrc", str(source), "-o", str(exe)]
                            if release:
                                cmd.insert(2, "-DNDEBUG")
                            built = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
                            self.assertEqual(built.returncode, 0, built.stderr)
                            result = subprocess.run([str(exe)], cwd=ROOT, capture_output=True, text=True)
                            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
                            self.assertIn("16384 checks; 0 failures; no Auth DB", result.stdout)

    def test_real_quest_stage_source_has_no_account_or_paid_db_fallback(self):
        source = (ROOT / "src/IndividualProgression.cpp").read_text(encoding="utf-8")
        body = source.split("uint8 IndividualProgression::GetPlayerProgressionFromQuests(", 1)[1].split(
            "bool IndividualProgression::hasPassedProgression(", 1)[0]
        self.assertIn("GetQuestStatus(PROGRESSION_QUEST)", body)
        for forbidden in ("GetAccountMatch(", "GetExpansionFacts(", "GetEffectiveProgression(",
                          "AccountMgr::", "CharacterDatabase", "LoginDatabase", "WorldDatabase"):
            self.assertNotIn(forbidden, body)

if __name__ == "__main__":
    unittest.main()
