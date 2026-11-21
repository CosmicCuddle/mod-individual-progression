"""F14 temporary view wiring contracts; not native Worldserver proof."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
BASE = "f3d0ff64b03fb72e5baca6511fb751a3ec9837f4"


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class GroupViewWiring(unittest.TestCase):
    def test_reuses_exact_existing_floor_model_and_test(self):
        for name in ("src/IndividualProgressionF14GroupSyncEraFloor.h", "tests/f14_group_sync_era_floor_test.cpp"):
            original = subprocess.run(["git", "show", "05435925:" + name], cwd=ROOT,
                                      check=True, capture_output=True).stdout
            self.assertEqual((ROOT / name).read_bytes(), original)

    def test_own_bot_identity_is_positive_and_population_specific(self):
        source = read("src/IndividualProgressionF14GroupSyncView.cpp")
        predicate = function(source, "bool IndividualProgression::UsesF14TemporaryGroupProgression(")
        for fact in ("OptionOn()", "player->IsInWorld()", "player->GetSession()->IsHeadless()",
                     "!player->IsGameMaster()", "IsExpansionRace(player->getRace())",
                     "HasF14VerifiedRandomBotIdentity(player)"):
            self.assertIn(fact, predicate)
        self.assertNotIn("sRandomPlayerbotMgr", source)
        self.assertNotIn("Database", source)

    def test_group_sync_preserves_earned_before_existing_write(self):
        sync = function(read("src/IndividualProgression.cpp"), "void IndividualProgression::SyncBotsProgressionToLeader(")
        gate = sync.index("if (UsesF14TemporaryGroupProgression(member))")
        self.assertLess(sync.index("if (!member || isNormalAccount(member))"), gate)
        self.assertLess(gate, sync.index("ForceUpdateProgressionState(member,"))
        self.assertIn("RequestF14GroupSyncView(group);", sync)

    def test_acceptance_preservation_does_not_preempt_admission(self):
        accept = function(read("src/IndividualProgressionPlayer.cpp"), "bool OnPlayerCanGroupAccept(")
        self.assertGreater(accept.index("UsesF14TemporaryGroupProgression(player)"),
                           accept.index("if (sIndividualProgression->enforceGroupRules)"))
        self.assertLess(accept.index("UsesF14TemporaryGroupProgression(player)"),
                        accept.index("ForceUpdateProgressionState("))
        self.assertIn("OnAddMember refreshes the view", accept)

    def test_command_floor_precedes_any_quest_removal(self):
        writer = function(read("src/IndividualProgression.cpp"), "void IndividualProgression::ForceUpdateProgressionState(")
        self.assertLess(writer.index("UsesF14TemporaryGroupProgression(player)"),
                        writer.index("RemoveRewardedQuest("))
        self.assertIn("GetPlayerProgressionFromQuests(player) >= IndividualProgressionGroupPolicy::FirstTbcStage", writer)
        for signature in ("void IndividualProgression::ForceUpdateProgressionState(",
                          "void IndividualProgression::UpdateProgressionState("):
            self.assertIn("RequestF14GroupSyncView(player->GetGroup());", function(read("src/IndividualProgression.cpp"), signature))

    def test_effective_query_reads_no_foreign_player(self):
        query = function(read("src/IndividualProgressionF14GroupSyncView.cpp"),
                         "bool IndividualProgression::TryGetF14TemporaryGroupProgression(")
        self.assertIn("group->GetLeaderGUID().GetRawValue()", query)
        self.assertIn("View::UnavailableStage(earned)", query)
        self.assertIn("earned < IndividualProgressionGroupPolicy::FirstWrathStage", query)
        self.assertNotIn("ObjectAccessor", query)
        self.assertNotIn("GetGroupByGUID", query)
        self.assertIn("if (!group)", query)

    def test_lifecycle_invalidates_and_refreshes_on_world(self):
        source = read("src/IndividualProgressionF14GroupSyncView.cpp")
        for hook in ("OnAddMember", "OnRemoveMember", "OnChangeLeader", "OnDisband",
                     "OnPlayerLogin", "OnPlayerLogout", "OnF14PlayerSessionRebound",
                     "OnF14PlayerSessionDisconnected", "OnAfterConfigLoad"):
            self.assertIn(hook + "(", source)
        refresh = function(source, "void Refresh()")
        self.assertIn("COSMIC_F14_SESSION_REBIND_EXPERIMENT < 1", refresh)
        self.assertIn("COSMIC_F14_SESSION_DISCONNECT_EXPERIMENT < 1", refresh)
        self.assertIn("IsSocketClosed()", refresh)
        self.assertIn("GroupViews.Publish(item,", refresh)
        owned = function(source, "void OnPlayerBeforeUpdate(")
        self.assertIn("group->GetLeaderGUID() != player->GetGUID()", owned)
        self.assertNotIn("ObjectAccessor", owned)

    def test_source_does_not_enable_features_or_write_quests(self):
        source = read("src/IndividualProgressionF14GroupSyncView.cpp")
        for forbidden in ("RewardQuest(", "RemoveRewardedQuest(", "ForceUpdateProgressionState(",
                          "SetPhaseMask(", "SetVisibility(", "#define COSMIC_"):
            self.assertNotIn(forbidden, source)
        conf = read("conf/individualProgression.conf.dist")
        self.assertIn("IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor = 0", conf)
        self.assertIn("IndividualProgression.PaidExpansion.Enable = 0", conf)
        self.assertIn("AddSC_mod_individual_progression_f14_group_sync_view();", read("src/IndividualProgression.cpp"))

    def test_prior_floor_test_only_changes_expected_combined_login_guard(self):
        name = "tests/test_ip_f14_verified_rndbot_tbc_floor.py"
        original = subprocess.run(["git", "show", BASE + ":" + name], cwd=ROOT,
                                  check=True, capture_output=True, text=True).stdout
        current = read(name)
        self.assertEqual(current.replace(" && !preserveF14GroupEarnedStage)", ")"), original)

    def test_persistent_titles_use_earned_evidence_for_selected_grouped_bots(self):
        cleanup = function(read("src/IndividualProgression.cpp"), "void IndividualProgression::CleanUpVanillaPvpTitles(")
        self.assertIn("UsesF14TemporaryGroupProgression(player) && player->GetGroup()", cleanup)
        self.assertIn("GetPlayerProgressionFromQuests(player) >= PROGRESSION_PRE_TBC", cleanup)
        self.assertIn(": hasPassedProgression(player, PROGRESSION_PRE_TBC)", cleanup)
        self.assertIn("!VanillaPvpTitlesKeepPostVanilla && pastVanilla", cleanup)

    def test_paired_mutation_callbacks_do_not_depend_on_runtime_option(self):
        source = read("src/IndividualProgressionF14GroupSyncView.cpp")
        mutation = function(source, "static void Mutation(")
        self.assertIn("questId < 66000", mutation)
        self.assertIn("GroupViews.BeginMutation(groupGuid)", mutation)
        self.assertIn("GroupViews.EndMutation(groupGuid)", mutation)
        # Option only controls group invalidation. It must not skip one half
        # of a counter pair when config changes between the notifications.
        self.assertNotIn("!OptionOn()", mutation)
        self.assertIn("PLAYERHOOK_ON_BEFORE_REWARDED_QUEST_CHANGE", source)
        self.assertIn("PLAYERHOOK_ON_AFTER_REWARDED_QUEST_CHANGE", source)
        query = function(source, "bool IndividualProgression::TryGetF14TemporaryGroupProgression(")
        self.assertIn("COSMIC_REWARDED_QUEST_CHANGE_HOOK_VERSION >= 1", query)
        self.assertIn("canBorrowWrath = !IsPaidExpansionActive();", query)

    def test_mutation_fence_survives_reload_and_retries_publication(self):
        header = read("src/IndividualProgressionF14GroupSyncView.h")
        clear = function(header, "void Clear()")
        self.assertNotIn("_activeMutations =", clear)
        self.assertNotIn("_mutationPoisoned =", clear)
        publish = function(header, "bool Publish(")
        self.assertIn("if (_activeMutations)", publish)
        self.assertIn("_pending.insert(work.first);", publish)
        read_view = function(header, "bool Read(")
        self.assertIn("_mutationPoisoned || _activeMutations", read_view)


if __name__ == "__main__":
    unittest.main()
