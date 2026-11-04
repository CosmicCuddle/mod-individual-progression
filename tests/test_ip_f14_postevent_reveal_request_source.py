"""F14 post-event one-shot refresh request: exact source contracts, NO gameplay proof."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "src" / "IndividualProgressionF14LifecycleScript.cpp"
MACRO = "COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT"


def source():
    return SCRIPT.read_text(encoding="utf-8")


def section(text, begin, end):
    assert text.count(begin) == 1, begin
    result = text.split(begin, 1)[1]
    assert end in result, end
    return result.split(end, 1)[0]


class PostEventRefreshRequest(unittest.TestCase):
    def test_compile_opt_in_only_not_release_ready_macro(self):
        s = source()
        self.assertIn("#if defined(" + MACRO + ") && " + MACRO + " >= 1", s)
        self.assertIn("UNITHOOK_ON_F14_CREATURE_VISIBILITY_REFRESH_REQUEST,", s)
        self.assertIn("bool OnF14ShouldRefreshCreatureVisibility(Creature const* creature) override", s)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", s)
        self.assertNotIn("COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT = 1", s)
        # The new per-map UnitUpdate handoff may request one Core refresh.
        # It remains forbidden inside the lifecycle callback under mutex.
        self.assertEqual(s.count("creature->UpdateObjectVisibility(true);"), 1)
        self.assertNotIn("CanSeeOrDetect(", s)

    def test_stamped_value_survives_retirement(self):
        s = source()
        f = section(s, "struct F14CreatureIncarnation : public DataMap::Base", "    };")
        for name in ("std::uint64_t token = 0;", "bool everTracked = false;",
                     "bool refreshPending = false;"):
            self.assertIn(name, f)
        f = section(s, "void SetStamp(Creature* creature, std::uint64_t token)", "\n    }")
        self.assertIn("state->token = token;", f)
        self.assertIn("if (token)", f)
        self.assertIn("state->everTracked = true;", f)
        self.assertIn("state->refreshPending = true;", f)
        self.assertNotIn("state->refreshPending = false", f)

    def test_source_events_queue_before_ending_or_retiring(self):
        s = source()
        old = section(s, "        if (oldCreature)\n", "        if (newCreature)")
        self.assertLess(old.index("MarkPendingRefresh(oldCreature);"),
                        old.index("registry.OnParticipantEnd("))
        self.assertIn("MarkPendingRefresh(newCreature);", s)
        own = section(s, "        if (selfCreature)\n", "        // A world creature")
        self.assertLess(own.index("MarkPendingRefresh(selfCreature);"),
                        own.index("OnCreatureStoppedAttacking("))
        end = section(s, "void RetireOnCombatEnd(Unit* unit)", "\n    }")
        self.assertLess(end.index("MarkPendingRefresh(creature);"),
                        end.index("OnCreatureLeftCombat("))
        corpse = section(s, "static void RetireIncarnation(Creature* creature, bool respawn)", "\n    }")
        self.assertLess(corpse.index("MarkPendingRefresh(creature);"),
                        corpse.index("if (!stamp)"))
        self.assertLess(corpse.index("MarkPendingRefresh(creature);"),
                        corpse.index("SetStamp(creature, 0);"))
        self.assertIn("OnF14CreatureCorpseReady(Creature* creature)", s)
        self.assertIn("OnF14CreatureRespawned(Creature* creature)", s)

    def test_callback_consumes_once_and_never_authorises_hiding(self):
        s = source()
        method = section(s, "bool OnF14ShouldRefreshCreatureVisibility(Creature const* creature)", "\n    }")
        for expression in ("!creature", "!creature->IsInWorld()", "creature->IsPet()",
                           "std::lock_guard<std::mutex> guard(LifecycleLock());",
                           "state->everTracked", "state->refreshPending",
                           "state->refreshPending = false;", "return true;"):
            self.assertIn(expression, method)
        self.assertLess(method.index("state->refreshPending = false;"),
                        method.index("return true;"))
        for forbidden in ("UpdateObjectVisibility(", "CanSeeOrDetect(", "sRandomPlayerbotMgr",
                          "SetPhaseMask(", "RemoveRewardedQuest(", "RewardQuest(", "WorldPacket"):
            self.assertNotIn(forbidden, method)

    def test_stays_dark_without_a_preexisting_tracked_creature(self):
        s = source()
        helper = section(s, "void MarkPendingRefresh(Creature* creature)", "\n    }")
        self.assertIn("CustomData.Get<F14CreatureIncarnation>(StampKey)", helper)
        self.assertIn("if (state->everTracked)", helper)
        self.assertNotIn("GetDefault", helper)  # no allocating an unknown target
        hook = section(s, "bool OnF14ShouldRefreshCreatureVisibility(Creature const* creature)", "\n    }")
        self.assertIn("if (!state || !state->everTracked || !state->refreshPending)", hook)


if __name__ == "__main__":
    unittest.main()
