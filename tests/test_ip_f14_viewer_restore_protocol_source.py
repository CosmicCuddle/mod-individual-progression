"""F14 per-viewer restore EVIDENCE protocol: source contracts, not in-game proof.

The protocol is a pure, unwired header. These contracts pin that it stays
value-only, never clears #85's ledger, keeps the four evidence stages
separate, and leaves every frozen parent file and default-OFF flag unchanged.
"""
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = "src/IndividualProgressionF14ViewerRestoreProtocol.h"
PARENT = "6bbd4542e8b8b992c51d21955a341c1f988658c6"  # #85 ledger head
ATTRIBUTION_PR_HEAD = "2933d939bea7ebf9abca224c585ddc3c08744eda"  # #86 (read-only)


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True,
                          capture_output=True, text=True).stdout


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

    def test_only_pure_includes_and_no_pointers(self):
        includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', self.src)
        self.assertEqual(includes, ["IndividualProgressionF14PriorHiddenLedger.h",
                                    "array", "cstddef", "cstdint"])
        body = self.src.split("namespace IndividualProgressionF14ViewerRestoreProtocol", 1)[1]
        self.assertNotRegex(body, r"\w\s*\*\s*\w")  # no pointer declarations
        for forbidden in ("Player", "Creature", "WorldObject", "WorldPacket", "SendPacket",
                          "SendDirectMessage", "UpdateData", "sScriptMgr", "sRandomPlayerbotMgr",
                          "UpdateObjectVisibility", "RemoveRewardedQuest", "mutex", "thread",
                          "new", "delete", "malloc"):
            self.assertIsNone(re.search(r"\b" + forbidden + r"\b", body), forbidden)

    def test_nothing_includes_or_calls_the_protocol(self):
        for path in sorted((ROOT / "src").rglob("*")):
            if path.is_file() and path.name != Path(HEADER).name:
                text = path.read_text(encoding="utf-8", errors="replace")
                self.assertNotIn("IndividualProgressionF14ViewerRestoreProtocol", text, path.name)
        for name in ("CMakeLists.txt",):
            if (ROOT / name).exists():
                self.assertNotIn("ViewerRestoreProtocol", read(name))


class LedgerNeverCleared(unittest.TestCase):
    def setUp(self):
        self.src = strip_comments(read(HEADER))

    def test_no_completion_or_reset_calls(self):
        for forbidden in ("CompleteRestoreOnlyWithClientProof", "ClearFrozenAfterGlobalProof"):
            self.assertNotIn(forbidden, self.src)

    def test_ledger_is_private_and_exposed_const_only(self):
        private = self.src.split("private:", 1)[1]
        self.assertIn("Ledger::Ledger<Capacity> _ledger{};", private)
        public = self.src.split("private:", 1)[0]
        self.assertIn("Ledger::Ledger<Capacity> const& LedgerView() const { return _ledger; }", public)
        self.assertNotRegex(public, r"Ledger::Ledger<Capacity>\s*&\s*\w+\(")

    def test_only_stage_two_decision_feeds_the_ledger(self):
        self.assertEqual(self.src.count("RecordConfirmedF14Suppression("), 1)
        decision = method(self.src, "Outcome ObserveF14SuppressionDecision(")
        self.assertIn("_ledger.RecordConfirmedF14Suppression(viewer, creature, incarnation, true)", decision)
        # The ledger call follows every fact check.
        self.assertLess(decision.index("CheckDecision("), decision.index("RecordConfirmedF14Suppression("))
        transition = method(self.src, "Outcome ObserveLinkTransition(")
        self.assertNotIn("RecordConfirmedF14Suppression", transition)
        flush = method(self.src, "Outcome ObserveProposedBatchFlush(")
        self.assertNotIn("_ledger", flush)

    def test_fail_visible_is_sticky_and_global(self):
        fail = method(self.src, "Outcome FailVisible(FailReason reason)")
        self.assertIn("_unattributedHideRisk = true;", fail)
        self.assertIn("_ledger.RequestGlobalRestore();", fail)
        # One declaration (false) and one assignment (true): nothing clears it.
        self.assertEqual(re.findall(r"_unattributedHideRisk\s*=(?!=)\s*(\w+)", self.src), ["true", "false"])
        self.assertIn("bool _unattributedHideRisk = false;", self.src.split("private:", 1)[1])

    def test_lifecycle_apis_only_add_obligations(self):
        for sig, call in (("void ObserveCreatureRetired(", "_ledger.RevokeCreature(creature)"),
                          ("void ObserveViewerSessionEnded(", "_ledger.RevokeViewer(viewerGuid)"),
                          ("void ObserveViewerEraOrRightsChanged(", "_ledger.RevokeViewer(viewerGuid)"),
                          ("void ObserveConfigDisabled(", "_ledger.RequestGlobalRestore()"),
                          ("void ObserveMapUnloaded(", "_ledger.RevokeCreature(_records[i].creature)")):
            self.assertIn(call, method(self.src, sig), sig)


class StagesStaySeparate(unittest.TestCase):
    def setUp(self):
        self.src = strip_comments(read(HEADER))

    def test_client_stage_has_one_value_and_no_setter(self):
        enum = self.src.split("enum class ClientStage : std::uint8_t", 1)[1].split("};", 1)[0]
        self.assertEqual([t.strip() for t in enum.strip(" \n{").split(",") if t.strip()],
                         ["NotServerObservable"])
        self.assertIn("constexpr ClientStage ClientStageOf() { return ClientStage::NotServerObservable; }",
                      self.src)
        self.assertNotIn("ClientStage client", self.src)

    def test_batched_handed_only_via_proposed_hook(self):
        self.assertEqual(self.src.count("PacketStage::HandedToSessionBatched"), 1)  # flush only
        self.assertIn("stage = PacketStage::HandedToSessionBatched;",
                      method(self.src, "Outcome ObserveProposedBatchFlush("))
        decision = method(self.src, "Outcome ObserveF14SuppressionDecision(")
        transition = method(self.src, "Outcome ObserveLinkTransition(")
        for body in (decision, transition):
            self.assertNotIn("HandedToSessionBatched", body)
            self.assertIn(": PacketStage::ImpliedPendingBatchFlush", body)

    def test_ordinary_transitions_are_never_suppression(self):
        transition = method(self.src, "Outcome ObserveLinkTransition(")
        self.assertIn("if (index == Capacity || !nowLinked)\n                return Outcome::IgnoredOrdinary;",
                      transition)
        self.assertNotIn("suppressServer", transition)
        self.assertNotIn("suppressPacket", transition)
        self.assertNotIn("suppressIntent", transition)

    def test_default_facts_are_fail_visible(self):
        for struct in ("struct DecisionFacts", "struct TransitionFacts"):
            body = self.src.split(struct, 1)[1].split("};", 1)[0]
            self.assertNotRegex(body, r"=\s*true")
            for value in re.findall(r"=\s*([^;]+);", body):
                self.assertIn(value.strip(), ("false", "0"))
        check = method(self.src, "static FailReason CheckDecision(")
        for token in ("!facts.configEnabled", "!facts.actorPositivelyVerifiedRandomBot",
                      "!facts.actorEntitled", "!facts.pairRightsProven",
                      "viewer.loginEpoch != facts.currentViewerEpoch",
                      "incarnation != facts.currentCreatureIncarnation", "!facts.creatureAlive",
                      "facts.eventMapId != creature.mapId", "facts.eventInstanceId != creature.instanceId"):
            self.assertIn(token, check)


class ParentsAndFlagsUnchanged(unittest.TestCase):
    def test_frozen_files_byte_identical_to_parent(self):
        for name in ("src/IndividualProgressionF14PriorHiddenLedger.h",
                     "src/IndividualProgressionF14LifecycleRegistry.h",
                     "src/IndividualProgressionF14LifecycleScript.cpp",
                     "src/IndividualProgressionF14LogoutRefreshQueue.h",
                     "src/IndividualProgressionF14CombatVisibilityPolicy.h",
                     "src/IndividualProgression.cpp", "src/IndividualProgression.h",
                     "src/IndividualProgressionPlayer.cpp", "conf/individualProgression.conf.dist"):
            self.assertEqual(read(name), git("show", PARENT + ":" + name), name)

    def test_pair_rights_remain_default_false(self):
        # The protocol neither reads nor answers the pair-rights policy; the
        # policy file is byte-identical to the parent (above) and still needs
        # the creature snapshot field that every runtime builder keeps false.
        header = strip_comments(read(HEADER))
        for name in ("ShouldConcealCreatureFromObserver", "IsConcealmentCandidate",
                     "CombatVisibilityPolicy", "CreatureSnapshot", "ObserverSnapshot"):
            self.assertNotIn(name, header)
        policy = strip_comments(read("src/IndividualProgressionF14CombatVisibilityPolicy.h"))
        conceal = method(policy, "constexpr bool ShouldConcealCreatureFromObserver(")
        self.assertIn("c.plainUnscriptedNonEliteHostileNpc &&", conceal)
        self.assertIn("bool plainUnscriptedNonEliteHostileNpc = false;", policy)
        for path in sorted((ROOT / "src").rglob("*")):
            if path.is_file():
                self.assertNotRegex(strip_comments(path.read_text(encoding="utf-8", errors="replace")),
                                    r"plainUnscriptedNonEliteHostileNpc\s*=\s*true", path.name)

    def test_refresh_macro_undefined_and_flags_off(self):
        for path in sorted((ROOT / "src").rglob("*")):
            if path.is_file():
                self.assertNotRegex(path.read_text(encoding="utf-8", errors="replace"),
                                    r"#\s*define\s+COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", path.name)
        conf = read("conf/individualProgression.conf.dist")
        for key in re.findall(r"(?m)^(IndividualProgression\.(?:F14\.\w+|PaidExpansion\.Enable))\s*=", conf):
            self.assertRegex(conf, r"(?m)^" + re.escape(key) + r"\s*=\s*0\s*$", key)


if __name__ == "__main__":
    unittest.main()
