"""Pinned Grim Core F14 refresh / teardown route source contracts.

Read-only source tests: not evidence of client packet ACK or an installed server.
We deliberately check REQUEST vs PROCESSED paths separately: the game engine
will have to deliver both correct nearby-viewer and owner-viewer refreshes when
F14 permission facts actually change. This suite does not enable any F14 flag.
"""
import os
import unittest
from pathlib import Path

CORE = Path(os.environ["COSMIC_F14_PINNED_CORE_ROOT"]).resolve()
SOURCES = {
    "object": "src/server/game/Entities/Object/Object.cpp",
    "unit": "src/server/game/Entities/Unit/Unit.cpp",
    "player": "src/server/game/Entities/Player/PlayerUpdates.cpp",
    "notifiers": "src/server/game/Grids/Notifiers/GridNotifiers.cpp",
    "creature": "src/server/game/Entities/Creature/Creature.cpp",
    "allcreature_h": "src/server/game/Scripting/ScriptDefines/AllCreatureScript.h",
    "allcreature_cpp": "src/server/game/Scripting/ScriptDefines/AllCreatureScript.cpp",
    "map": "src/server/game/Maps/Map.cpp",
}

def section(name, begin, end, minimum=80):
    data = (CORE / SOURCES[name]).read_text(encoding="utf-8")
    first = data.find(begin)
    if first < 0:
        raise AssertionError(f"Core source contract missing {name}: {begin}")
    finish = data.find(end, first + len(begin))
    if finish < 0 or finish - first < minimum:
        raise AssertionError(f"Core source contract missing end for {name}: {end}")
    return data[first:finish]


class CoreRouteContract(unittest.TestCase):
    def test_pinned_core_must_have_required_sources(self):
        for name, path in SOURCES.items():
            with self.subTest(name=name):
                self.assertTrue((CORE / path).is_file(), path)

    def test_non_player_forced_refresh_visits_nearby_viewers(self):
        src = section("object", "void WorldObject::UpdateObjectVisibility(", "void WorldObject::AddToNotify(")
        self.assertIn("Acore::VisibleChangesNotifier notifier(*this);", src)
        self.assertIn("Cell::VisitObjects(this, notifier, GetVisibilityRange());", src)
        self.assertLess(src.index("VisibleChangesNotifier notifier"), src.index("Cell::VisitObjects"))

    def test_unit_forced_refresh_uses_world_object_notifier(self):
        src = section("unit", "void Unit::UpdateObjectVisibility(", "void Unit::KnockbackFrom(")
        self.assertIn("if (!forced)", src)
        self.assertIn("AddToNotify(NOTIFY_VISIBILITY_CHANGED)", src)
        self.assertIn("WorldObject::UpdateObjectVisibility(true);", src)
        self.assertIn("Acore::AIRelocationNotifier notifier(*this);", src)

    def test_core_notifier_walks_nearby_and_shared_vision_players(self):
        src = section("notifiers", "void VisibleChangesNotifier::Visit(PlayerMapType& m)", "void VisibleChangesNotifier::Visit(CreatureMapType& m)")
        self.assertIn("iter->GetSource()->UpdateVisibilityOf(&i_object);", src)
        self.assertIn("HasSharedVision()", src)
        self.assertIn("(*i)->UpdateVisibilityOf(&i_object);", src)

    def test_core_notifier_includes_creature_and_dynamic_shared_vision(self):
        src = section("notifiers", "void VisibleChangesNotifier::Visit(CreatureMapType& m)", "inline void CreatureUnitRelocationWorker(")
        self.assertIn("void VisibleChangesNotifier::Visit(DynamicObjectMapType& m)", src)
        self.assertIn("GetCasterGUID().IsPlayer()", src)
        self.assertIn("UpdateVisibilityOf(&i_object)", src)

    def test_visibility_evaluation_delivers_destroy_and_create_operations(self):
        src = section("player", "void Player::UpdateVisibilityOf(WorldObject* target)", "void Player::UpdateTriggerVisibility(")
        self.assertIn("HaveAtClient(target)", src)
        self.assertEqual(src.count("CanSeeOrDetect(target, false, true)"), 2)
        for required in ("target->DestroyForPlayer(this);",
                         "GetObjectVisibilityContainer().UnlinkWorldObjectVisibility(target);",
                         "target->SendUpdateToPlayer(this);",
                         "GetObjectVisibilityContainer().LinkWorldObjectVisibility(target);",
                         "GetInitialVisiblePackets((Unit*) target);"):
            self.assertIn(required, src)
        self.assertLess(src.index("DestroyForPlayer"), src.index("SendUpdateToPlayer"))

    def test_forced_player_refresh_is_deferred_unless_from_update(self):
        src = section("player", "void Player::UpdateObjectVisibility(bool forced, bool fromUpdate)", "template <class T>")
        self.assertIn("if (!IsInWorld())", src)
        self.assertIn("if (!fromUpdate)", src)
        self.assertIn("bRequestForcedVisibilityUpdate = true;", src)
        self.assertIn("Unit::UpdateObjectVisibility(true);", src)
        self.assertIn("UpdateVisibilityForPlayer();", src)
        self.assertLess(src.index("bRequestForcedVisibilityUpdate = true;"), src.index("Unit::UpdateObjectVisibility(true);"))

    def test_player_update_processes_request_only_if_not_teleporting(self):
        src = section("player", "void Player::Update(uint32 p_time)", "void Player::UpdateMirrorTimers()")
        self.assertIn("!IsBeingTeleported() && bRequestForcedVisibilityUpdate", src)
        self.assertIn("bRequestForcedVisibilityUpdate = false;", src)
        self.assertIn("UpdateObjectVisibility(true, true);", src)
        self.assertLess(src.index("bRequestForcedVisibilityUpdate = false;"), src.index("UpdateObjectVisibility(true, true);"))

    def test_player_own_view_uses_full_visibility_rescan(self):
        src = section("player", "void Player::UpdateVisibilityForPlayer(bool mapChange)", "void Player::UpdateObjectVisibility(")
        self.assertIn("Acore::VisibleNotifier notifier(*this, mapChange);", src)
        self.assertIn("Cell::VisitObjects(", src)
        self.assertIn("Cell::VisitFarVisibleObjects(", src)
        self.assertIn("notifier.SendToSelf();", src)

    def test_creature_removal_hook_runs_before_world_and_map_reset(self):
        src = section("creature", "void Creature::RemoveFromWorld()", "void Creature::DisappearAndDie()")
        self.assertIn("sScriptMgr->OnCreatureRemoveWorld(this);", src)
        self.assertIn("Unit::RemoveFromWorld();", src)
        self.assertIn("GetMap()->GetObjectsStore().Remove<Creature>(GetGUID());", src)
        self.assertLess(src.index("OnCreatureRemoveWorld(this)"), src.index("Unit::RemoveFromWorld();"))
        self.assertLess(src.index("OnCreatureRemoveWorld(this)"), src.index("GetObjectsStore().Remove<Creature>"))

    def test_existing_allcreature_hook_is_dispatched(self):
        h = (CORE / SOURCES["allcreature_h"]).read_text(encoding="utf-8")
        self.assertIn("virtual void OnCreatureRemoveWorld(Creature* /*creature*/) { }", h)
        src = section("allcreature_cpp", "void ScriptMgr::OnCreatureRemoveWorld(Creature* creature)", "void ScriptMgr::OnCreatureSaveToDB(")
        self.assertIn("ExecuteScript<AllCreatureScript>", src)
        self.assertIn("script->OnCreatureRemoveWorld(creature);", src)

    def test_map_removal_starts_with_remove_from_world(self):
        src = section("map", "void Map::RemoveFromMap(T* obj, bool remove)", "template<>")
        self.assertIn("obj->RemoveFromWorld();", src)
        self.assertIn("obj->RemoveFromGrid();", src)
        self.assertIn("obj->ResetMap();", src)
        self.assertLess(src.index("obj->RemoveFromWorld();"), src.index("obj->ResetMap();"))


if __name__ == "__main__":
    unittest.main()
