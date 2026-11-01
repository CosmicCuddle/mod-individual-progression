"""Source contracts for the default-OFF paid expansion entitlement (IP-11).

These check how the pure policy (IndividualProgressionExpansionPolicy.h,
tested by tests/expansion_purchase_policy.cpp) is wired into the real IP
source. They are source checks, not gameplay tests.

Run: python3 -B -m unittest discover -s tests -p 'test_ip_paid_expansion_source.py'
"""
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT_20 = "5fe0f9105d55d249aa55a25510d3fa23e3d84148"


def read(path: str) -> str:
    return (ROOT / path).read_text()


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, check=True).stdout


def git_show(rev: str, path: str) -> str:
    return git("show", f"{rev}:{path}")


def body(src: str, signature: str) -> str:
    """Function text from ``signature`` through its matching closing brace."""
    i = src.index(signature)
    j = src.index("{", i)
    depth = 0
    for k in range(j, len(src)):
        depth += src[k] == "{"
        depth -= src[k] == "}"
        if depth == 0:
            return src[i:k + 1]
    raise AssertionError(signature)


def code(text: str) -> str:
    return re.sub(r"//[^\n]*", "", text)


CORE = read("src/IndividualProgression.cpp")
HEADER = read("src/IndividualProgression.h")
PLAYER = read("src/IndividualProgressionPlayer.cpp")
EXP = read("src/IndividualProgressionExpansion.cpp")
POLICY = read("src/IndividualProgressionExpansionPolicy.h")
CONF = read("conf/individualProgression.conf.dist")


class Provenance(unittest.TestCase):
    def test_parent_is_pr20_head(self):
        subprocess.run(["git", "-C", str(ROOT), "merge-base", "--is-ancestor", PARENT_20, "HEAD"], check=True)

    def test_protected_files_unchanged(self):
        for path in ("src/IndividualProgressionSpells.cpp", "src/cs_individualProgression.cpp",
                     "src/IndividualProgressionGroupPolicy.h", "src/IndividualProgression_loader.cpp",
                     "src/IndividualProgressionAwareness.cpp", "src/IndividualProgressionPvP.cpp",
                     "src/IndividualProgressionBG.cpp"):
            with self.subTest(path=path):
                self.assertEqual(git("diff", "--name-only", PARENT_20, "--", path).strip(), "")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionSpells.cpp").strip(),
                         "6ee93e1174b5aef6c5d61c9ea31526506c9d08da")
        self.assertEqual(git("rev-parse", "HEAD:src/cs_individualProgression.cpp").strip(),
                         "654119d29e0519600d50b179ccd5a9c5df8695b0")

    def test_no_sql_or_data_or_script_dir_change(self):
        self.assertEqual(git("diff", "--name-only", PARENT_20, "--", "data", "optional", "sql",
                             "src/naxx40Scripts", "src/tbcScripts", "src/vanillaScripts",
                             "src/wotlkScripts", "src/spellScripts").strip(), "")
        added = git("diff", "--name-only", "--diff-filter=A", PARENT_20).split()
        self.assertFalse([p for p in added if p.lower().endswith(".sql")], added)


class DefaultOff(unittest.TestCase):
    def test_config_defaults_off(self):
        self.assertIn('GetOption<bool>("IndividualProgression.PaidExpansion.Enable", false)', CORE)
        self.assertIn('GetOption<bool>("IndividualProgression.PaidExpansion.GrandfatherStage13", false)', CORE)
        self.assertRegex(CONF, r"\nIndividualProgression\.PaidExpansion\.Enable = 0\n")
        self.assertRegex(CONF, r"\nIndividualProgression\.PaidExpansion\.GrandfatherStage13 = 0\n")
        self.assertIn("bool paidExpansionEnabled = false;", HEADER)
        self.assertIn("bool paidExpansionGrandfatherStage13 = false;", HEADER)

    def test_active_is_startup_snapshot_and_purchase_needs_live_store(self):
        active = code(body(EXP, "bool IndividualProgression::IsPaidExpansionActive() const"))
        self.assertIn("return enabled && paidExpansionEnabled;", active)
        self.assertNotIn("CONFIG_PLAYER_SETTINGS_ENABLED", active)
        can = code(body(EXP, "PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"))
        self.assertIn("if (IsPaidExpansionActive() && !StoreWritesAllowed(true, PaidExpansionStoreSaving()))\n        return PurchaseResult::FeatureInactive;", can)
        self.assertLess(can.index("PaidExpansionStoreSaving()"), can.index("return CheckPurchase("))
        self.assertIn("return sWorld->getBoolConfig(CONFIG_PLAYER_SETTINGS_ENABLED);", body(EXP, "static bool PaidExpansionStoreSaving()"))

    def test_forced_off_only_at_startup_never_on_reload(self):
        after = body(CORE, "void OnAfterConfigLoad(bool reload) override")
        self.assertIn("bool const requested = sIndividualProgression->paidExpansionEnabled;", after)
        self.assertIn("IndividualProgressionExpansionPolicy::FeatureEnabledAfterConfigLoad(reload, requested, storeSaving);", after)
        self.assertNotIn("paidExpansionEnabled = false;", after)
        self.assertIn("if (!reload)\n                LOG_ERROR", after)
        self.assertIn("return reload ? requested : (requested && storeSaving);", POLICY)

    def test_store_writes_need_saving_store(self):
        self.assertIn("return featureActive && storeSaving;", POLICY)
        gf = code(body(EXP, "void IndividualProgression::EvaluatePaidExpansionGrandfather(Player* player)"))
        self.assertIn("if (!player || !StoreWritesAllowed(IsPaidExpansionActive(), PaidExpansionStoreSaving()))\n        return;", gf)
        can = code(body(EXP, "PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"))
        self.assertIn("StoreWritesAllowed(true, PaidExpansionStoreSaving())", can)

    def test_switch_is_startup_only(self):
        before = body(CORE, "void OnBeforeConfigLoad(bool reload) override")
        self.assertLess(before.index("bool const paidExpansionAtStartup"), before.index("LoadConfig();"))
        self.assertIn("sIndividualProgression->paidExpansionEnabled = paidExpansionAtStartup;", before)
        self.assertIn("sIndividualProgression->paidExpansionGrandfatherStage13 = grandfatherAtStartup;", before)
        self.assertLess(before.index("bool const grandfatherAtStartup"), before.index("LoadConfig();"))

    def test_effective_stage_is_raw_when_off(self):
        eff = code(body(EXP, "uint8 IndividualProgression::GetEffectiveProgression(Player* player)"))
        self.assertIn("uint8 const raw = GetPlayerProgressionFromQuests(player);", eff)
        self.assertIn("if (raw < WrathAccessStage || !IsPaidExpansionActive())\n        return raw;", eff)
        grant = code(body(EXP, "bool IndividualProgression::IsAutomaticGrantAllowed(Player* player, uint8 targetStage)"))
        self.assertIn("if (targetStage < TbcEntryStage || !IsPaidExpansionActive())\n        return true;", grant)


class AccessWiring(unittest.TestCase):
    def test_has_passed_and_is_before_use_effective_stage(self):
        has_passed = code(body(CORE, "bool IndividualProgression::hasPassedProgression(Player* player, ProgressionState state) const"))
        self.assertIn("return sIndividualProgression->GetEffectiveProgression(player) >= state;", has_passed)
        self.assertNotIn("GetPlayerProgressionFromQuests", has_passed)
        before = code(body(CORE, "bool IndividualProgression::isBeforeProgression(Player* player, ProgressionState state)"))
        self.assertIn("return sIndividualProgression->GetEffectiveProgression(player) < state;", before)
        # The guard lines above the return are unchanged from the parent.
        parent = git_show(PARENT_20, "src/IndividualProgression.cpp")
        old = code(body(parent, "bool IndividualProgression::hasPassedProgression(Player* player, ProgressionState state) const"))
        self.assertEqual(has_passed.split("return sIndividualProgression")[0].strip(),
                         old.split("return sIndividualProgression")[0].strip())

    def test_raw_stage_reader_unchanged(self):
        sig = "uint8 IndividualProgression::GetPlayerProgressionFromQuests(Player* player) const"
        self.assertEqual(body(CORE, sig), body(git_show(PARENT_20, "src/IndividualProgression.cpp"), sig))

    def test_update_progression_refuses_unpaid_era(self):
        upd = code(body(CORE, "void IndividualProgression::UpdateProgressionState(Player* player, ProgressionState newState) const"))
        gate = upd.index("if (!sIndividualProgression->IsAutomaticGrantAllowed(player, newState))\n        return;")
        self.assertLess(upd.index("if (progressionLimit && newState > progressionLimit)"), gate)
        self.assertLess(gate, upd.index("uint8 currentState = GetPlayerProgressionFromQuests(player);"))

    def test_force_update_is_unchanged(self):
        sig = "void IndividualProgression::ForceUpdateProgressionState(Player* player, ProgressionState newState)"
        self.assertEqual(body(CORE, sig), body(git_show(PARENT_20, "src/IndividualProgression.cpp"), sig))

    def test_group_on_checks_use_effective_stage_off_sync_keeps_raw(self):
        invite = code(body(PLAYER, "bool OnPlayerCanGroupInvite("))
        self.assertEqual(invite.count("GetEffectiveProgression("), 3)
        self.assertNotIn("GetPlayerProgressionFromQuests(", invite)
        accept = code(body(PLAYER, "bool OnPlayerCanGroupAccept("))
        self.assertIn("uint8 const joiningState = sIndividualProgression->GetPlayerProgressionFromQuests(player);", accept)
        self.assertIn("uint8 const leaderState = sIndividualProgression->GetPlayerProgressionFromQuests(leader);", accept)
        on_start = accept.index("if (sIndividualProgression->enforceGroupRules)")
        on_end = accept.index("return true;", on_start)
        on_block = accept[on_start:on_end]
        self.assertEqual(on_block.count("GetEffectiveProgression("), 3)
        self.assertNotIn("joiningState", on_block)
        self.assertNotIn("leaderState", on_block)
        off_part = accept[on_end:]
        self.assertIn("if (!sIndividualProgression->isNormalAccount(player) && joiningState != leaderState)", off_part)
        self.assertIn("static_cast<ProgressionState>(leaderState)", off_part)
        sync = code(body(CORE, "void IndividualProgression::SyncBotsProgressionToLeader(Group* group)"))
        self.assertIn("uint8 refProgress = enforceGroupRules ? GetEffectiveProgression(leader) : GetPlayerProgressionFromQuests(leader);", sync)

    def test_only_documented_player_cpp_changes(self):
        removed = [l[1:] for l in git("diff", "-U0", PARENT_20, "--", "src/IndividualProgressionPlayer.cpp").splitlines()
                   if l.startswith("-") and not l.startswith("---")]
        for line in removed:
            self.assertRegex(line, r"GetPlayerProgressionFromQuests\(|GetAccountProgression\(accountId\)|joiningState, player->getRace\(\), (leaderState|memberState)", line)
        self.assertEqual(len(removed), 8, removed)

    def test_account_unlocks_use_effective_account_ceiling(self):
        self.assertEqual(PLAYER.count("sIndividualProgression->GetAccountEffectiveProgression(accountId)"), 2)
        self.assertNotIn("sIndividualProgression->GetAccountProgression(", PLAYER)
        acct = code(body(EXP, "uint8 IndividualProgression::GetAccountEffectiveProgression(uint32 accountId)"))
        self.assertIn("if (!sIndividualProgression->IsPaidExpansionActive())\n        return GetAccountProgression(accountId);", acct)
        self.assertIn("EffectiveAccountStage(raw, SettingsSlotSet(data, SettingWrath))", acct)
        self.assertIn("LEFT JOIN character_settings s ON s.guid = c.guid AND s.source = '{}'", acct)
        # The raw account reader stays unchanged for every other caller.
        sig = "uint8 IndividualProgression::GetAccountProgression(uint32 accountId)"
        self.assertEqual(body(CORE, sig), body(git_show(PARENT_20, "src/IndividualProgression.cpp"), sig))


class PurchaseSequence(unittest.TestCase):
    PURCHASE = code(body(EXP, "PurchaseResult IndividualProgression::PurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"))

    def test_validate_before_money_moves(self):
        p = self.PURCHASE
        check = p.index("CanPurchaseExpansion(player, eraValue, costCopper)")
        bail = p.index("if (check != PurchaseResult::Success)\n        return check;")
        debit = p.index("player->ModifyMoney(-static_cast<int32>(costCopper));")
        self.assertLess(check, bail)
        self.assertLess(bail, debit)

    def test_flag_after_debit_then_grant_then_verify_refund_save(self):
        p = self.PURCHASE
        debit = p.index("player->ModifyMoney(-static_cast<int32>(costCopper));")
        flag = p.index("SetPaidExpansionFlag(player, index, FlagPaid);")
        grant = p.index("UpdateProgressionState(player, static_cast<ProgressionState>(grant));")
        verify = p.index("if (!opened)")
        undo = p.index("SetPaidExpansionFlag(player, index, FlagNone);")
        refund = p.index("player->ModifyMoney(static_cast<int32>(costCopper));")
        save = p.index("player->SaveToDB(false, false);")
        self.assertTrue(debit < flag < grant < verify < undo < refund < save)

    def test_no_direct_stage_writes_or_wrath_tier_skip(self):
        self.assertNotIn("ForceUpdateProgressionState", code(EXP))
        self.assertNotIn("66014", EXP)
        self.assertNotIn("PROGRESSION_WOTLK_TIER_1", code(EXP))
        self.assertIn("return era == Era::Tbc ? TbcEntryStage : 0;", POLICY)

    def test_can_purchase_rejects_bad_input_and_checks_marker_template(self):
        can = code(body(EXP, "PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"))
        self.assertIn("costCopper > static_cast<uint32>(std::numeric_limits<int32>::max())", can)
        self.assertIn("return PurchaseResult::InvalidRequest;", can)
        self.assertIn("sObjectMgr->GetQuestTemplate(66000 + TbcEntryStage) != nullptr", can)
        self.assertIn("return CheckPurchase(facts, eraValue, player->HasEnoughMoney(costCopper), entryAvailable);", can)


class StorageAndThreads(unittest.TestCase):
    def test_hot_path_reads_only_ip_cache(self):
        facts = code(body(EXP, "CharacterFacts IndividualProgression::GetExpansionFacts(Player* player)"))
        self.assertNotIn("GetPlayerSetting", facts)
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_paidExpansionLock);", facts)

    def test_settings_store_used_only_on_login_thread_or_purchase(self):
        users = {name for name in re.findall(r"\n(?:void|bool|uint8|PurchaseResult|CharacterFacts) IndividualProgression::(\w+)\(", EXP)
                 if "GetPlayerSetting" in body(EXP, f"IndividualProgression::{name}(")}
        self.assertEqual(users, {"LoadPaidExpansionEntitlement", "EvaluatePaidExpansionGrandfather"})

    def test_cache_filled_before_map_add_and_kept_across_logout(self):
        script = body(EXP, "class IndividualProgression_PaidExpansionPlayerScript")
        self.assertIn("{ PLAYERHOOK_ON_SEND_INITIAL_PACKETS_BEFORE_ADD_TO_MAP, PLAYERHOOK_ON_LOGIN }", script)
        self.assertIn("sIndividualProgression->LoadPaidExpansionEntitlement(player);",
                      body(script, "void OnPlayerSendInitialPacketsBeforeAddToMap("))
        self.assertNotIn("OnPlayerLogout", code(script))
        self.assertNotIn("ForgetPaidExpansionEntitlement", EXP + HEADER)

    def test_cache_holds_purchase_bits_only(self):
        bit = code(body(EXP, "uint8 BitForSetting(uint32 index)"))
        self.assertNotIn("SettingGrandfatherEvaluated", bit)
        setter = code(body(EXP, "void IndividualProgression::SetPaidExpansionFlag(Player* player, uint32 index, uint32 value)"))
        self.assertLess(setter.index("if (!bit)\n        return;"), setter.index("std::unique_lock"))

    def test_grandfather_evaluates_every_gated_character_once(self):
        gf = code(body(EXP, "void IndividualProgression::EvaluatePaidExpansionGrandfather(Player* player)"))
        self.assertNotIn("!paidExpansionGrandfatherStage13 ||", gf)
        self.assertIn("ShouldGrandfatherWrath(facts, paidExpansionGrandfatherStage13, evaluated)", gf)
        self.assertIn("if (!evaluated)\n        SetPaidExpansionFlag(player, SettingGrandfatherEvaluated, 1);", gf)

    def test_registered_without_loader_change(self):
        add = body(CORE, "void AddSC_mod_individual_progression()")
        self.assertIn("AddSC_ip_paid_expansion();", add)
        self.assertIn("void AddSC_ip_paid_expansion();\n", CORE)


class PolicyHeader(unittest.TestCase):
    def test_pure_header(self):
        includes = re.findall(r'#include [<"]([^>"]+)[>"]', POLICY)
        self.assertEqual(includes, ["cstddef", "cstdint", "string_view"])

    def test_stage_constants_match_ip_enum(self):
        for name, value in (("PROGRESSION_NAXX40", 7), ("PROGRESSION_PRE_TBC", 8),
                            ("PROGRESSION_TBC_TIER_4", 12), ("PROGRESSION_TBC_TIER_5", 13),
                            ("PROGRESSION_WOTLK_TIER_1", 14), ("PROGRESSION_WOTLK_TIER_5", 18)):
            self.assertRegex(HEADER, rf"\n\s*{name}\s*=\s*{value},?")
        for name, value in (("VanillaFinalStage", 7), ("TbcEntryStage", 8), ("UnpaidWrathAccessCap", 12),
                            ("TbcFinalStage", 13), ("WrathAccessStage", 13), ("WrathFirstCompletionStage", 14),
                            ("LastSupportedStage", 18)):
            self.assertIn(f"constexpr std::uint8_t {name} = {value};", POLICY)

    def test_api_version_macro_for_dependent_modules(self):
        self.assertIn("#define COSMIC_IP_PAID_EXPANSION_API_VERSION 1\n", HEADER)
        self.assertLess(HEADER.index('#include "IndividualProgressionExpansionPolicy.h"'),
                        HEADER.index("#define COSMIC_IP_PAID_EXPANSION_API_VERSION"))

    def test_f14_header_line_untouched(self):
        self.assertIn("enforceGroupRules, f14PreserveVerifiedRandomBotStageOnLogin, EnableSetRepCommand", HEADER)


class WorkflowDeviation(unittest.TestCase):
    WF = ".github/workflows/ip19-gm-null-safety-integration-20261101.yml"

    def test_gm_workflow_only_scopes_its_frozen_step(self):
        diff = git("diff", "-U0", PARENT_20, "--", self.WF)
        added = [l[1:] for l in diff.splitlines() if l.startswith("+") and not l.startswith("+++")]
        removed = [l for l in diff.splitlines() if l.startswith("-") and not l.startswith("---")]
        self.assertEqual(removed, [])
        self.assertEqual(len(added), 5, added)
        src = read(self.WF)
        step = src.split("      - name: Exact frozen PR19 parent, six paths, append-only register\n", 1)[1]
        step = step.split("        shell: bash", 1)[0]
        self.assertIn("github.head_ref == 'integration/ip19-gm-null-safety-20261101'", step)
        self.assertIn("github.ref == 'refs/heads/integration/ip19-gm-null-safety-20261101'", step)
        self.assertIn("python3 -B -m unittest discover -s tests -p 'test_ip_gm_command_null_safety.py' -v", src)


if __name__ == "__main__":
    unittest.main()
