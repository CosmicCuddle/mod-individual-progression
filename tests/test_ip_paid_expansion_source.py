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
        # CS01 command extension is an explicitly reviewed exception:
        # it has its OWN exact Git blob assertion below and must not also be
        # required byte-identical to the pre-extension #20 parent.
        for path in ("src/IndividualProgressionSpells.cpp",
                     "src/IndividualProgressionGroupPolicy.h", "src/IndividualProgression_loader.cpp",
                     "src/IndividualProgressionAwareness.cpp", "src/IndividualProgressionPvP.cpp",
                     "src/IndividualProgressionBG.cpp"):
            with self.subTest(path=path):
                self.assertEqual(git("diff", "--name-only", PARENT_20, "--", path).strip(), "")
        self.assertEqual(git("rev-parse", "HEAD:src/IndividualProgressionSpells.cpp").strip(),
                         "6ee93e1174b5aef6c5d61c9ea31526506c9d08da")
        self.assertEqual(git("rev-parse", "HEAD:src/cs_individualProgression.cpp").strip(),
                         "278aaa48dc5fb7770bd6013e659e4872752d4b03")

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

    def test_start_and_unlock_warnings_are_log_only(self):
        after = body(CORE, "void OnAfterConfigLoad(bool reload) override")
        block = after[after.index("if (!reload && sIndividualProgression->paidExpansionEnabled)"):]
        block = block[:block.index("\n        }\n") + 10]
        self.assertEqual(block.count("LOG_WARN("), 6)
        for name in ("deathKnightStartingProgression", "tbcRacesStartingProgression", "startingProgression", "tbcRacesProgressionLevel"):
            self.assertIn(name, block)
        self.assertNotRegex(code(block), r"(ip|sIndividualProgression)->\w+\s*=(?!=)", "warnings must not change any setting")
        self.assertIn("auto const* ip = sIndividualProgression;", block)
        # A starting stage alone is not a bypass if account creation requires
        # the matching purchased era. Preserve both conditions in the warning.
        self.assertIn("ip->deathKnightStartingProgression >= IndividualProgressionExpansionPolicy::WrathAccessStage &&", block)
        self.assertIn("ip->deathKnightProgressionLevel == 0 ||", block)
        self.assertIn("ip->deathKnightProgressionLevel < IndividualProgressionExpansionPolicy::WrathAccessStage", block)
        self.assertIn("ip->tbcRacesStartingProgression >= IndividualProgressionExpansionPolicy::TbcEntryStage &&", block)
        self.assertIn("ip->tbcRacesProgressionLevel == 0 ||", block)
        self.assertIn("ip->tbcRacesProgressionLevel < IndividualProgressionExpansionPolicy::TbcEntryStage", block)
        # No warning should tell the operator that the default 13/13 DK
        # or fully gated 8/8 race pair itself grants free access.
        self.assertIn("with DeathKnightUnlockProgression = {}", block)
        self.assertIn("with TbcRacesUnlockProgression = {}", block)
        # Both starting race/class paths must check *both* era thresholds,
        # not only their most typical expansion.
        self.assertIn("ip->deathKnightStartingProgression >= IndividualProgressionExpansionPolicy::TbcEntryStage &&", block)
        self.assertIn("ip->deathKnightProgressionLevel < IndividualProgressionExpansionPolicy::TbcEntryStage", block)
        self.assertIn("ip->tbcRacesStartingProgression >= IndividualProgressionExpansionPolicy::WrathAccessStage &&", block)
        self.assertIn("ip->tbcRacesProgressionLevel < IndividualProgressionExpansionPolicy::WrathAccessStage", block)
        self.assertIn("may give TBC without a purchase", block)
        self.assertIn("may give Wrath without a purchase", block)

    def test_cpp_warning_guards_match_exact_cross_era_predicates(self):
        # Unlike the synthetic matrix below, validate the whole C++ condition:
        # both operands, parentheses, matching tier, and LOG_WARN as its body.
        after = body(CORE, "void OnAfterConfigLoad(bool reload) override")
        block = after[after.index("if (!reload && sIndividualProgression->paidExpansionEnabled)"):]
        block = block[:block.index("\n        }\n") + 10]
        shape = re.compile(
            r'if\s*\(\s*ip->(?P<start>deathKnightStartingProgression|tbcRacesStartingProgression)'
            r'\s*>=\s*IndividualProgressionExpansionPolicy::(?P<era>TbcEntryStage|WrathAccessStage)'
            r'\s*&&\s*\(\s*ip->(?P<unlock>deathKnightProgressionLevel|tbcRacesProgressionLevel)'
            r'\s*==\s*0\s*\|\|\s*ip->(?P=unlock)\s*<\s*'
            r'IndividualProgressionExpansionPolicy::(?P=era)\s*\)\s*\)\s*'
            r'LOG_WARN\("module",\s*"[^"\n]*?may give (?P<message>TBC|Wrath) without a purchase",'
        )
        matched = {(m.group("start"), m.group("era"), m.group("unlock"))
                   for m in shape.finditer(block)}
        expected = {
            ("deathKnightStartingProgression", "TbcEntryStage", "deathKnightProgressionLevel"),
            ("deathKnightStartingProgression", "WrathAccessStage", "deathKnightProgressionLevel"),
            ("tbcRacesStartingProgression", "TbcEntryStage", "tbcRacesProgressionLevel"),
            ("tbcRacesStartingProgression", "WrathAccessStage", "tbcRacesProgressionLevel"),
        }
        self.assertEqual(matched, expected)
        # Prevent duplicate matches from being hidden by the set comparison.
        self.assertEqual(len(list(shape.finditer(block))), 4)
        # The era named in the human-readable operator warning must also
        # match the tier threshold, not just exist somewhere in the log block.
        # Otherwise a copy/paste swap can mislead a GM about a configuration.
        expected_era_label = {"TbcEntryStage": "TBC", "WrathAccessStage": "Wrath"}
        for m in shape.finditer(block):
            with self.subTest(origin=m.group("start"), era=m.group("era")):
                self.assertEqual(m.group("message"), expected_era_label[m.group("era")])

    def test_warning_cross_era_start_threshold_matrix(self):
        # Synthetic truth-table model of the four warning conditions.
        # This does not execute the AzerothCore C++ start/creation path.
        def warns(start, unlock, target):
            return start >= target and (unlock == 0 or unlock < target)

        cases = (
            ("default DK", 13, 13, False, False),
            ("DK safe TBC", 8, 8, False, False),
            ("DK free TBC", 8, 7, True, False),
            ("DK free Wrath", 13, 12, False, True),
            ("DK no unlock", 13, 0, True, True),
            ("DK both eras below unlock", 13, 7, True, True),
            ("safe race TBC", 8, 8, False, False),
            ("race free TBC", 8, 7, True, False),
            ("race free Wrath", 13, 12, False, True),
            ("race no unlock", 13, 0, True, True),
            ("safe race Wrath", 13, 13, False, False),
        )
        for name, start, unlock, tbc, wrath in cases:
            with self.subTest(case=name):
                self.assertEqual((warns(start, unlock, 8), warns(start, unlock, 13)), (tbc, wrath))

    def test_far_teleport_cannot_accept_purchase_before_delayed_save(self):
        can = code(body(EXP, "PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"))
        self.assertIn("player->IsBeingTeleportedFar()", can)
        self.assertLess(can.index("player->IsBeingTeleportedFar()"),
                        can.index("CharacterFacts const facts = GetExpansionFacts(player);"))
        # Pinned Core SaveToDB delays, rather than commits, if teleporting far.
        # This pre-debit refusal is an extra safeguard, not live transaction proof.

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
        # F19 is a separately reviewed, deliberate login-only extension.
        # Retain the exact four removed F14 lines as a fixed allowlist, then
        # enforce all eight existing paid-era group/account-read changes.
        f19_removed = {
            "savedStage <= IndividualProgressionGroupPolicy::LastSupportedStage)",
            "// may not yet exist during Core's PlayerScript login.",
            "// Do not call the mutable currentBots-backed predicate",
            "// outside the source-verified WorldScript login thread.",
        }
        self.assertEqual(len([l for l in removed if l.strip() in f19_removed]), 4)
        paid_removed = [l for l in removed if l.strip() not in f19_removed]
        for line in paid_removed:
            self.assertRegex(line, r"GetPlayerProgressionFromQuests\(|GetAccountProgression\(accountId\)|joiningState, player->getRace\(\), (leaderState|memberState)", line)
        self.assertEqual(len(paid_removed), 8, paid_removed)

    def test_account_unlocks_use_effective_account_ceiling(self):
        self.assertEqual(PLAYER.count("sIndividualProgression->GetAccountEffectiveProgression(accountId)"), 2)
        self.assertNotIn("sIndividualProgression->GetAccountProgression(", PLAYER)
        acct = code(body(EXP, "uint8 IndividualProgression::GetAccountEffectiveProgression(uint32 accountId)"))
        self.assertIn("if (!sIndividualProgression->IsPaidExpansionActive())\n        return GetAccountProgression(accountId);", acct)
        self.assertIn("ReceiptPolicy::DecodeRaw(data, !fields[1].IsNull())", acct)
        self.assertIn("EffectiveAccountStage(raw, verifiedWrath)", acct)
        self.assertNotIn("SettingsSlotSet(data, SettingWrath)", acct)
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

    def test_flag_after_debit_then_verify_refund_save_without_quest_grant(self):
        p = self.PURCHASE
        debit = p.index("player->ModifyMoney(-static_cast<int32>(costCopper));")
        flag = p.index("SetPaidExpansionFlag(player, index, FlagPaid);")
        verify = p.index("if (!opened)")
        undo = p.index("SetPaidExpansionFlag(player, index, FlagNone);")
        refund = p.index("player->ModifyMoney(static_cast<int32>(costCopper));")
        save = p.index("player->SaveToDB(false, false);")
        self.assertTrue(debit < flag < verify < undo < refund < save)
        # Payment records entitlement but does not itself award stage8 or 10259.
        self.assertNotIn("UpdateProgressionState(", p)
        self.assertIn("GetPlayerProgressionFromQuests(player) == rawBeforePurchase", p)
        self.assertIn("GetExpansionFacts(player).tbcPaid", p)
        self.assertIn("GetEffectiveProgression(player) >= WrathAccessStage", p)

    def test_no_direct_stage_writes_or_wrath_tier_skip(self):
        self.assertNotIn("ForceUpdateProgressionState", code(EXP))
        self.assertNotIn("66014", EXP)
        self.assertNotIn("PROGRESSION_WOTLK_TIER_1", code(EXP))
        self.assertIn("constexpr std::uint8_t PurchaseGrantStage(Era)", POLICY)
        self.assertIn("return 0;", code(body(POLICY, "constexpr std::uint8_t PurchaseGrantStage(Era)")))
        self.assertIn("f.rawStage < TbcEntryStage && f.startStage < TbcEntryStage", POLICY)
        self.assertIn("VanillaFinalStage : VanillaFinalStage - 1;", POLICY)

    def test_unpaid_into_the_breach_completion_veto_is_hook_registered(self):
        script = code(EXP)
        self.assertIn("PLAYERHOOK_ON_BEFORE_QUEST_COMPLETE", script)
        method = code(body(EXP, "bool OnPlayerBeforeQuestComplete(Player* player, uint32 questId) override"))
        self.assertIn("questId != INTO_THE_BREACH", method)
        self.assertIn("!sIndividualProgression->IsPaidExpansionActive()", method)
        self.assertIn("!sIndividualProgression->isNormalAccount(player)", method)
        self.assertIn("sIndividualProgression->HasExpansionEntitlement(player, Era::Tbc)", method)
        self.assertIn("return true;", method)
        self.assertIn("return false;", method)
        self.assertIn("Purchase The Burning Crusade", method)
        # The new already-paid/DPP no-strand veto intentionally returns
        # false BEFORE the historical unpaid gate. Preserve its *original*
        # OFF/bot/GM/entitled fast-exit ordering within that gate, and require
        # the additional veto to be limited to quest 10259.
        self.assertIn("MustHoldPaidTbcQuestReward(player)", method)
        self.assertLess(method.index("MustHoldPaidTbcQuestReward(player)"),
                        method.index("if (questId != INTO_THE_BREACH || !player ||"))
        historical = method[method.index("if (questId != INTO_THE_BREACH || !player ||"):]
        self.assertLess(historical.index("return true;"), historical.index("return false;"))

    def test_can_purchase_rejects_bad_input_and_checks_marker_template(self):
        can = code(body(EXP, "PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"))
        self.assertIn("costCopper > static_cast<uint32>(std::numeric_limits<int32>::max())", can)
        self.assertIn("return PurchaseResult::InvalidRequest;", can)
        self.assertIn("sObjectMgr->GetQuestTemplate(66000 + TbcEntryStage) != nullptr", can)
        self.assertIn("sObjectMgr->GetQuestTemplate(INTO_THE_BREACH) != nullptr", can)
        self.assertIn("player->GetQuestStatus(INTO_THE_BREACH) == QUEST_STATUS_REWARDED", can)
        self.assertIn("entryAvailable = false;", can)
        self.assertIn("&&", can)
        self.assertLess(can.index("GetQuestTemplate(INTO_THE_BREACH)"), can.index("GetQuestStatus(INTO_THE_BREACH)"))
        self.assertIn("return CheckPurchase(facts, eraValue, player->HasEnoughMoney(costCopper), entryAvailable);", can)


class PurchaseReminderSafety(unittest.TestCase):
    def test_pre_rewarded_10259_warns_instead_of_advertising_unusable_sale(self):
        reminder = code(body(EXP, "void IndividualProgression::SendPaidExpansionReminder(Player* player)"))
        self.assertIn("if (!ShouldRemind(facts, era))", reminder)
        self.assertIn("era == Era::Tbc && facts.rawStage < TbcEntryStage", reminder)
        self.assertIn("player->GetQuestStatus(INTO_THE_BREACH) == QUEST_STATUS_REWARDED", reminder)
        self.assertIn("Contact server staff to review TBC passage. No purchase was processed.", reminder)
        self.assertLess(reminder.index("No purchase was processed."), reminder.index("Visit {} to purchase access."))
        # A generic continue earlier in ShouldRemind is insufficient. The
        # specific no-sale warning must exit BEFORE the Vezrath invitation.
        warning_to_invite = reminder[
            reminder.index("No purchase was processed."):
            reminder.index("Visit {} to purchase access.")
        ]
        self.assertRegex(warning_to_invite, r"\bcontinue\s*;")
        # The exception applies only to TBC, not Wrath, OFF or a paid player.
        self.assertNotIn("UpdateProgressionState(", reminder)
        self.assertNotIn("ModifyMoney(", reminder)


class SavedCompleteQuestSafety(unittest.TestCase):
    """Source contract for login-only fail-closed deferral, NOT a gameplay test."""

    def test_only_unpaid_normal_complete_10259_is_deferred_on_login(self):
        login = code(body(EXP, "void OnPlayerLogin(Player* player) override"))
        # Pin the *whole* guard; losing just one restriction must fail this test.
        guard = " ".join(login.split())
        required = (
            "if (player && sIndividualProgression->IsPaidExpansionActive() && "
            "sIndividualProgression->isNormalAccount(player) && "
            "!sIndividualProgression->HasExpansionEntitlement(player, Era::Tbc) && "
            "player->GetQuestStatus(INTO_THE_BREACH) == QUEST_STATUS_COMPLETE)"
        )
        self.assertIn(required, guard)
        self.assertEqual(login.count("player->IncompleteQuest(INTO_THE_BREACH);"), 1)
        self.assertLess(login.index("EvaluatePaidExpansionGrandfather(player);"),
                        login.index("player->IncompleteQuest(INTO_THE_BREACH);"))
        self.assertLess(login.index("player->IncompleteQuest(INTO_THE_BREACH);"),
                        login.index("SendPaidExpansionReminder(player);"))

    def test_only_restores_status_and_keeps_items_gold_quest_objectives(self):
        login = code(body(EXP, "void OnPlayerLogin(Player* player) override"))
        self.assertIn("player->IncompleteQuest(INTO_THE_BREACH);", login)
        self.assertIn("if (player->GetSession())", login)
        self.assertIn("without losing objective progress", login)
        self.assertIn('LOG_INFO("module",', login)
        for forbidden in ("RemoveQuest(", "RemoveActiveQuest(", "RemoveRewardedQuest(",
                          "SetRewardedQuest(", "SetQuestStatus(", "SetQuestSlot(",
                          "SetQuestSlotCounter(", "CreatureOrGOCount", "ItemCount",
                          "AbandonQuest(", "QUEST_STATUS_REWARDED",
                          "ForceUpdateProgressionState(", "UpdateProgressionState(",
                          "ModifyMoney(", "SaveToDB(", "IsGameMaster()", "return;"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, login)

    def test_exact_login_body_permits_no_extra_calls_or_changed_guard_scope(self):
        # Unlike a substring check, this pins the *whole executable login body*.
        # It catches a deferral moved outside its if, early return, GM bypass,
        # REWARDED reset, quest-objective wipe, or a dead-coded guard.
        expected = """void OnPlayerLogin(Player* player) override
    {
        sIndividualProgression->EvaluatePaidExpansionGrandfather(player);

        
        
        
        
        
        if (player && sIndividualProgression->IsPaidExpansionActive() &&
            sIndividualProgression->isNormalAccount(player) &&
            !sIndividualProgression->HasExpansionEntitlement(player, Era::Tbc) &&
            player->GetQuestStatus(INTO_THE_BREACH) == QUEST_STATUS_COMPLETE)
        {
            player->IncompleteQuest(INTO_THE_BREACH);
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffffff00Into the Breach has been kept in your quest log without losing objective progress. Purchase TBC passage before claiming its reward.|r");
            LOG_INFO("module", "IndividualProgression: deferred unpaid character {} quest 10259 completion at login",
                player->GetGUID().GetCounter());
        }

        sIndividualProgression->SendPaidExpansionReminder(player);
    }"""
        actual = code(body(EXP, "void OnPlayerLogin(Player* player) override"))
        self.assertEqual(" ".join(actual.split()), " ".join(expected.split()))

    def test_feature_off_bot_start_privilege_and_paid_cases_are_left_alone(self):
        # Documentation-only truth table; the executable source is pinned above.
        def defer(enabled, normal, entitled, complete):
            return enabled and normal and not entitled and complete

        for enabled in (False, True):
            for normal in (False, True):
                for entitled in (False, True):
                    for complete in (False, True):
                        with self.subTest(enabled=enabled, normal=normal,
                                          entitled=entitled, complete=complete):
                            self.assertEqual(defer(enabled, normal, entitled, complete),
                                             (enabled, normal, entitled, complete) ==
                                             (True, True, False, True))

    def test_pre_map_refresh_remains_read_only_and_no_new_hooks(self):
        refresh = code(body(EXP, "void OnPlayerSendInitialPacketsBeforeAddToMap("))
        self.assertIn("LoadPaidExpansionEntitlement(player);", refresh)
        self.assertNotIn("IncompleteQuest(", refresh)
        self.assertEqual(EXP.count("PLAYERHOOK_ON_LOGIN"), 1)


class UnpaidTbcQuestAccessGate(unittest.TestCase):
    """Pin IP-only acceptance undo and NPC gossip guard without changing Core."""

    def test_only_unpaid_normal_accounts_are_refused(self):
        helper = code(body(EXP, "bool MustRefuseUnpaidTbcQuest(Player* player)"))
        self.assertEqual(" ".join(helper.split()), " ".join("""bool MustRefuseUnpaidTbcQuest(Player* player)
        {
            return player && sIndividualProgression->IsPaidExpansionActive() &&
                sIndividualProgression->isNormalAccount(player) &&
                !sIndividualProgression->HasExpansionEntitlement(player, Era::Tbc);
        }""".split()))

    def test_accept_undo_is_ordered_and_cannot_leave_ghost_slot(self):
        method = code(body(EXP, "void OnPlayerQuestAccept(Player* player, Quest const* quest) override"))
        expected = """void OnPlayerQuestAccept(Player* player, Quest const* quest) override
        {
            if (!quest || quest->GetQuestId() != INTO_THE_BREACH ||
                !MustRefuseUnpaidTbcQuest(player))
                return;
            uint16 const slot = player->FindQuestSlot(INTO_THE_BREACH);
            if (slot < MAX_QUEST_LOG_SIZE)
                player->SetQuestSlot(slot, 0);
            player->RemoveActiveQuest(INTO_THE_BREACH, true);
            player->RemoveTimedAchievement(ACHIEVEMENT_TIMED_TYPE_QUEST, INTO_THE_BREACH);
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffffff00Purchase The Burning Crusade from Chronomancer Vezrath before taking Into the Breach.|r");
            LOG_INFO("module", "IndividualProgression: refused unpaid character {} quest 10259 acceptance",
                player->GetGUID().GetCounter());
        }"""
        self.assertEqual(" ".join(method.split()), " ".join(expected.split()))
        self.assertLess(method.index("FindQuestSlot("), method.index("SetQuestSlot("))
        self.assertLess(method.index("SetQuestSlot("), method.index("RemoveActiveQuest("))
        self.assertLess(method.index("RemoveActiveQuest("), method.index("RemoveTimedAchievement("))
        for forbidden in ("ModifyMoney(", "UpdateProgressionState(", "RemoveRewardedQuest(",
                          "SetRewardedQuest(", "ForceUpdateProgressionState(", "AbandonQuest("):
            self.assertNotIn(forbidden, method)

    def test_gossip_guard_is_scoped_to_19942_none_and_unpaid(self):
        method = code(body(EXP, "bool CanCreatureGossipHello(Player* player, Creature* creature) override"))
        expected = """bool CanCreatureGossipHello(Player* player, Creature* creature) override
        {
            if (!creature || creature->GetEntry() != 19942 ||
                !MustRefuseUnpaidTbcQuest(player) ||
                player->GetQuestStatus(INTO_THE_BREACH) != QUEST_STATUS_NONE)
                return false;
            ClearGossipMenuFor(player);
            if (player->GetSession())
                ChatHandler(player->GetSession()).PSendSysMessage(
                    "|cffffff00Purchase The Burning Crusade from Chronomancer Vezrath before taking Into the Breach.|r");
            CloseGossipMenuFor(player);
            return true;
        }"""
        self.assertEqual(" ".join(method.split()), " ".join(expected.split()))
        # Do not count commented-out hook registration as working code.
        # Pin the whole registration body and reject preprocessor disabled code
        # (#if 0 evades ordinary comment stripping).
        registered = body(EXP, "void AddSC_ip_paid_expansion()")
        self.assertNotRegex(registered, r"(?m)^\s*#\s*if(?:def|ndef)?\b")
        active = code(re.sub(r"/\*.*?\*/", "", registered, flags=re.S))
        expected_add_sc = """void AddSC_ip_paid_expansion()
        {
            new IndividualProgression_PaidExpansionPlayerScript();
            new IndividualProgression_PaidExpansionQuestGossip();
            new IndividualProgression_PaidTbcQuestRewardPacketGuard();
        }"""
        self.assertEqual(" ".join(active.split()), " ".join(expected_add_sc.split()))
        self.assertEqual(active.count("new IndividualProgression_PaidExpansionQuestGossip();"), 1)
        self.assertEqual(EXP.count("new IndividualProgression_PaidExpansionQuestGossip();"), 1)
        # Extra narrow before-RewardQuest packet gate is present exactly once;
        # the two historically registered scripts keep their original order.
        self.assertEqual(active.count("new IndividualProgression_PaidTbcQuestRewardPacketGuard();"), 1)
        self.assertLess(active.index("new IndividualProgression_PaidExpansionQuestGossip();"),
                        active.index("new IndividualProgression_PaidTbcQuestRewardPacketGuard();"))

        script = body(EXP, "class IndividualProgression_PaidExpansionPlayerScript")
        self.assertNotRegex(script, r"(?m)^\s*#\s*if(?:def|ndef)?\b")
        script = code(re.sub(r"/\*.*?\*/", "", script, flags=re.S))
        self.assertIn("PLAYERHOOK_ON_BEFORE_QUEST_COMPLETE, PLAYERHOOK_ON_PLAYER_QUEST_ACCEPT })",
                      " ".join(script.split()))
        self.assertEqual(script.count("PLAYERHOOK_ON_PLAYER_QUEST_ACCEPT"), 1)
        self.assertNotIn("GetDialogStatus", EXP)
        self.assertNotIn("creature_template", EXP)

    def test_four_bit_gate_truth_table_is_documentation_only(self):
        # Source is compared independently above. This models OFF / excluded /
        # paid/start and the requested quest only, without game execution.
        for enabled in (False, True):
            for normal in (False, True):
                for entitled in (False, True):
                    result = enabled and normal and not entitled
                    self.assertEqual(result, (enabled, normal, entitled) == (True, True, False))


class StrictReceiptRuntime(unittest.TestCase):
    def test_raw_settings_are_decoded_before_ledger_or_account_entitlement(self):
        loader = code(body(EXP, "void IndividualProgression::LoadPaidExpansionEntitlement(Player* player)"))
        reader = code(body(EXP, "static ReceiptPolicy::Decoded ReadStrictPaidLedger(uint32 guid)"))
        self.assertIn("SELECT data FROM character_settings", reader)
        self.assertIn("ReceiptPolicy::DecodeRaw(", reader)
        self.assertIn("ReceiptPolicy::Decoded const ledger = ReadStrictPaidLedger(guid);", loader)
        self.assertNotIn("GetPlayerSetting(", loader)
        self.assertIn("_paidExpansionLedgers.erase(guid);", loader)
        acct = code(body(EXP, "uint8 IndividualProgression::GetAccountEffectiveProgression(uint32 accountId)"))
        self.assertIn("ReceiptPolicy::DecodeRaw(data, !fields[1].IsNull())", acct)

    def test_denied_unknown_row_and_receipt_built_before_money(self):
        can = code(body(EXP, "PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"))
        purchase = code(body(EXP, "PurchaseResult IndividualProgression::PurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"))
        self.assertIn("_paidExpansionLedgers.find(", can)
        self.assertLess(can.index("_paidExpansionLedgers.find("), can.index("return CheckPurchase("))
        self.assertLess(purchase.index("ReceiptPolicy::RecordPurchase("),
                        purchase.index("player->ModifyMoney(-static_cast<int32>(costCopper));"))
        self.assertLess(purchase.index("if (!opened)"), purchase.index("uint32 const moneySlot"))
        self.assertLess(purchase.index("uint32 const moneySlot"), purchase.index("player->SaveToDB(false, false);"))
        self.assertIn("player->UpdatePlayerSetting(PaidExpansionSettingSource, moneySlot", purchase)
        self.assertIn("player->UpdatePlayerSetting(PaidExpansionSettingSource, timeSlot", purchase)
        self.assertEqual(purchase.count("player->SaveToDB(false, false);"), 1)
        self.assertNotIn("PlayerSettingsStore::UpdateSetting(", purchase)
        self.assertNotIn("CharacterDatabase.Execute(", purchase)

class StorageAndThreads(unittest.TestCase):
    def test_hot_path_reads_only_ip_cache(self):
        facts = code(body(EXP, "CharacterFacts IndividualProgression::GetExpansionFacts(Player* player)"))
        self.assertNotIn("GetPlayerSetting", facts)
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_paidExpansionLock);", facts)

    def test_settings_store_used_only_on_login_thread_or_purchase(self):
        users = {name for name in re.findall(r"\n(?:void|bool|uint8|PurchaseResult|CharacterFacts) IndividualProgression::(\w+)\(", EXP)
                 if "GetPlayerSetting" in body(EXP, f"IndividualProgression::{name}(")}
        self.assertEqual(users, {"EvaluatePaidExpansionGrandfather"})

    def test_cache_filled_before_map_add_and_kept_across_logout(self):
        script = body(EXP, "class IndividualProgression_PaidExpansionPlayerScript")
        # The paid PlayerScript now also registers the pre-completion veto;
        # check each required hook independently rather than freezing a
        # two-entry initializer list and masking a real hook regression.
        self.assertIn("PLAYERHOOK_ON_SEND_INITIAL_PACKETS_BEFORE_ADD_TO_MAP", script)
        self.assertIn("PLAYERHOOK_ON_LOGIN", script)
        self.assertIn("PLAYERHOOK_ON_BEFORE_QUEST_COMPLETE", script)
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
