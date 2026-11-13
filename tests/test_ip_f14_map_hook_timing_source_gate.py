#!/usr/bin/env python3
"""Fail-visible pinned Core timing contract for F14's optional AllMapScript observer.

A successful source check proves timing/absence, not an exclusive map owner,
registered runtime hook invocation, an F14 hide, a client ACK or restoration.
"""
import os
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "src/IndividualProgressionF14LifecycleScript.cpp"
MAP_CPP = "src/server/game/Maps/Map.cpp"
UPDATE_SIGNATURE = "void Map::Update(const uint32 t_diff, const uint32 s_diff, bool  /*thread*/)"
ZERO_DIFF_EXIT = """    if (!t_diff)
    {
        HandleDelayedVisibility();
        return;
    }"""
CALLBACK = "sScriptMgr->OnMapUpdate(this, t_diff);"


def exactly_once(text, token):
    if text.count(token) != 1:
        raise ValueError(f"expected one pinned token {token!r}, got {text.count(token)}")


def between(text, start, end):
    exactly_once(text, start)
    tail = text.split(start, 1)[1]
    if end not in tail:
        raise ValueError(f"missing pinned end boundary {end!r}")
    return tail.split(end, 1)[0]


def audit(core, module):
    body = between(core, UPDATE_SIGNATURE, "void Map::UpdateNonPlayerObjects(uint32 const diff)")
    delayed = between(core, "void Map::DelayedUpdate(const uint32 t_diff)",
                      "void Map::AddObjectToRemoveList(WorldObject* obj)")

    # The hook is skipped by the zero-t_diff branch even though that branch
    # processes HandleDelayedVisibility. Do not treat one map update as a
    # guaranteed late restore/recheck opportunity.
    exactly_once(body, "Events.Update(t_diff);")
    exactly_once(body, ZERO_DIFF_EXIT)
    exactly_once(body, CALLBACK)
    if not body.index("Events.Update(t_diff);") < body.index(ZERO_DIFF_EXIT):
        raise ValueError("zero-diff exit moved before normal events")
    if not body.index(ZERO_DIFF_EXIT) < body.index("UpdateNonPlayerObjects(t_diff);"):
        raise ValueError("zero-diff early return no longer precedes object updates")

    # A callback after update flush and delayed visibility cannot retroactively
    # guarantee an immediate CREATE, corpse targetability, or quest loot.
    steps = ("UpdateNonPlayerObjects(t_diff);", "SendObjectUpdates();",
             "HandleDelayedVisibility();", "UpdateExpiredCorpses(t_diff);", CALLBACK)
    positions = []
    offset = body.index(ZERO_DIFF_EXIT) + len(ZERO_DIFF_EXIT)
    for token in steps:
        pos = body.find(token, offset)
        if pos < 0:
            raise ValueError(f"missing or reordered pinned post-zero map phase {token!r}")
        positions.append(pos)
        offset = pos + len(token)
    if positions != sorted(positions):
        raise ValueError("Map::Update callback not after visibility/corpse phases")

    exactly_once(delayed, "RemoveAllObjectsInRemoveList();")
    if CALLBACK in delayed:
        raise ValueError("later coordinator delayed removal must not inherit map hook proof")
    if "F14ScopedMapUpdateInvocation" in delayed:
        raise ValueError("delayed removal was incorrectly made a frame-owner proof")

    # Original #120 observer remains read-only and compile-optional.
    guard = ("#if defined(COSMIC_IP_F14_MAP_UPDATE_FRAME_OBSERVER) "
             "&& COSMIC_IP_F14_MAP_UPDATE_FRAME_OBSERVER >= 1")
    if module.count(guard) != 3:
        raise ValueError("optional observer include/class/registration guards drifted")
    cls = between(module,
                  "class IndividualProgressionF14MapUpdateFrameObserver final : public AllMapScript",
                  "\n};\n#endif")
    for token in ("void OnMapUpdate(Map* map, uint32 /*diff*/) override",
                  "if (!map || !map->IsWithinF14MapUpdateInvocation())"):
        exactly_once(cls, token)
    for forbidden in ("UpdateObjectVisibility", "SendDirectMessage", "CompleteRestore",
                      "CopyExactPendingRestores", "ClearFrozen", "GetSession()",
                      "GetPlayers", "UpdateVisibilityOf", "ShouldF14Suppress"):
        if forbidden in cls:
            raise ValueError(f"observer gained forbidden action: {forbidden}")
    return True


class MapHookTimingSourceGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = os.environ.get("F14_PINNED_CORE_ROOT")
        if not path:
            raise ValueError("explicit pinned Core checkout REQUIRED; no skipped audit")
        cls.core = (Path(path) / MAP_CPP).read_text()
        cls.module = MODULE.read_text()

    def test_exact_source_contract(self):
        self.assertTrue(audit(self.core, self.module))

    def test_zero_diff_return_is_required(self):
        bad = self.core.replace(ZERO_DIFF_EXIT, ZERO_DIFF_EXIT.replace("        return;\n", ""), 1)
        self.assertNotEqual(bad, self.core)
        with self.assertRaises(ValueError):
            audit(bad, self.module)

    def test_early_visibility_path_not_a_hook(self):
        bad = self.core.replace(ZERO_DIFF_EXIT, ZERO_DIFF_EXIT.replace("HandleDelayedVisibility();", CALLBACK), 1)
        self.assertNotEqual(bad, self.core)
        with self.assertRaises(ValueError):
            audit(bad, self.module)

    def test_hook_before_packet_flush_fails(self):
        body = between(self.core, UPDATE_SIGNATURE, "void Map::UpdateNonPlayerObjects(uint32 const diff)")
        bad_body = body.replace(CALLBACK, "", 1).replace(
            "SendObjectUpdates();", CALLBACK + "\n    SendObjectUpdates();", 1)
        self.assertNotEqual(bad_body, body)
        with self.assertRaises(ValueError):
            audit(self.core.replace(body, bad_body, 1), self.module)

    def test_hook_before_corpse_phase_fails(self):
        body = between(self.core, UPDATE_SIGNATURE, "void Map::UpdateNonPlayerObjects(uint32 const diff)")
        bad_body = body.replace(CALLBACK, "", 1).replace(
            "UpdateExpiredCorpses(t_diff);", CALLBACK + "\n    UpdateExpiredCorpses(t_diff);", 1)
        self.assertNotEqual(bad_body, body)
        with self.assertRaises(ValueError):
            audit(self.core.replace(body, bad_body, 1), self.module)

    def test_missing_post_update_delayed_visibility_fails(self):
        body = between(self.core, UPDATE_SIGNATURE, "void Map::UpdateNonPlayerObjects(uint32 const diff)")
        p = body.index("SendObjectUpdates();")
        bad_body = body[:p] + body[p:].replace("HandleDelayedVisibility();", "SkippedDelayedVisibility();", 1)
        self.assertNotEqual(bad_body, body)
        with self.assertRaises(ValueError):
            audit(self.core.replace(body, bad_body, 1), self.module)

    def test_missing_delayed_removal_fails(self):
        bad = self.core.replace("    RemoveAllObjectsInRemoveList();\n}", "    NoDelayedRemoval();\n}", 1)
        self.assertNotEqual(bad, self.core)
        with self.assertRaises(ValueError):
            audit(bad, self.module)

    def test_no_callback_in_later_delayed_update(self):
        bad = self.core.replace("    RemoveAllObjectsInRemoveList();\n}",
                                "    RemoveAllObjectsInRemoveList();\n    " + CALLBACK + "\n}", 1)
        self.assertNotEqual(bad, self.core)
        with self.assertRaises(ValueError):
            audit(bad, self.module)

    def test_observer_cannot_gain_visibility_action(self):
        bad = self.module.replace(
            "void OnMapUpdate(Map* map, uint32 /*diff*/) override\n    {",
            "void OnMapUpdate(Map* map, uint32 /*diff*/) override\n    {\n        map->UpdateObjectVisibility(true);", 1)
        self.assertNotEqual(bad, self.module)
        with self.assertRaises(ValueError):
            audit(self.core, bad)


if __name__ == "__main__":
    unittest.main()
