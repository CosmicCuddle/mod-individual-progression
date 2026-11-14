"""F14 human viewer session epoch source contracts on top of unchanged #104.

No positive F14 visibility, no session/creature GUID ledger and no client ACK.
"""
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = "3c273c8060eb9f1f90c01f76669b9bec0d1d8935"
LIFE = "src/IndividualProgressionF14LifecycleScript.cpp"
HEADER = "src/IndividualProgressionF14HumanViewerEpoch.h"
CHANGED = {
    LIFE, HEADER,
    "tests/f14_human_viewer_session_epoch.cpp",
    "tests/test_ip_f14_human_viewer_session_epoch_source.py",
    ".github/workflows/ip-f14-human-viewer-session-epoch-20261105.yml",
    "docs/F14_HUMAN_VIEWER_SESSION_EPOCH_2026-11-05.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}
MACRO = ("defined(COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT) && "
         "COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT >= 1")

def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], text=True,
                          capture_output=True, check=True).stdout.strip()

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

class Contracts(unittest.TestCase):
    def test_exact_seven_paths_preserve_core_paid_original_claude(self):
        self.assertEqual(set(git("diff", "--name-only", BASE, "HEAD").splitlines()), CHANGED)
        self.assertEqual(git("diff", "--name-only", BASE, "HEAD", "--",
                             "conf", "sql", "data", "optional",
                             "src/IndividualProgression.cpp",
                             "src/IndividualProgressionExpansion.cpp",
                             "src/IndividualProgressionPlayer.cpp",
                             "src/IndividualProgressionF14ViewerRestoreProtocol.h",
                             "src/IndividualProgressionF14PriorHiddenLedger.h"), "")
        preserved = {
            "src/IndividualProgressionF14LifecycleRegistry.h": "2d6bf7fb473ae118be35a0c9f98276b8bed1e2e8",
            "src/IndividualProgressionF14ViewerRestoreProtocol.h": "f520356a8152d98580751604f4649fe1b564f072",
            "src/IndividualProgressionF14PriorHiddenLedger.h": "2250bae56bb09a810d337927f738bd3cbcf5fddd",
        }
        for filename, sha in preserved.items():
            self.assertEqual(git("rev-parse", "HEAD:" + filename), sha)
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionF14GuidSendAttemptEvidence.h"),
                         "f89e87428ffe173468ad60a3cfcad1b4f6bf1c81")

    def test_reversing_only_new_epoch_code_yields_exact_original_104_lifecycle(self):
        old = git("show", BASE + ":" + LIFE) + "\n"
        now = read(LIFE)
        include = '#include "WorldSession.h"\n#include "IndividualProgressionF14HumanViewerEpoch.h"\n'
        self.assertEqual(now.count(include), 1)
        now = now.replace(include, "", 1)

        begin = "namespace\n{\n    namespace ViewerEpoch = IndividualProgressionF14HumanViewerEpoch;\n"
        end = "class IndividualProgressionF14TaggedGuidObserverScript final : public WorldObjectScript"
        self.assertEqual(now.count(begin), 1)
        self.assertEqual(now.count(end), 1)
        first = now.index(begin)
        last = now.index(end, first)
        now = now[:first] + now[last:]

        check = ('            guid.IsEmpty() || !guid.IsCreature() ||\n'
                 '            CurrentHumanViewerEpoch(viewer) == 0)')
        self.assertEqual(now.count(check), 1)
        now = now.replace(check, '            guid.IsEmpty() || !guid.IsCreature())', 1)
        stamp_registration = "    new IndividualProgressionF14HumanViewerEpochScript();\n"
        self.assertEqual(now.count(stamp_registration), 1)
        now = now.replace(stamp_registration, "", 1)
        self.assertEqual(now, old)

    def test_epoch_record_from_human_login_and_revoked_on_logout(self):
        code = read(LIFE)
        header = read(HEADER)
        self.assertIn("class IndividualProgressionF14HumanViewerEpochScript final : public PlayerScript", code)
        self.assertIn('PlayerScript("IndividualProgression_F14HumanViewerEpoch")', code)
        self.assertIn("void OnPlayerLogin(Player* player) override", code)
        self.assertIn("void OnPlayerLogout(Player* player) override", code)
        self.assertIn("GetDefault<HumanViewerSessionStamp>(ViewerEpochStampKey)", code)
        self.assertIn("viewer->CustomData.Get<HumanViewerSessionStamp>(ViewerEpochStampKey)", code)
        self.assertIn("old->stamp.Clear();", code)
        self.assertIn("state->stamp.Clear();", code)
        self.assertIn("current->stamp.Bind(player->GetGUID().GetRawValue(), epoch);", code)
        self.assertIn("CurrentHumanViewerEpoch(viewer) == 0", code)
        self.assertIn("viewer->GetSession()->IsHeadless()", code)
        self.assertIn("player->GetSession()->IsHeadless()", code)
        self.assertIn("!player->GetGUID().IsPlayer()", code)
        self.assertIn("!sIndividualProgression->isNormalAccount(player)", code)
        self.assertIn("!sIndividualProgression->isNormalAccount(const_cast<Player*>(viewer))", code)
        self.assertIn("if (!viewer || !viewer->IsInWorld()", code)
        self.assertIn("compare_exchange_weak", header)
        self.assertIn("if (current == std::numeric_limits<std::uint64_t>::max())", header)
        self.assertIn("return 0;", header)
        self.assertIn("constexpr bool AuthorizesCreatureConcealment() { return false; }", header)
        self.assertIn("constexpr bool HasProvenClientRestoration() { return false; }", header)
        self.assertIn("constexpr bool HasProvenOwningMapThread() { return false; }", header)
        self.assertEqual(code.count("new IndividualProgressionF14HumanViewerEpochScript();"), 1)

    def test_off_macro_only_no_new_positive_policy_or_original_protocol_changes(self):
        code = read(LIFE)
        self.assertIn("#if " + MACRO, code)
        self.assertNotIn("OnF14ShouldSuppressCreatureForViewer(", code)
        assert code.count("return false;") >= 1
        new_code = code.split("namespace\n{\n    namespace ViewerEpoch =", 1)[1]
        for forbidden in (
            "ObserveProposedBatchFlush(",
            "CompleteRestoreOnlyWithClientProof(",
            "ClearFrozenAfterGlobalProof(",
            "SendDirectMessage(", "SetPhaseMask(", "UpdateObjectVisibility(",
            "RecordCreatureHidden(", "sRandomPlayerbotMgr",
        ):
            self.assertNotIn(forbidden, new_code)
        conf = read("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0", conf)
        self.assertIn("IndividualProgression.PaidExpansion.Enable = 0", conf)
        self.assertNotIn("COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT", conf)
        self.assertNotIn("IndividualProgressionF14HumanViewerEpoch.h",
                         read("src/IndividualProgression.cpp"))

if __name__ == "__main__":
    unittest.main()
