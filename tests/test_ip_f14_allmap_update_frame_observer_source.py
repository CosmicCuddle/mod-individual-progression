#!/usr/bin/env python3
"""Source contracts for opt-in IP AllMapScript Map::Update frame observer.

Pinned Grim Core hook: optional diagnostic only; feature/callback never
uses any Player/Creature, network, prior-hidden ledger or positive policy.
"""
import os
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
MOD = ROOT / "src/IndividualProgressionF14LifecycleScript.cpp"
SCRIPTS_H = "src/server/game/Scripting/ScriptDefines/AllMapScript.h"
SCRIPTS_CPP = "src/server/game/Scripting/ScriptDefines/AllMapScript.cpp"
MAP_CPP = "src/server/game/Maps/Map.cpp"


def one(s, needle):
    n = s.count(needle)
    if n != 1:
        raise ValueError(f"expected exactly once: {needle!r}, observed {n}")


def audit(module, definitions, implementation, core_map):
    one(definitions, "ALLMAPHOOK_ON_MAP_UPDATE,")
    one(definitions, "virtual void OnMapUpdate(Map* /*map*/, uint32 /*diff*/) { }")
    one(implementation, "void ScriptMgr::OnMapUpdate(Map* map, uint32 diff)")
    on = implementation.split("void ScriptMgr::OnMapUpdate(Map* map, uint32 diff)", 1)[1]
    on = on.split("\n}\n", 1)[0]
    one(on, "CALL_ENABLED_HOOKS(AllMapScript, ALLMAPHOOK_ON_MAP_UPDATE, script->OnMapUpdate(map, diff));")
    one(core_map, "sScriptMgr->OnMapUpdate(this, t_diff);")
    map_update = core_map.split(
        "void Map::Update(const uint32 t_diff, const uint32 s_diff, bool  /*thread*/)", 1)[1]
    map_update = map_update.split("void Map::UpdateNonPlayerObjects(uint32 const diff)", 1)[0]
    one(map_update, "sScriptMgr->OnMapUpdate(this, t_diff);")

    guard = "#if defined(COSMIC_IP_F14_MAP_UPDATE_FRAME_OBSERVER) && COSMIC_IP_F14_MAP_UPDATE_FRAME_OBSERVER >= 1"
    if module.count(guard) != 3:
        raise ValueError("observer include, class, and registration need matching opt-in macro")
    one(module, 'class IndividualProgressionF14MapUpdateFrameObserver final : public AllMapScript')
    observer = module.split(
        'class IndividualProgressionF14MapUpdateFrameObserver final : public AllMapScript', 1)[1]
    observer = observer.split('\n};\n#endif', 1)[0]
    for token in (
        'AllMapScript("IndividualProgression_F14MapUpdateFrameObserver",',
        '{ALLMAPHOOK_ON_MAP_UPDATE})',
        'void OnMapUpdate(Map* map, uint32 /*diff*/) override',
        'if (!map || !map->IsWithinF14MapUpdateInvocation())',
        'std::atomic<std::uint64_t> _observed{0};',
        'std::atomic<std::uint64_t> _rejected{0};',
    ):
        one(observer, token)

    unsafe = ("UpdateObjectVisibility", "SendDirectMessage", "ShouldF14Suppress",
              "CopyExactPendingRestores", "GetPlayers", "WorldSession", "Creature*",
              "Player*", "CompleteRestore", "ClearFrozen", "CreatePacket",
              "UpdateVisibilityOf", "GetSession()", "GetMap()", "LogPlayer")
    for token in unsafe:
        if token in observer:
            raise ValueError(f"observer must not use runtime data or actions: {token}")

    registration = module.split("void AddSC_mod_individual_progression_f14_lifecycle()", 1)[1]
    one(registration, 'new IndividualProgressionF14MapUpdateFrameObserver();')
    if registration.index(guard) > registration.index('new IndividualProgressionF14MapUpdateFrameObserver();'):
        raise ValueError("registration unguarded")

    # Map script APIs and assembly source assertions imply a callback can
    # be invoked when enabled, but do not establish a client/client ACK,
    # exclusive map ownership, or even a Worldserver process having run.
    return True


class MapUpdateFrameObserver(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        core_root = os.environ.get("F14_PINNED_CORE_ROOT")
        if not core_root:
            raise ValueError("must provide pinned Core reference; never skip")
        root = Path(core_root)
        cls.mod = MOD.read_text()
        cls.h = (root / SCRIPTS_H).read_text()
        cls.cpp = (root / SCRIPTS_CPP).read_text()
        cls.mapcpp = (root / MAP_CPP).read_text()

    def test_optin_observer_uses_only_existing_core_allmap_hook(self):
        self.assertTrue(audit(self.mod, self.h, self.cpp, self.mapcpp))

    def test_missing_core_dispatch_fail_closed(self):
        bad = self.cpp.replace(
            "CALL_ENABLED_HOOKS(AllMapScript, ALLMAPHOOK_ON_MAP_UPDATE, script->OnMapUpdate(map, diff));",
            "NO_ALL_MAP_CALLBACK();", 1)
        with self.assertRaises(ValueError):
            audit(self.mod, self.h, bad, self.mapcpp)

    def test_registration_without_guard_fails(self):
        bad = self.mod.replace(
            "    new IndividualProgressionF14MapUpdateFrameObserver();",
            "    new IndividualProgressionF14MapUpdateFrameObserver();", 1)
        # Explicitly mutate registration by deleting its scoped opt-in.
        pre, post = bad.split("void AddSC_mod_individual_progression_f14_lifecycle()", 1)
        post = post.replace(
            "#if defined(COSMIC_IP_F14_MAP_UPDATE_FRAME_OBSERVER) && COSMIC_IP_F14_MAP_UPDATE_FRAME_OBSERVER >= 1\n",
            "", 1)
        with self.assertRaises(ValueError):
            audit(pre + "void AddSC_mod_individual_progression_f14_lifecycle()" + post,
                  self.h, self.cpp, self.mapcpp)

    def test_observer_must_never_force_client_visibility(self):
        bad = self.mod.replace(
            "void OnMapUpdate(Map* map, uint32 /*diff*/) override\n    {",
            "void OnMapUpdate(Map* map, uint32 /*diff*/) override\n    {\n        map->UpdateObjectVisibility(true);", 1)
        with self.assertRaises(ValueError):
            audit(bad, self.h, self.cpp, self.mapcpp)

    def test_wrong_map_frame_fails(self):
        bad = self.mod.replace(
            "if (!map || !map->IsWithinF14MapUpdateInvocation())",
            "if (!map)", 1)
        with self.assertRaises(ValueError):
            audit(bad, self.h, self.cpp, self.mapcpp)

    def test_missing_f14_observer_registration_fails(self):
        bad = self.mod.replace("    new IndividualProgressionF14MapUpdateFrameObserver();",
                               "    NoMapObserver();", 1)
        with self.assertRaises(ValueError):
            audit(bad, self.h, self.cpp, self.mapcpp)


if __name__ == "__main__":
    unittest.main()
