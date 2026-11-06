#!/usr/bin/env python3
"""F14 #119: pinned source audit of Map::Update vs Map::DelayedUpdate.

The map update invocation marker from #108 is ONLY a current Map::Update
frame; delayed-removal after MapUpdater::wait is a separate coordinator
phase. Neither phase by itself proves exclusive owning-map authority or
safe client prior-hidden restoration. SOURCE ONLY; F14 and paid OFF.
"""
import os
from pathlib import Path
import unittest

CORE_PATHS = {
    "manager": "src/server/game/Maps/MapMgr.cpp",
    "updater": "src/server/game/Maps/MapUpdater.cpp",
    "instance": "src/server/game/Maps/MapInstanced.cpp",
    "map": "src/server/game/Maps/Map.cpp",
    "world": "src/server/game/World/World.cpp",
    "creature": "src/server/game/Entities/Creature/Creature.cpp",
}
IP_MARKER = "tests/core_patches/apply_f14_map_update_invocation_diagnostic.py"
IP_NEGATIVE = "tests/core_patches/apply_f14_prequery_map_frame_hard_veto.py"


def exactly(s, text, label, n=1):
    count = s.count(text)
    if count != n:
        raise ValueError(f"{label}: expected {n} '{text}', observed {count}")


def between(s, start, stop, label):
    exactly(s, start, label + " start")
    exactly(s, stop, label + " end")
    body = s.split(start, 1)[1].split(stop, 1)[0]
    if not body:
        raise ValueError(f"{label}: empty exact Core span")
    return body


def audit(sources, marker, negative):
    for key in CORE_PATHS:
        if not sources.get(key):
            raise ValueError(f"missing pinned Core source {key}")

    world = sources["world"]
    mgr = between(sources["manager"], "void MapMgr::Update(uint32 diff)",
                  "void MapMgr::DoDelayedMovesAndRemoves()", "MapMgr::Update")
    request = between(sources["updater"],
                      "class MapUpdateRequest : public UpdateRequest",
                      "class MapPreloadRequest : public UpdateRequest", "MapUpdateRequest")
    worker = between(sources["updater"], "void MapUpdater::WorkerThread()",
                     "\n}\n", "MapUpdater worker")
    instance = between(sources["instance"],
                       "void MapInstanced::Update(const uint32 t, const uint32 s_diff, bool /*thread*/)",
                       "void MapInstanced::DelayedUpdate(const uint32 diff)", "MapInstanced::Update")
    child_delayed = between(sources["instance"],
                            "void MapInstanced::DelayedUpdate(const uint32 diff)",
                            "void MapInstanced::UnloadAll()", "MapInstanced::DelayedUpdate")
    current = between(sources["map"],
                      "void Map::Update(const uint32 t_diff, const uint32 s_diff, bool  /*thread*/)",
                      "void Map::UpdateNonPlayerObjects(uint32 const diff)",
                      "Map::Update")
    delayed = between(sources["map"], "void Map::DelayedUpdate(const uint32 t_diff)",
                      "void Map::AddObjectToRemoveList(WorldObject* obj)", "Map::DelayedUpdate")
    removal = between(sources["map"],
                      "void Map::RemoveAllObjectsInRemoveList()",
                      "uint32 Map::GetPlayersCountExceptGMs(", "Map removal queue")
    death = between(sources["creature"], "void Creature::RemoveCorpse(bool setSpawnTime, bool skipVisibility)",
                    "change the entry of creature until respawn", "Creature::RemoveCorpse")

    exactly(world, "sMapMgr->Update(diff);", "world event origin")
    for val in (
        "m_updater.schedule_update(*iter->second,",
        "iter->second->Update(uint32(full ? i_timer[mapUpdateStep].GetCurrent() : 0), diff);",
        "m_updater.wait();",
        "iter->second->DelayedUpdate(uint32(i_timer[mapUpdateStep].GetCurrent()));",
    ):
        exactly(mgr, val, "MapMgr root dispatch")

    # The coordinator must wait for worker requests before delayed removal
    # and must NOT assume the same worker/thread/frame for these operations.
    if not (mgr.index("m_updater.schedule_update(*iter->second,") <
            mgr.index("m_updater.wait();") <
            mgr.index("iter->second->DelayedUpdate(")):
        raise ValueError("map worker and coordinator delayed phases reordered")

    for val in ("m_map.Update(m_diff, s_diff);", "m_updater.update_finished();"):
        exactly(request, val, "per-map request")
    for val in ("_queue.WaitAndPop(request);", "request->call();"):
        exactly(worker, val, "worker dispatch")
    if not (request.index("m_map.Update(m_diff, s_diff);") <
            request.index("m_updater.update_finished();")):
        raise ValueError("pending task completed before map update")

    for val in ("Map::Update(t, s_diff, false);",
                "sMapMgr->GetMapUpdater()->schedule_update(*i->second, t, s_diff);",
                "i->second->Update(t, s_diff);"):
        exactly(instance, val, "child map update")
    if not (instance.index("Map::Update(t, s_diff, false);") <
            instance.index("schedule_update(*i->second")):
        raise ValueError("root/child scheduling semantics changed")

    for val in ("i->second->DelayedUpdate(diff);", "Map::DelayedUpdate(diff);"):
        exactly(child_delayed, val, "instance delayed cleanup")
    if child_delayed.index("i->second->DelayedUpdate(diff);") > child_delayed.index("Map::DelayedUpdate(diff);"):
        raise ValueError("child/root delayed cleanup reordered")

    exactly(current, "UpdateNonPlayerObjects(t_diff);", "non-player update")
    exactly(current, "sScriptMgr->OnMapUpdate(this, t_diff);", "map update script")
    exactly(delayed, "RemoveAllObjectsInRemoveList();", "deferred removals")
    if "RemoveAllObjectsInRemoveList();" in current:
        raise ValueError("object deletion unexpectedly moved into current Map::Update")
    exactly(removal, "RemoveFromMap(obj->ToCreature(), true);", "object unit deletion")

    for val in ("if (_respawnCompatibilityMode)", "setDeathState(DeathState::Dead);",
                "DestroyForVisiblePlayers();", "AddObjectToRemoveList();"):
        exactly(death, val, "two Core corpse modes")

    # Installed Core F14 marker is only in the disposable patch, not Core
    # upstream. The on-frame guard cannot grant delayed update authority.
    exactly(marker, "F14ScopedMapUpdateInvocation f14ScopedInvocation(this);",
            "original experimental #108 marker")
    exactly(marker, "return f14CurrentMapUpdateInvocation == this;",
            "original exact-map comparison")
    if "Map::DelayedUpdate" in marker.split(
        "F14ScopedMapUpdateInvocation f14ScopedInvocation(this);", 1)[1].split(
        "return f14CurrentMapUpdateInvocation == this;", 1)[0]:
        raise ValueError("unreviewed frame-marker scope")
    exactly(negative, "GetMap()->IsWithinF14MapUpdateInvocation() &&",
            "#110 pre-policy map-current-frame requirement")
    return True


class MapPhaseSourceAudit(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = os.environ.get("F14_PINNED_MAP_PHASE_CORE_ROOT")
        if not root:
            raise ValueError("exact pinned Core checkout REQUIRED, never skip")
        root = Path(root)
        cls.sources = {name: (root / path).read_text(encoding="utf-8")
                       for name, path in CORE_PATHS.items()}
        ip = Path(__file__).resolve().parents[1]
        cls.marker = (ip / IP_MARKER).read_text(encoding="utf-8")
        cls.negative = (ip / IP_NEGATIVE).read_text(encoding="utf-8")

    def test_exact_real_pinned_worker_vs_delayed_core_path(self):
        self.assertTrue(audit(self.sources, self.marker, self.negative))

    def test_missing_worker_barrier_rejected(self):
        changed = dict(self.sources)
        changed["manager"] = changed["manager"].replace(
            "m_updater.wait();", "m_updater.fake_wait();", 1)
        with self.assertRaises(ValueError):
            audit(changed, self.marker, self.negative)

    def test_delayed_cleanup_before_barrier_rejected(self):
        changed = dict(self.sources)
        s = changed["manager"]
        prior = "    if (m_updater.activated())\n        m_updater.wait();"
        inserted = "    if (mapUpdateStep < 3)\n        iter->second->DelayedUpdate(uint32(i_timer[mapUpdateStep].GetCurrent()));\n"
        # Do not tolerate a second premature cleanup even if original remains.
        changed["manager"] = s.replace(prior, inserted + prior, 1)
        with self.assertRaises(ValueError):
            audit(changed, self.marker, self.negative)

    def test_child_instance_queue_removed_rejected(self):
        changed = dict(self.sources)
        changed["instance"] = changed["instance"].replace(
            "sMapMgr->GetMapUpdater()->schedule_update(*i->second, t, s_diff);",
            "i->second->Update(t, s_diff);", 1)
        with self.assertRaises(ValueError):
            audit(changed, self.marker, self.negative)

    def test_child_delayed_cleanup_removed_rejected(self):
        changed = dict(self.sources)
        changed["instance"] = changed["instance"].replace(
            "i->second->DelayedUpdate(diff);",
            "i->second->NoCleanup(diff);", 1)
        with self.assertRaises(ValueError):
            audit(changed, self.marker, self.negative)

    def test_core_delayed_remove_list_changed_rejected(self):
        changed = dict(self.sources)
        changed["map"] = changed["map"].replace(
            "    RemoveAllObjectsInRemoveList();",
            "    NoCleanupHere();", 1)
        with self.assertRaises(ValueError):
            audit(changed, self.marker, self.negative)

    def test_f14_map_update_marker_missing_rejected(self):
        with self.assertRaises(ValueError):
            audit(self.sources, self.marker.replace(
                "F14ScopedMapUpdateInvocation f14ScopedInvocation(this);",
                "NoF14FrameMarker();"), self.negative)

    def test_f14_prepolicy_negative_current_frame_missing_rejected(self):
        with self.assertRaises(ValueError):
            audit(self.sources, self.marker,
                  self.negative.replace("GetMap()->IsWithinF14MapUpdateInvocation() &&",
                                        "GetMap() &&", 1))


if __name__ == "__main__":
    unittest.main()
