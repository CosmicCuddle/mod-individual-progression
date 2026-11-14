"""F14 threat/assist invalidation -> owning-map queue, never hot-path hide."""
from pathlib import Path
import unittest
R=Path(__file__).resolve().parents[1]
S=(R/"src/IndividualProgressionF14LifecycleScript.cpp").read_text()
H=(R/"src/IndividualProgressionF14LifecycleRegistry.h").read_text()
Q=(R/"src/IndividualProgressionF14LogoutRefreshQueue.h").read_text()
def part(src,start,stop):
    assert src.count(start)==1,(start,src.count(start))
    return src.split(start,1)[1].split(stop,1)[0]
class Safety(unittest.TestCase):
    def test_direct_damage_and_dot_enqueue_but_never_send(self):
        m=part(S,"void HandleParticipantEvent(Unit* source, Unit* target, bool mayBeAssist)","\n    }\n\n")
        for s in ("bool evidenceChanged = registry.OnParticipantEvidence(key, stamp, sourceToken);",
                  "if (!evidenceChanged && registry.NeedsSnapshot(key, stamp))",
                  "if (evidenceChanged)", "pending.Remember(key, stamp);",
                  "HasPendingLogoutRefresh().store(pending.HasPending(), std::memory_order_release)"):
            self.assertIn(s,m)
        self.assertLess(m.index("std::lock_guard<std::mutex> guard(LifecycleLock());"),m.index("pending.Remember(key, stamp);"))
        self.assertIn("if (!StampOf(tracked))",m)
        for forbidden in ("UpdateObjectVisibility(", "ShouldF14RefreshCreatureVisibility(", "SetPhaseMask(", "sRandomPlayerbotMgr"):
            self.assertNotIn(forbidden,m)
    def test_assist_fanout_keeps_plain_values_and_old_abi(self):
        m=part(H,"std::size_t OnAssistOfActor(std::uint64_t receiverToken,","\n        }\n")
        self.assertIn("std::function<void(CreatureKey const&, std::uint64_t)> onAffected = {}",m)
        self.assertLess(m.index("onAffected(pair.second.key, pair.second.token);"),m.index("Apply(pair.second,"))
        self.assertIn("if (helperToken != 0 && Rostered(pair.second.state, helperToken))",m)
        s=part(S,"void HandleParticipantEvent(Unit* source, Unit* target, bool mayBeAssist)","\n    }\n\n")
        self.assertIn("OnAssistOfActor(receiver, ActorTokenOf(source),",s)
        self.assertIn("pending.Remember(key, stamp);",s)
        self.assertIn("OnAssistOfActor(receiver, ActorTokenOf(source));",s)
    def test_map_thread_sole_visibility_update(self):
        m=part(S,"void OnUnitUpdate(Unit* unit, uint32 /*diff*/) override","\n    }")
        self.assertIn("pending.Take(KeyOf(creature), StampOf(creature));",m)
        self.assertIn("creature->UpdateObjectVisibility(true);",m)
        self.assertLess(m.index("\n        }"),m.index("sScriptMgr->ShouldF14RefreshCreatureVisibility(creature)"))
        self.assertEqual(S.count("creature->UpdateObjectVisibility(true);"),1)
        self.assertIn("_poisoned = true;",Q)
    def test_no_owner_host_changes(self):
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION",S)
        for f in ("RemoveRewardedQuest(", "SetPhaseMask(", "WorldPacket(", "GrantAttunements("):
            self.assertNotIn(f,S)
if __name__=="__main__":
    unittest.main()
