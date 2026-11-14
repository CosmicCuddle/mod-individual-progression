"""F14 one-shot bot-logout map-thread request, source safety checks only."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "src/IndividualProgressionF14LifecycleScript.cpp").read_text()
REGISTRY = (ROOT / "src/IndividualProgressionF14LifecycleRegistry.h").read_text()
QUEUE = (ROOT / "src/IndividualProgressionF14LogoutRefreshQueue.h").read_text()


def part(s, start, stop):
    assert s.count(start) == 1, (start, s.count(start))
    return s.split(start, 1)[1].split(stop, 1)[0]


class ValueOnlyLogout(unittest.TestCase):
    def test_registry_reports_key_and_stamp_before_invalidation(self):
        fn = part(REGISTRY, "std::size_t ExpireActor(std::uint64_t actorToken,", "\n    private:")
        self.assertEqual(fn.count("onAffected(pair.second.key, pair.second.token);"), 2)
        self.assertLess(fn.index("onAffected(pair.second.key, pair.second.token);"),
                        fn.index("Apply(pair.second, Policy::Event::ActorIncarnationExpired, actorToken);"))
        self.assertIn("std::function<void(CreatureKey const&, std::uint64_t)>", fn)

    def test_queue_is_pure_bounded_values_and_poison_on_overflow(self):
        for token in ("std::array<Request, Capacity>", "std::uint64_t stamp = 0;",
                      "Registry::CreatureKey key", "_poisoned = true;", "_count = 0;",
                      "bool Take(", "void Discard(", "void Reset()"):
            self.assertIn(token, QUEUE)
        self.assertNotIn("Unit*", QUEUE.split("namespace IndividualProgressionF14LogoutRefreshQueue", 1)[1])
        self.assertNotIn("sRandomPlayerbotMgr", QUEUE)
        self.assertNotIn("UpdateObjectVisibility", QUEUE.split("namespace IndividualProgressionF14LogoutRefreshQueue", 1)[1])

    def test_logout_only_enqueues_copied_values(self):
        fn = part(SCRIPT, "void IndividualProgression::ExpireF14LifecycleActor(std::uint64_t loginEpoch)", "\n}\n")
        for token in ("std::lock_guard<std::mutex> guard(LifecycleLock());",
                      "Registry::CreatureKey const& key, std::uint64_t stamp",
                      "pending.Remember(key, stamp)", "HasPendingLogoutRefresh().store(",
                      "std::memory_order_release"):
            self.assertIn(token, fn)
        for forbidden in ("UpdateObjectVisibility(", "ShouldF14RefreshCreatureVisibility(", "Creature* ", "FindMap()"):
            self.assertNotIn(forbidden, fn)

    def test_update_maps_and_releases_lock_before_refresh(self):
        method = part(SCRIPT, "void OnUnitUpdate(Unit* unit, uint32 /*diff*/) override", "\n    }")
        for token in ("HasPendingLogoutRefresh().load(std::memory_order_acquire)",
                      "Creature* creature = AsWorldCreature(unit);",
                      "pending.Take(KeyOf(creature), StampOf(creature));",
                      "MarkPendingRefresh(creature);",
                      "sScriptMgr->ShouldF14RefreshCreatureVisibility(creature)",
                      "creature->UpdateObjectVisibility(true);"):
            self.assertIn(token, method)
        lock_begin = method.index("std::lock_guard<std::mutex> guard(LifecycleLock());")
        lock_end = method.index("\n        }", lock_begin)
        self.assertLess(lock_end, method.index("sScriptMgr->ShouldF14RefreshCreatureVisibility(creature)"))
        self.assertLess(lock_end, method.index("creature->UpdateObjectVisibility(true);"))
        self.assertNotIn("sRandomPlayerbotMgr", method)

    def test_default_off_and_removal_clears_without_refresh(self):
        self.assertIn("#if defined(COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT)", SCRIPT)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", SCRIPT)
        self.assertEqual(SCRIPT.count("creature->UpdateObjectVisibility(true);"), 1)
        self.assertIn("UNITHOOK_ON_UNIT_UPDATE,", SCRIPT)
        remover = part(SCRIPT, "void OnCreatureRemoveWorld(Creature* creature) override", "\n    }")
        self.assertIn("PendingLogoutRefresh().Discard(KeyOf(creature));", remover)
        self.assertLess(remover.index("PendingLogoutRefresh().Discard(KeyOf(creature));"),
                        remover.index("if (!stamp)"))
        reset = part(SCRIPT, "void IndividualProgression::ResetF14LifecycleRegistry()", "\n}")
        self.assertIn("PendingLogoutRefresh().Reset();", reset)
        self.assertIn("HasPendingLogoutRefresh().store(false,", reset)

if __name__ == "__main__":
    unittest.main()
