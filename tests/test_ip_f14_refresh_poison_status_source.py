"""F14 map-thread refresh poison is reported for both logout and combat/assist.
Source-only checks, NOT a runtime permission or a client-restore assertion.
"""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT/"src/IndividualProgressionF14LifecycleScript.cpp").read_text()
Q = (ROOT/"src/IndividualProgressionF14LogoutRefreshQueue.h").read_text()
CONF = (ROOT/"conf/individualProgression.conf.dist").read_text()

def between(a,b):
    assert SRC.count(a)==1,(a,SRC.count(a))
    x=SRC.split(a,1)[1]
    assert b in x,b
    return x.split(b,1)[0]

class RefreshPoisonStatus(unittest.TestCase):
    def test_single_shared_logger_exposes_queue_poison_even_if_registry_healthy(self):
        body=between("void LogPoisonOnce()", "\n}\n\nvoid IndividualProgression::ResetF14LifecycleRegistry()")
        self.assertEqual(body.count("PendingLogoutRefresh().IsPoisoned()"),1)
        self.assertEqual(body.count("LogoutRefreshOverflowLogged() = true;"),1)
        self.assertEqual(SRC.count("experimental map-thread refresh queue exhausted"),1)
        self.assertLess(body.index("PendingLogoutRefresh().IsPoisoned()"),body.index("Registry::Registry const& registry = LifecycleRegistry();"))
        self.assertLess(body.index("PendingLogoutRefresh().IsPoisoned()"),body.index("if (!registry.IsPoisoned() || PoisonLogged())"))
        self.assertIn("COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT",body)

    def test_diagnostic_status_includes_independent_refresh_failure(self):
        body=between("IndividualProgression::F14LifecycleStatus IndividualProgression::GetF14LifecycleStatus()","\n}\n")
        self.assertIn("status.poisoned = registry.IsPoisoned();",body)
        self.assertIn("status.poisoned = status.poisoned || PendingLogoutRefresh().IsPoisoned();",body)
        self.assertIn("COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT",body)
        self.assertLess(body.index("status.poisoned = registry.IsPoisoned();"),body.index("status.poisoned = status.poisoned ||"))
        self.assertNotIn("PendingLogoutRefresh().Reset()",body)

    def test_combat_damage_and_heal_paths_call_same_one_shot_logger(self):
        body=between("void HandleParticipantEvent(Unit* source, Unit* target, bool mayBeAssist)","\n    }\n\n    // Combat end")
        for s in ("pending.Remember(key, stamp);","OnAssistOfActor(receiver, ActorTokenOf(source),",
                  "HasPendingLogoutRefresh().store(pending.HasPending(), std::memory_order_release);",
                  "LogPoisonOnce();"):
            self.assertIn(s,body)
        self.assertEqual(body.count("LogPoisonOnce();"),2)
        logout=between("void IndividualProgression::ExpireF14LifecycleActor(std::uint64_t loginEpoch)","\n}\n")
        self.assertIn("pending.Remember(key, stamp);",logout)
        self.assertIn("LogPoisonOnce();",logout)
        self.assertNotIn("LogoutRefreshOverflowLogged() = true;",logout)
        self.assertNotIn('LOG_ERROR(',logout)

    def test_reset_clears_poison_reporting_only_on_explicit_lifecycle_reset(self):
        reset=between("void IndividualProgression::ResetF14LifecycleRegistry()","\n}\n")
        self.assertIn("PendingLogoutRefresh().Reset();",reset)
        self.assertIn("LogoutRefreshOverflowLogged() = false;",reset)
        self.assertIn("HasPendingLogoutRefresh().store(false, std::memory_order_release);",reset)
        self.assertIn("_poisoned = true;",Q)
        self.assertIn("bool IsPoisoned() const",Q)

    def test_no_concealment_or_data_mutation(self):
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION",SRC)
        for name in ("PaidExpansion.Enable", "F14.ExperimentalAttackLifecycleRegistry"):
            self.assertIn(name+" = 0",CONF)
        for mutation in ("SetPhaseMask(", "RemoveRewardedQuest(", "WorldPacket(", "ShouldF14SuppressCreatureForViewer(", "sRandomPlayerbotMgr"):
            self.assertNotIn(mutation,SRC)
if __name__=="__main__":
    unittest.main()
