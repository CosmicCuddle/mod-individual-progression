#!/usr/bin/env python3
"""F14 disposable Core Map::Update invocation evidence: source-only tests.

These tests do NOT authorize concealment or prove exclusive map ownership.
"""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PATCHER = ROOT / "tests/core_patches/apply_f14_map_update_invocation_diagnostic.py"
MAP_H = "src/server/game/Maps/Map.h"
MAP_CPP = "src/server/game/Maps/Map.cpp"

def apply(root):
    return subprocess.run(
        [sys.executable, str(PATCHER), str(root)],
        capture_output=True, text=True, check=False
    )

class F14MapUpdateInvocationSourceTests(unittest.TestCase):
    def make_fixture(self, *, bad_cpp=False):
        temp = tempfile.TemporaryDirectory()
        root = Path(temp.name)
        for file in (MAP_H, MAP_CPP):
            (root / file).parent.mkdir(parents=True, exist_ok=True)
        (root / MAP_H).write_text(
            "class Map {\npublic:\n"
            "    virtual void Update(const uint32, const uint32, bool thread = true);\n"
            "};\n", encoding="utf-8"
        )
        (root / MAP_CPP).write_text(
            "void Map::Update(const uint32 t_diff, const uint32 s_diff, bool  /*thread*/)\n"
            + ("// missing start brace\n" if bad_cpp else "{\n    // update body\n}\n"),
            encoding="utf-8"
        )
        return temp, root

    def test_two_file_source_contract_and_scoped_raii(self):
        temp, root = self.make_fixture()
        with temp:
            result = apply(root)
            self.assertEqual(result.returncode, 0, result.stderr)
            h = (root / MAP_H).read_text()
            c = (root / MAP_CPP).read_text()
            self.assertIn("IsWithinF14MapUpdateInvocation() const", h)
            self.assertIn("thread_local Map const* f14CurrentMapUpdateInvocation = nullptr", c)
            self.assertIn("f14CurrentMapUpdateInvocation = map;", c)
            self.assertIn("f14CurrentMapUpdateInvocation = previous_;", c)
            self.assertIn("F14ScopedMapUpdateInvocation f14ScopedInvocation(this);", c)
            self.assertIn("return f14CurrentMapUpdateInvocation == this;", c)
            self.assertNotIn("SetPhaseMask", c)
            self.assertNotIn("SendDirectMessage", c)

    def test_repeated_patch_refused_without_change(self):
        temp, root = self.make_fixture()
        with temp:
            self.assertEqual(apply(root).returncode, 0)
            before = [(root / p).read_bytes() for p in (MAP_H, MAP_CPP)]
            result = apply(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(before, [(root / p).read_bytes() for p in (MAP_H, MAP_CPP)])

    def test_broken_second_anchor_prevents_any_write(self):
        temp, root = self.make_fixture(bad_cpp=True)
        with temp:
            before = [(root / p).read_bytes() for p in (MAP_H, MAP_CPP)]
            result = apply(root)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(before, [(root / p).read_bytes() for p in (MAP_H, MAP_CPP)])

    def test_live_ip_observer_diagnostic_is_off_and_only_rejects(self):
        source = (ROOT / "src/IndividualProgressionF14LifecycleScript.cpp").read_text()
        self.assertIn("COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT", source)
        self.assertIn("IsWithinF14MapUpdateInvocation()", source)
        self.assertIn("if (facts.sameLiveCoreMapPointer &&", source)
        self.assertIn("not within current Map::Update invocation; concealment OFF", source)
        self.assertIn("void OnF14BeforeCreatureSuppression", source)
        self.assertIn("void OnF14CreatureSuppressionDecision", source)
        self.assertIn("return false;", source)
        self.assertNotIn("ObserveProposedBatchFlush(", source)
        self.assertNotIn("ClearPreviouslyHidden(", source)

    @unittest.skipUnless(os.environ.get("F14_PINNED_CORE_ROOT"), "pinned Core not supplied")
    def test_exact_pinned_grim_core_map_sources_accept_patch(self):
        pinned = Path(os.environ["F14_PINNED_CORE_ROOT"]).resolve()
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            for name in (MAP_H, MAP_CPP):
                target = root / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(pinned / name, target)
            self.assertEqual(apply(root).returncode, 0)
            c = (root / MAP_CPP).read_text(encoding="utf-8")
            self.assertIn("F14ScopedMapUpdateInvocation f14ScopedInvocation(this);", c)
            self.assertEqual(c.count("bool Map::IsWithinF14MapUpdateInvocation() const"), 1)
            self.assertEqual(c.count("thread_local Map const* f14CurrentMapUpdateInvocation"), 1)
            self.assertIn("bool Map::AddPlayerToMap(Player* player)", c)
            self.assertIn("player->UpdateObjectVisibility(false);", c)

if __name__ == "__main__":
    unittest.main()
