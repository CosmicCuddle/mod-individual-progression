#!/usr/bin/env python3
"""Compile and RUN the exact real IP F14 cached-human reader in a standalone shim.

The runtime C++ method body is extracted verbatim; no copy/paste approximation.
This proves its cache-hit/miss behavior, NOT its actual AzerothCore thread
scheduling, Auth-DB latency, config reload, player rights or a Worldserver.
"""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "src/IndividualProgression.cpp"
PARENT = "e9ed33511a66bfe3207c7badfc4905c58b6c6f65"
SIGNATURE = "bool IndividualProgression::TryReadF14CachedNormalHumanAccount(uint32 accountId)"


def verbatim_body():
    text = CPP.read_text(encoding="utf-8")
    begin = text.index(SIGNATURE)
    end = text.index("\n}\n#endif", begin) + 2
    return text[begin:end]


def program():
    method = verbatim_body()
    return r"""
#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>
using uint32 = std::uint32_t;

class IndividualProgression {
public:
    struct AccountMatch { bool excluded = false; bool bot = false; bool gm = false; };
    std::unordered_map<uint32, AccountMatch> _accountMatchCache;
    std::shared_mutex _accountMatchLock;
    bool TryReadF14CachedNormalHumanAccount(uint32 accountId);

    void Seed(uint32 id, AccountMatch facts) {
        std::unique_lock<std::shared_mutex> lock(_accountMatchLock);
        _accountMatchCache[id] = facts;
    }
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(_accountMatchLock);
        _accountMatchCache.clear();
    }
};
""" + method + r"""

int main() {
    IndividualProgression ip;
    unsigned checks = 0;
    unsigned fails = 0;
    auto test = [&](bool yes, char const* msg) {
        ++checks;
        if (!yes) { ++fails; std::cerr << "FAIL " << msg << '\n'; }
    };
    test(!ip.TryReadF14CachedNormalHumanAccount(0), "zero account never normal");
    test(!ip.TryReadF14CachedNormalHumanAccount(42), "uncached nonzero must fail visible");
    ip.Seed(42, {false,false,false});
    test(ip.TryReadF14CachedNormalHumanAccount(42), "positively cached ordinary human");
    ip.Seed(43, {true,false,false});
    ip.Seed(44, {false,true,false});
    ip.Seed(45, {false,false,true});
    ip.Seed(46, {true,true,true});
    test(!ip.TryReadF14CachedNormalHumanAccount(43), "excluded account not human");
    test(!ip.TryReadF14CachedNormalHumanAccount(44), "bot account not human");
    test(!ip.TryReadF14CachedNormalHumanAccount(45), "GM account not ordinary");
    test(!ip.TryReadF14CachedNormalHumanAccount(46), "multi-role account not ordinary");
    test(!ip.TryReadF14CachedNormalHumanAccount(47), "unknown not a normal account");
    ip.Clear();
    test(!ip.TryReadF14CachedNormalHumanAccount(42), "config reload clear fails visible");
    ip.Seed(42, {false,false,true});
    test(!ip.TryReadF14CachedNormalHumanAccount(42), "reclassified GM fails visible");
    ip.Seed(42, {false,false,false});
    test(ip.TryReadF14CachedNormalHumanAccount(42), "fresh ordinary cache may be allowed");

    // Concurrent snapshots must not look through an erased account, leak
    // a previous GM result or race on the map while its owner reloads it.
    std::atomic<unsigned> violations{0};
    std::vector<std::thread> workers;
    for (int t = 0; t < 6; ++t)
        workers.emplace_back([&,t]{
            for (int i = 0; i < 2000; ++i) {
                if (ip.TryReadF14CachedNormalHumanAccount(0) ||
                    ip.TryReadF14CachedNormalHumanAccount(43) ||
                    ip.TryReadF14CachedNormalHumanAccount(44) ||
                    ip.TryReadF14CachedNormalHumanAccount(45) ||
                    ip.TryReadF14CachedNormalHumanAccount(47))
                    ++violations;
                // This may legitimately change during the writer's lock;
                // the reader must only observe an intact cached snapshot.
                bool const current = ip.TryReadF14CachedNormalHumanAccount(42);
                (void) current;
            }
        });
    workers.emplace_back([&] {
        for (int i = 0; i < 2000; ++i) {
            ip.Seed(42, {false,false,(i % 2) != 0});
            if (i % 10 == 0) { ip.Clear(); ip.Seed(42, {false,false,true}); }
        }
    });
    for (auto& t : workers) t.join();
    test(violations.load() == 0, "concurrent reload never grants unknown/GM");
    test(!ip.TryReadF14CachedNormalHumanAccount(43),
        "after cache clear, previous excluded entries cannot resurrect");
    std::cout << "F14 actual IP cache-only source method: " << checks
              << " assertions, " << fails << " failures; no Auth DB, server or client" << '\n';
    return fails == 0 ? 0 : 1;
}
"""


class ActualIPCacheOnlyReader(unittest.TestCase):
    def test_compiles_and_runs_actual_extracted_cpp_with_gcc_clang(self):
        source = program()
        with tempfile.TemporaryDirectory(prefix="f14-cache-only-") as t:
            source_path = Path(t) / "f14_read.cpp"
            source_path.write_text(source, encoding="utf-8")
            for compiler in ("g++", "clang++"):
                for standard in ("c++17", "c++20"):
                    for release in (False, True):
                        exe = Path(t) / "f14-reader"
                        cmd = [compiler, "-std=" + standard, "-O2",
                               "-Wall", "-Wextra", "-Werror", "-pedantic", "-pthread"]
                        if release:
                            cmd += ["-DNDEBUG"]
                        cmd += [str(source_path), "-o", str(exe)]
                        build = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
                        self.assertEqual(build.returncode, 0,
                            "ACTUAL extracted C++ compiler diagnostic (" + compiler + " " + standard +
                            " release=" + str(release) + "):\n" + build.stderr)
                        run = subprocess.run([str(exe)], cwd=ROOT, capture_output=True,
                                             text=True)
                        self.assertEqual(run.returncode, 0,
                            "ACTUAL extracted C++ runtime diagnostic: " + run.stderr)
                        self.assertIn("0 failures; no Auth DB", run.stdout)

    def test_original_old_ip139_uses_blocking_auth_on_cache_miss(self):
        old_cpp = subprocess.check_output(
            ["git", "show", PARENT + ":src/IndividualProgression.cpp"],
            cwd=ROOT, text=True)
        old_header = subprocess.check_output(
            ["git", "show",
             PARENT + ":src/IndividualProgressionF14Core6PositiveRndbotAdapter.h"],
            cwd=ROOT, text=True)
        self.assertIn("AccountMgr::GetName(accountId, accountName)", old_cpp)
        self.assertIn("AccountMgr::GetSecurity(accountId)", old_cpp)
        self.assertIn("sIndividualProgression->isNormalAccount(ordinaryViewer)", old_header)
        self.assertNotIn(SIGNATURE, old_cpp)
        now = CPP.read_text(encoding="utf-8")
        method = verbatim_body()
        self.assertIn("_accountMatchCache.find(accountId)", method)
        self.assertIn("std::shared_lock<std::shared_mutex>", method)
        self.assertNotIn("GetAccountMatch(", method)
        self.assertNotIn("AccountMgr", method)
        self.assertIn("TryReadF14CachedNormalHumanAccount(", now)


if __name__ == "__main__":
    unittest.main()
