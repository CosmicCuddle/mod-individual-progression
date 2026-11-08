#!/usr/bin/env python3
"""Compile and RUN the exact old/new IP player-login gate extracted from source.

No mock entitlement, packet, hide, database or live server. A legacy compile
failure is NOT counted as a bug reproduction; the original must compile and
run but FAIL the positive diagnostic-lease case, then the child must PASS.
"""
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PLAYER = ROOT / "src" / "IndividualProgressionPlayer.cpp"
OLD = "73e2748e0ab5bd3078cdb045a8f93f57605dd286"
MACRO = "COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT"


def predicate(source):
    login = source.split("void OnPlayerLogin(Player* player) override", 1)[1]
    after = login.split("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE", 1)[1]
    block = after.split("#endif", 1)[0]
    body = block[block.index("        if (("):].strip()
    if body.count("RememberF14VerifiedRandomBotGroupIdentity(") != 1:
        raise AssertionError("Expected original single identity producer call")
    if "sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())" not in body:
        raise AssertionError("Original positive RandomBot source verification lost")
    if "f14SourceLease);" not in body:
        raise AssertionError("Original captured lease parameter lost")
    if "sIndividualProgression->isBotAccount(player)" not in body:
        raise AssertionError("Original account and session checks lost")
    if "player->GetSession()->IsHeadless()" not in body:
        raise AssertionError("Original genuine headless requirement lost")
    return body


MOCK_PREFIX = r"""
#include <cstdint>
#include <iostream>
#define RACE_DRAENEI 11
#define RACE_BLOODELF 10
struct Lease { bool publicationWasEnabled = false; };
struct Guid { std::uint32_t GetCounter() const { return 123; } };
struct Session { bool headless = true; bool IsHeadless() const { return headless; } };
struct Player {
    int race = 10;
    bool hasSession = true;
    Session session;
    int getRace() const { return race; }
    Session const* GetSession() const { return hasSession ? &session : nullptr; }
    Guid GetGUID() const { return {}; }
};
struct IP {
    bool f14ProtectVerifiedRandomBotGroupFloor = false;
    bool f14ExperimentalRndbotPlayerVisibility = false;
    bool f14ExperimentalAttackLifecycleRegistry = false;
    bool f14ProtectVerifiedRandomBotStageFloor = false;
    bool botAccount = true;
    int called = 0;
    bool verified = false;
    bool leaseReceived = false;
    bool isBotAccount(Player*) const { return botAccount; }
    void RememberF14VerifiedRandomBotGroupIdentity(Player*, bool v, Lease l)
    {
        called++;
        verified = v;
        leaseReceived = l.publicationWasEnabled;
    }
} ip;
IP* sIndividualProgression = &ip;
struct RandomBotMgr {
    bool verified = true;
    bool IsRandomBot(std::uint32_t) const { return verified; }
} sRandomPlayerbotMgr;
void CallOriginalLoginProducer(Player* player, Lease f14SourceLease)
{
"""

MOCK_SUFFIX = r"""
}
int main()
{
    int failures = 0;
    auto check = [&](bool valid, char const* name) {
        if (!valid) {
            std::cerr << "FAIL " << name << "\n";
            failures++;
        }
    };
    Player player;
    auto reset = [&] {
        ip = IP{};
        player = Player{};
        sRandomPlayerbotMgr.verified = true;
    };
    reset();
    CallOriginalLoginProducer(&player, Lease{false});
    check(ip.called == 0, "ordinary/default OFF without legacy flag");
    reset();
    CallOriginalLoginProducer(&player, Lease{true});
    check(ip.called == 1 && ip.verified && ip.leaseReceived,
          "diagnostic publication with captured positive lease and original verifier");
    reset();
    ip.f14ProtectVerifiedRandomBotGroupFloor = true;
    CallOriginalLoginProducer(&player, Lease{false});
    check(ip.called == 1 && ip.verified && !ip.leaseReceived,
          "legacy group-floor publication path unchanged with lease OFF");
    reset();
    ip.botAccount = false;
    CallOriginalLoginProducer(&player, Lease{true});
    check(ip.called == 0, "non-bot and excluded-human refusal");
    reset();
    player.race = 1;
    CallOriginalLoginProducer(&player, Lease{true});
    check(ip.called == 0, "non-TBC race refusal");
    reset();
    player.session.headless = false;
    CallOriginalLoginProducer(&player, Lease{true});
    check(ip.called == 0, "real human/Altbot headless proof absent");
    reset();
    player.hasSession = false;
    CallOriginalLoginProducer(&player, Lease{true});
    check(ip.called == 0, "session absent");
    reset();
    sRandomPlayerbotMgr.verified = false;
    CallOriginalLoginProducer(&player, Lease{true});
    check(ip.called == 1 && !ip.verified,
          "unverified identity passed false to original writer, never positive");
    if (failures == 0)
        std::cout << "PASS real extracted login source gate, 8 cases\n";
    return failures ? 1 : 0;
}
"""


def compile_run(compiler, std, text):
    source = MOCK_PREFIX + "\n" + predicate(text) + "\n" + MOCK_SUFFIX
    with tempfile.TemporaryDirectory() as root:
        output = str(Path(root) / "login-gate")
        build = subprocess.run(
            [compiler, "-x", "c++", "-std=" + std,
             "-Wall", "-Wextra", "-Werror", "-O2", "-o", output, "-"],
            input=source, text=True, capture_output=True, check=False,
        )
        if build.returncode:
            raise AssertionError("Actual extracted source did not compile:\n" +
                                 build.stderr)
        return subprocess.run([output], text=True, capture_output=True, check=False)


class F14PublicationLoginPredicate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.new = PLAYER.read_text(encoding="utf-8")
        old = subprocess.run(
            ["git", "show", OLD + ":src/IndividualProgressionPlayer.cpp"],
            cwd=ROOT, text=True, capture_output=True, check=False,
        )
        if old.returncode:
            raise AssertionError("Must checkout immutable old parent with full ancestry")
        cls.old = old.stdout

    def test_parent_scope_and_original_false_only_adapter(self):
        old = predicate(self.old)
        new = predicate(self.new)
        self.assertNotIn("f14SourceLease.publicationWasEnabled ||", old)
        self.assertIn("f14SourceLease.publicationWasEnabled ||", new)
        self.assertNotIn(MACRO, self.new)  # inherited strict negative source guard
        self.assertEqual(
            re.findall(r"sRandomPlayerbotMgr\.IsRandomBot\(", new).__len__(), 1
        )
        self.assertIn(
            "void OnPlayerLogin(Player* player) override", self.new
        )
        adapter = self.new.split(
            "class IndividualProgressionF14Core6NegativeActorWhoAdapter final", 1
        )[1].split("void AddSC_mod_individual_progression_player()", 1)[0]
        self.assertEqual(adapter.count("return false;"), 2)
        self.assertNotIn("TryReadF14VerifiedActorPublication(", adapter)

    def test_actual_old_red_new_green_runtime_behavior(self):
        for compiler in ("g++", "clang++"):
            self.assertTrue(shutil.which(compiler), compiler + " required")
            for std in ("c++17", "c++20"):
                with self.subTest(compiler=compiler, std=std):
                    old = compile_run(compiler, std, self.old)
                    self.assertNotEqual(old.returncode, 0,
                                        "Old source unexpectedly satisfies new diagnostic lease")
                    self.assertIn("diagnostic publication", old.stderr)
                    new = compile_run(compiler, std, self.new)
                    self.assertEqual(new.returncode, 0,
                                     "New source failed:\n" + new.stderr)
                    self.assertIn("PASS real extracted login source gate", new.stdout)


if __name__ == "__main__":
    unittest.main()
