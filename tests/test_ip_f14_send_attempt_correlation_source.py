"""F14 E1 send-attempt correlation: source contracts, not in-game proof.

Pins that the correlation table stays pure, value-only and unwired; reads the
#94 protocol only through const accessors; never calls the #103 adapter or a
mutating/clearing #94 API; keeps every fail-closed default; and leaves the
frozen #94/#103/#104 files, runtime source, config and flags unchanged.
"""
import hashlib
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = "src/IndividualProgressionF14SendAttemptCorrelation.h"
PARENT = "3c273c8060eb9f1f90c01f76669b9bec0d1d8935"  # IP #104 head (frozen)
# Original #94 blobs (head 61302474), carried unchanged through #97..#104.
ORIGINAL_94_BLOBS = {
    "src/IndividualProgressionF14PriorHiddenLedger.h": "2250bae56bb09a810d337927f738bd3cbcf5fddd",
    "src/IndividualProgressionF14ViewerRestoreProtocol.h": "f520356a8152d98580751604f4649fe1b564f072",
    "tests/f14_viewer_restore_protocol.cpp": "9bce90ed2c3e0f05dc41583d8e96b4f53f661422",
}
NEW_PATHS = {
    HEADER,
    "tests/f14_send_attempt_correlation.cpp",
    "tests/test_ip_f14_send_attempt_correlation_source.py",
    "docs/F14_SEND_ATTEMPT_CORRELATION_2026-11-08.md",
    ".github/workflows/ip-f14-send-attempt-correlation-20261108.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True,
                          capture_output=True).stdout


def blob_sha(data):
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def method(src, signature):
    rest = src.split(signature, 1)[1]
    depth, started = 0, False
    for i, ch in enumerate(rest):
        if ch == "{":
            depth += 1
            started = True
        elif ch == "}":
            depth -= 1
            if started and depth == 0:
                return rest[:i + 1]
    raise AssertionError("unbalanced " + signature)


class PureAndUnwired(unittest.TestCase):
    def setUp(self):
        self.src = strip_comments(read(HEADER))
        self.body = self.src.split("namespace IndividualProgressionF14SendAttemptCorrelation", 1)[1]

    def test_only_pure_includes(self):
        self.assertEqual(re.findall(r'#include\s+[<"]([^>"]+)[>"]', self.src),
                         ["IndividualProgressionF14ViewerRestoreProtocol.h", "array", "cstddef", "cstdint"])

    def test_no_pointers_core_types_or_threads(self):
        self.assertNotRegex(self.body, r"\w\s*\*\s*\w")
        for word in ("Player", "Creature", "WorldObject", "ObjectGuid", "Map", "WorldPacket",
                     "SendPacket", "SendDirectMessage", "UpdateData", "sScriptMgr",
                     "sRandomPlayerbotMgr", "mutex", "thread", "atomic", "new", "delete", "malloc"):
            self.assertIsNone(re.search(r"\b" + word + r"\b", self.body), word)

    def test_nothing_includes_or_registers_the_model(self):
        for path in sorted((ROOT / "src").rglob("*")):
            if path.is_file() and path.name != Path(HEADER).name:
                self.assertNotIn("IndividualProgressionF14SendAttemptCorrelation",
                                 path.read_text(encoding="utf-8", errors="replace"), path.name)

    def test_never_asserts_presentation_or_a_core_token(self):
        self.assertIn("constexpr bool IsSendCallClientPresentation() { return false; }", self.src)
        self.assertIn("constexpr bool HasCorePassTokenProducer() { return false; }", self.src)


class ProtocolIsReadOnly(unittest.TestCase):
    def setUp(self):
        self.src = strip_comments(read(HEADER))

    def test_protocol_only_by_const_reference(self):
        params = re.findall(r"Restore::Protocol<\w+>([^,)]*)", self.src)
        self.assertEqual(len(params), 1)
        self.assertEqual(params[0].strip(), "const& protocol")

    def test_only_const_protocol_accessors(self):
        calls = set(re.findall(r"\bprotocol\.(\w+)\(", self.src))
        self.assertEqual(calls, {"Find", "GlobalRestoreRequired", "NeedsRestore"})

    def test_no_adapter_ledger_or_clearing_calls(self):
        for name in ("ObserveExactTaggedBatchAttempt", "GuidSendAttemptEvidence", "ObserveProposedBatchFlush",
                     "ObserveF14SuppressionDecision", "ObserveLinkTransition", "ObserveConfigDisabled",
                     "RecordConfirmedF14Suppression", "RequestGlobalRestore", "RevokeCreature", "RevokeViewer",
                     "CompleteRestoreOnlyWithClientProof", "ClearFrozenAfterGlobalProof", "LedgerView"):
            self.assertNotIn(name, self.src, name)


class FailClosedRules(unittest.TestCase):
    def setUp(self):
        self.src = strip_comments(read(HEADER))

    def test_defaults_fail_closed(self):
        nom = self.src.split("struct NominationFacts", 1)[1].split("};", 1)[0]
        self.assertIn("bool globalRestoreRequired = true;", nom)
        self.assertIn("bool priorF14SuppressionRecorded = false;", nom)
        self.assertIn("bool restoreOwed = false;", nom)
        send = self.src.split("struct SendFacts", 1)[1].split("};", 1)[0]
        self.assertIn("bool sourceAttributedByExactCoreF14Hook = false;", send)
        self.assertIn("bool globalRestoreRequired = true;", send)
        self.assertIn("std::uint64_t passToken = 0;", send)

    def test_nomination_checks_precede_any_state_change(self):
        nominate = method(self.src, "Result Nominate(")
        first_state = nominate.index("AdvancePass(")
        for token in ("passToken == 0", "viewer.loginEpoch != facts.currentViewerEpoch",
                      "incarnation != facts.currentCreatureIncarnation",
                      "facts.eventMapId != creature.mapId", "facts.globalRestoreRequired",
                      "!facts.priorF14SuppressionRecorded",
                      "direction == Direction::OutOfRange && facts.restoreOwed",
                      "direction == Direction::Create && !facts.restoreOwed"):
            self.assertLess(nominate.index(token), first_state, token)

    def test_tokens_must_strictly_increase(self):
        advance = method(self.src, "Result AdvancePass(")
        self.assertIn("if (passToken < pass.lastToken)", advance)
        self.assertIn("pass.poisoned = true;", advance)
        self.assertIn("return Result::TokenReorderedOrReused;", advance)
        self.assertIn("return Result::UnobservedSessionChange;", advance)
        correlate = method(self.src, "Result CorrelateInner(")
        self.assertIn("if (f.passToken < pass.lastToken)\n                return Result::StaleToken;", correlate)

    def test_ambiguity_and_cross_map_fail_closed(self):
        correlate = method(self.src, "Result CorrelateInner(")
        self.assertIn("if (matches > 1 || ambiguous)", correlate)
        self.assertIn("return Result::Ambiguous;", correlate)
        self.assertLess(correlate.index("RemoveAt(index);"), correlate.index("return Result::CrossMap;"))
        self.assertLess(correlate.index("return Result::GlobalFreeze;"), correlate.index("FindViewerByGuid("))

    def test_overflow_poisons_without_eviction(self):
        poison = method(self.src, "void PoisonTable()")
        self.assertIn("_poisoned = true;", poison)
        self.assertEqual(len(re.findall(r"_poisoned\s*=(?!=)\s*(\w+)", self.src)), 2)  # decl + poison
        self.assertIn("bool _poisoned = false;", self.src)


class ParentsAndFlagsUnchanged(unittest.TestCase):
    def test_original_94_blobs_unchanged(self):
        for name, sha in ORIGINAL_94_BLOBS.items():
            self.assertEqual(blob_sha((ROOT / name).read_bytes()), sha, name)

    def test_every_parent_file_outside_scope_is_byte_identical(self):
        tracked = git("ls-tree", "-r", "--name-only", PARENT).decode().splitlines()
        self.assertGreater(len(tracked), 50)
        for name in tracked:
            if name in NEW_PATHS:
                continue
            self.assertEqual((ROOT / name).read_bytes(), git("show", PARENT + ":" + name), name)

    def test_refresh_and_observer_macros_undefined_and_flags_off(self):
        for path in sorted((ROOT / "src").rglob("*")):
            if path.is_file():
                text = path.read_text(encoding="utf-8", errors="replace")
                for macro in ("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION",
                              "COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT"):
                    self.assertNotRegex(text, r"#\s*define\s+" + macro, path.name)
        conf = read("conf/individualProgression.conf.dist")
        keys = re.findall(r"(?m)^(IndividualProgression\.(?:F14\.\w+|PaidExpansion\.Enable))\s*=", conf)
        self.assertGreater(len(keys), 3)
        for key in keys:
            self.assertRegex(conf, r"(?m)^" + re.escape(key) + r"\s*=\s*0\s*$", key)


if __name__ == "__main__":
    unittest.main()
