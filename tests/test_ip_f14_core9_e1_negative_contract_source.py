"""Source-scope contract for Core9-shaped, NEGATIVE-ONLY E1 model cases.

No callback wiring, runtime code, SQL, session state or client result is proven.
"""
from __future__ import annotations
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "tests/f14_core9_e1_negative_contract.cpp"
HEADER = ROOT / "src/IndividualProgressionF14SendAttemptCorrelation.h"
WORKFLOW = ROOT / ".github/workflows/ip-f14-core9-e1-negative-contract-20261110.yml"
REGISTER = ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md"


class Core9E1NegativeOnlySource(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.cpp = CPP.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")

    def test_no_positive_match_claim_in_new_model(self) -> None:
        self.assertNotIn(".Matched()", self.cpp)
        self.assertNotIn("C::Result::Matched", self.cpp)
        self.assertIn("ZERO SYNTHETIC MATCHES", self.cpp)
        self.assertIn("!C::IsSendCallClientPresentation()", self.cpp)

    def test_real_missing_facts_all_have_negative_cases(self) -> None:
        for case in (
            "NoHumanEpochCannotBecomeEvidence",
            "NoPriorSuppressionCannotNominate",
            "PostSendGuidCannotSelfNominate",
            "ImmediateDirectRouteHasNoBatchAttribution",
            "GlobalTokensAcrossViewersStayUnmatched",
            "NestedPassIsFailClosedRatherThanProof",
            "UnknownEndCapacityPoisonsButNeverMatches",
        ):
            self.assertRegex(self.cpp, re.escape(case) + r"\(\);")
        for result in (
            "InvalidKey", "NoPriorF14Suppression", "NoCandidate", "PassClosed",
            "Unattributed", "StaleToken", "Capacity", "TablePoisoned",
        ):
            self.assertIn("C::Result::" + result, self.cpp)

    def test_untouched_ledger_is_immutable_under_model(self) -> None:
        self.assertIn("P::Protocol<16> debt;", self.cpp)
        self.assertIn("debt.LedgerView().PendingRestores()", self.cpp)
        self.assertNotIn("ObserveF14SuppressionDecision(", self.cpp)
        self.assertNotIn("ObserveLinkTransition(", self.cpp)

    def test_model_header_still_unwired(self) -> None:
        self.assertIn("constexpr bool IsSendCallClientPresentation() { return false; }", self.header)
        self.assertIn("constexpr bool HasCorePassTokenProducer() { return false; }", self.header)
        sources = list((ROOT / "src").rglob("*.cpp"))
        self.assertTrue(sources)
        live_inclusions = [
            str(path.relative_to(ROOT)) for path in sources
            if re.search(r'^\s*#\s*include\s*["<]IndividualProgressionF14SendAttemptCorrelation.h[">]',
                         path.read_text(encoding="utf-8"), re.MULTILINE)
        ]
        self.assertEqual(live_inclusions, [], "production E1 consumer was unexpectedly wired")

    def test_workflow_is_pinned_and_isolated(self) -> None:
        self.assertIn("e92f6e0e4ecc81d0656d5e66e468a9b13c467478", self.workflow)
        self.assertIn("git diff --name-only", self.workflow)
        self.assertIn("g++", self.workflow)
        self.assertIn("clang++", self.workflow)
        self.assertIn("c++17", self.workflow)
        self.assertIn("c++20", self.workflow)
        self.assertIn("test -z", self.workflow)

    def test_register_has_nonruntime_boundary(self) -> None:
        reg = REGISTER.read_text(encoding="utf-8")
        self.assertIn("IP-F14-CORE9-E1-NEGATIVE-CONTRACT-CS01-20261110", reg)
        self.assertIn("NO CORE LISTENER", reg)


if __name__ == "__main__":
    unittest.main()
