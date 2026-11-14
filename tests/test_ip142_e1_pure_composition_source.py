"""IP145 N1 child: exact model-only same-epoch invalidation correction."""
from pathlib import Path
import hashlib
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[1]
PARENT="f259c8593f7a8ac5521bee343c40d5a2a35fd76b"
HEADER=ROOT/"src/IndividualProgressionF14SendAttemptCorrelation.h"
E1_ORIGINAL="4410ece8b9a80b5823c18aee214949e78fb16113"
ALLOWED={
"src/IndividualProgressionF14SendAttemptCorrelation.h",
"tests/f14_send_attempt_correlation.cpp",
"tests/test_ip142_e1_pure_composition_source.py",
"docs/F14_SEND_ATTEMPT_CORRELATION_2026-11-08.md",
"docs/CUSTOM_MODIFICATIONS_REGISTER.md",
".github/workflows/ip142-e1-pure-compose-native-20261111.yml",
}
def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])
def blob(data):
    return hashlib.sha1(b"blob %d\0" % len(data)+data).hexdigest()

class SourceOnlyE1Composition(unittest.TestCase):
    def test_frozen_ip145_header_and_exact_n1_method_only(self):
        parent="65d0110ec7b34a6d284833e235cb0a98f2e50f55"
        original=git("show",parent+":src/IndividualProgressionF14SendAttemptCorrelation.h")
        self.assertEqual(blob(original),E1_ORIGINAL)
        current=HEADER.read_text()
        start=current.index("        // Rights/era changes do not end")
        stop=current.index("        void ObserveCreatureRetired",start)
        body=current[start:stop]
        self.assertIn("pass.closedThrough = pass.lastToken;",body)
        self.assertIn("DropIf([&](Candidate const& c) { return c.viewer == pass.viewer; });",body)
        self.assertNotIn("ForgetViewer(",body)
        self.assertNotIn("pass.poisoned =",body)
        reconstructed=current[:start]+("        void ObserveViewerRightsOrEraChanged(std::uint64_t viewerGuid) { ForgetViewer(viewerGuid); }\n")+current[stop:]
        self.assertEqual(reconstructed.encode(),original)

    def test_unchanged_ip142_runtime_protocol_ledger_and_configs(self):
        names=git("ls-tree","-r","--name-only",PARENT).decode().splitlines()
        self.assertGreater(len(names),100)
        for path in names:
            if path in ALLOWED:
                continue
            self.assertEqual((ROOT/path).read_bytes(),git("show",PARENT+":"+path),path)

    def test_exact_six_paths_and_register_append_only(self):
        changed=set(git("diff","--name-only",PARENT,"HEAD").decode().splitlines())
        self.assertEqual(changed,ALLOWED)
        before=git("show",PARENT+":docs/CUSTOM_MODIFICATIONS_REGISTER.md")
        after=(ROOT/"docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
        self.assertTrue(after.startswith(before.rstrip()))
        self.assertIn(b"IP-F14-E1-IP142-COMPOSITION-CS01-20261111",after)
        parent="65d0110ec7b34a6d284833e235cb0a98f2e50f55"
        changed_child=set(git("diff","--name-only",parent,"HEAD").decode().splitlines())
        self.assertEqual(changed_child, ALLOWED-{"docs/F14_SEND_ATTEMPT_CORRELATION_2026-11-08.md"})
        self.assertTrue(after.startswith(git("show",parent+":docs/CUSTOM_MODIFICATIONS_REGISTER.md")))
        self.assertIn(b"IP-F14-E1-N1-RIGHTS-ERA-TOMBSTONE-20261112",after)

    def test_unwired_no_fake_client_ack_or_core_token(self):
        src=HEADER.read_text()
        self.assertIn("constexpr bool IsSendCallClientPresentation() { return false; }",src)
        self.assertIn("constexpr bool HasCorePassTokenProducer() { return false; }",src)
        self.assertIn("bool sourceAttributedByExactCoreF14Hook = false;",src)
        self.assertIn("bool globalRestoreRequired = true;",src)
        self.assertIn("bool priorF14SuppressionRecorded = false;",src)
        self.assertIn("return Result::PassClosed;",src)
        self.assertIn("void ForgetViewer(",src)
        self.assertNotIn("SendDirectMessage(",src)
        for path in (ROOT/"src").rglob("*"):
            if path.is_file() and path != HEADER:
                self.assertNotIn("IndividualProgressionF14SendAttemptCorrelation.h",
                                 path.read_text(errors="replace"),path.name)

    def test_original_debt_header_preserved_and_snapshot_stays_readonly(self):
        p=ROOT/"src/IndividualProgressionF14ViewerRestoreProtocol.h"
        self.assertEqual(p.read_bytes(),git("show",PARENT+":"+str(p.relative_to(ROOT))))
        self.assertIn("CopyExactPendingRestores(",p.read_text())
        l=ROOT/"src/IndividualProgressionF14PriorHiddenLedger.h"
        self.assertEqual(l.read_bytes(),git("show",PARENT+":"+str(l.relative_to(ROOT))))
        self.assertIn("CompleteRestoreOnlyWithClientProof",l.read_text())

if __name__=="__main__":
    unittest.main()
