#!/usr/bin/env python3
"""Historical Playerbots/Core/IP evidence gate: F14 early identity is NOT name-only.

All inputs are synthetic or read-only from exact public pinned source checkouts.
Green means known ordering and negative identity boundaries remain understood,
NOT that any character/quest/group/visibility is protected in live Worldserver.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import unittest
from dataclasses import dataclass
from enum import Enum

CORE = Path(os.environ.get("PINNED_CORE_ROOT", "historical-core"))
PB = Path(os.environ.get("PINNED_PLAYERBOTS_ROOT", "historical-playerbots"))
IP = Path(os.environ.get("PINNED_IP_ROOT", "."))


class Verdict(Enum):
    VERIFIED_RANDOMBOT = "verified-randombot"
    VERIFIED_NOT_RANDOMBOT = "verified-not-randombot"
    UNKNOWN = "unknown"


@dataclass(frozen=True)
class Snapshot:
    boot_epoch: int
    prefix: str
    account_id: int
    account_name: str
    account_type: int | None
    selected_guid: int | None
    selected_epoch: int | None
    session_guid: int
    session_account_id: int
    is_playerbot_session: bool


def matches_generated_account_name(name: str, configured_prefix: str) -> bool:
    """Name is candidate hint ONLY. Exact prefix+numeric suffix, case folded."""
    if not configured_prefix or not name:
        return False
    lower_name = name.lower()
    lower_prefix = configured_prefix.lower()
    suffix = lower_name[len(lower_prefix):] if lower_name.startswith(lower_prefix) else ""
    return bool(suffix) and suffix.isascii() and suffix.isdecimal()


def classify(snapshot: Snapshot) -> Verdict:
    """Proposed external-proof input. This is NOT wired to IP or live Playerbots."""
    if not snapshot.account_id or not snapshot.session_guid:
        return Verdict.UNKNOWN
    if snapshot.session_account_id != snapshot.account_id:
        return Verdict.UNKNOWN
    if snapshot.account_type == 2:
        # Even if owner's current usage excludes AddClass, historical accounts
        # may exist. No F14 positive write for them.
        return Verdict.VERIFIED_NOT_RANDOMBOT
    if snapshot.account_type == 0:
        return Verdict.VERIFIED_NOT_RANDOMBOT
    if snapshot.account_type != 1:
        return Verdict.UNKNOWN
    if not matches_generated_account_name(snapshot.account_name, snapshot.prefix):
        return Verdict.UNKNOWN
    if not snapshot.is_playerbot_session:
        return Verdict.UNKNOWN
    if (snapshot.selected_guid != snapshot.session_guid or
            snapshot.selected_epoch != snapshot.boot_epoch):
        return Verdict.UNKNOWN
    return Verdict.VERIFIED_RANDOMBOT


class SourceEvidence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pb_config = (PB / "src/PlayerbotAIConfig.cpp").read_text()
        cls.pb_factory = (PB / "src/Bot/Factory/RandomPlayerbotFactory.cpp").read_text()
        cls.pb_mgr = (PB / "src/Bot/RandomPlayerbotMgr.cpp").read_text()
        cls.pb_db = (PB / "src/Db/PlayerbotsDatabase.cpp").read_text()
        cls.pb_session = (PB / "src/Bot/PlayerbotMgr.cpp").read_text()
        cls.pb_world = (PB / "src/Script/Playerbots.cpp").read_text()
        cls.core_session = (CORE / "src/server/game/Server/WorldSession.h").read_text()
        cls.core_login = (CORE / "src/server/game/Handlers/CharacterHandler.cpp").read_text()
        cls.core_account = (CORE / "src/server/game/Accounts/AccountMgr.cpp").read_text()
        cls.ip = (IP / "src/IndividualProgression.cpp").read_text()
        cls.ip_login = (IP / "src/IndividualProgressionPlayer.cpp").read_text()

    def test_prefix_configurable_not_fixed(self):
        self.assertIn('GetOption<std::string>("Playerbots.RandomBotAccountPrefix", "rndbot")',
                      self.pb_config)
        self.assertIn('out << sPlayerbotAIConfig.RandomBotAccountPrefix << accountNumber',
                      self.pb_factory)

    def test_type_0_1_2_all_share_generated_name_pool(self):
        self.assertIn('std::vector<uint32> allRandomBotAccounts;', self.pb_mgr)
        self.assertIn('currentAssignments[accountId] = 0', self.pb_mgr)
        self.assertIn('currentAssignments[accountId] = 1', self.pb_mgr)
        self.assertIn('currentAssignments[accountId] = 2', self.pb_mgr)
        self.assertIn('if (accountType == 1) rndBotTypeAccounts.push_back(accountId)', self.pb_mgr)
        self.assertIn('else if (accountType == 2) addClassTypeAccounts.push_back(accountId)', self.pb_mgr)
        self.assertIn('SELECT account_id, account_type FROM playerbots_account_type', self.pb_db)

    def test_addclass_zero_clears_type2_but_not_unassigned_pool(self):
        source = self.pb_factory
        self.assertIn('sPlayerbotAIConfig.AddClassAccountPoolSize == 0', source)
        self.assertIn('stmt->SetData(0, uint8(2));', source)
        self.assertIn('PLAYERBOTS_UPD_ACCOUNT_TYPE_UNASSIGN', source)
        self.assertIn('UPDATE playerbots_account_type SET account_type = 0 WHERE account_type = ?',
                      self.pb_db)

    def test_startup_invalidates_saved_add_events(self):
        start = self.pb_mgr.index('void RandomPlayerbotMgr::Init()')
        end = self.pb_mgr.index('void RandomPlayerbotMgr::InitArenaTeams()', start)
        section = self.pb_mgr[start:end]
        self.assertIn('PLAYERBOTS_DEL_RANDOM_BOTS_BY_EVENT', section)
        self.assertIn('stmt->SetData(0, std::string("add"));', section)
        self.assertIn('PlayerbotsDatabase.DirectExecute(stmt);', section)
        self.assertIn('DELETE FROM playerbots_random_bots WHERE event = ?', self.pb_db)
        # In particular, owner=0/event=add is not immutable across a server restart.

    def test_selection_sets_event_and_current_guid_before_login(self):
        start = self.pb_mgr.index('auto tryLoginBot =')
        end = self.pb_mgr.index('// PHASE 1:', start)
        section = self.pb_mgr[start:end]
        tokens = ['GetEventValue(charInfo.guid, "add")',
                  'SetEventValue(charInfo.guid, "add", 1, add_time);',
                  'currentBots.insert(charInfo.guid);']
        positions = [section.index(k) for k in tokens]
        self.assertEqual(positions, sorted(positions))
        self.assertIn('currentBots.contains(charInfo.guid)', section)

    def test_israndombot_requires_current_guid_not_just_type1_account(self):
        start = self.pb_mgr.index('bool RandomPlayerbotMgr::IsRandomBot(ObjectGuid::LowType bot)')
        end = self.pb_mgr.index('bool RandomPlayerbotMgr::IsAddclassBot(Player* bot)', start)
        section = self.pb_mgr[start:end]
        self.assertIn('IsInRandomAccountList(', section)
        self.assertIn('currentBots.contains(bot)', section)

    def test_lazy_current_bots_and_startup_are_not_immutable(self):
        start = self.pb_mgr.index('void RandomPlayerbotMgr::GetBots()')
        end = self.pb_mgr.index('std::vector<uint32> RandomPlayerbotMgr::GetBgBots(', start)
        self.assertIn('if (!currentBots.empty())', self.pb_mgr[start:end])
        self.assertIn('GetBots();', self.pb_mgr)
        self.assertIn('currentBots.erase(bot)', self.pb_mgr)

    def test_world_thread_drives_pending_bot_login_and_random_selection(self):
        # In this historical source, headless query processing and the
        # RandomPlayerbotMgr update execute synchronously in WorldScript.
        # This is promising for a read-only IsRandomBot check at IP login,
        # but does NOT prove every future version or map-thread reader safe.
        start = self.pb_world.index('class PlayerbotsWorldScript : public WorldScript')
        tick = self.pb_world.index('void OnUpdate(uint32 diff) override', start)
        end = self.pb_world.index('void OnShutdown() override', tick)
        section = self.pb_world[tick:end]
        pending = section.index('PlayerbotHolder::UpdatePendingLogins();')
        events = section.index('PlayerbotWorldThreadProcessor::instance().Update(diff);')
        random = section.index('sRandomPlayerbotMgr.UpdateAI(diff);')
        self.assertLess(pending, events)
        self.assertLess(events, random)
        self.assertIn('session->ProcessQueryCallbacks();', self.pb_session)
        self.assertIn('botSession->HandlePlayerLoginFromDB(holder)', self.pb_session)

    def test_selected_guid_exists_before_async_headless_login(self):
        # AddRandomBots chooses ONLY type-1 account IDs; it inserts currentBots
        # before the later UpdatePendingLogins callback loads the character.
        start = self.pb_mgr.index('uint32 RandomPlayerbotMgr::AddRandomBots()')
        end = self.pb_mgr.index('void RandomPlayerbotMgr::LoadBattleMastersCache()', start)
        section = self.pb_mgr[start:end]
        self.assertIn('accountsToUse = rndBotTypeAccounts;', section)
        self.assertIn('std::vector<uint32> shuffledAccounts = rndBotTypeAccounts;', section)
        self.assertIn('currentBots.insert(charInfo.guid);', section)
        # This is a per-boot mutable membership signal, not a saved DB marker.
        self.assertIn('currentBots.erase(bot);', self.pb_mgr)

    def test_existing_israndom_uses_no_sync_database_query(self):
        start = self.pb_mgr.index('bool RandomPlayerbotMgr::IsRandomBot(ObjectGuid::LowType bot)')
        end = self.pb_mgr.index('bool RandomPlayerbotMgr::IsAddclassBot(Player* bot)', start)
        body = self.pb_mgr[start:end]
        self.assertIn('sPlayerbotAIConfig.IsInRandomAccountList(', body)
        self.assertIn('currentBots.contains(bot)', body)
        self.assertNotIn('Database.Query(', body)
        self.assertNotIn('IsAccountType(', body)
        self.assertNotIn('AccountMgr::GetName(', body)

    def test_core_login_script_runs_before_later_pb_callback(self):
        start = self.pb_session.index('void PlayerbotHolder::HandlePlayerBotLoginCallback')
        portion = self.pb_session[start:start + 4500]
        tokens = ['botSession->HandlePlayerLoginFromDB(holder)',
                  'sRandomPlayerbotMgr.OnPlayerLogin(bot)']
        self.assertEqual([portion.index(t) for t in tokens],
                         sorted(portion.index(t) for t in tokens))
        self.assertIn('sScriptMgr->OnPlayerLogin(pCurrChar);', self.core_login)

    def test_ip_existing_bot_account_is_regex_cache_not_positive_bot_identity(self):
        self.assertIn('std::regex_match(accountName, _botAccountsRegex)', self.ip)
        self.assertIn('AccountMgr::GetName(accountId, accountName)', self.ip)
        self.assertIn('return GetAccountMatch(accountId).bot;', self.ip)
        self.assertIn('LOGIN_GET_USERNAME_BY_ID', self.core_account)
        self.assertIn('uint32 GetAccountId() const { return _accountId; }',
                      self.core_session)
        self.assertIn('ForceUpdateProgressionState(player, static_cast<ProgressionState>(0))',
                      self.ip_login)

    def test_pre_login_handoff_must_remain_external_to_policy(self):
        policy = (IP / 'src/IndividualProgressionRandomBotStagePolicy.h').read_text()
        self.assertIn('VerifiedRandomBot', policy)
        self.assertIn('HoldUnknownIdentity', policy)
        self.assertNotIn('LoginDatabase', policy)
        self.assertNotIn('AccountMgr::GetName', policy)
        self.assertNotIn('RandomBotAccountPrefix', policy)


class SyntheticIdentityContract(unittest.TestCase):
    def base(self, **overrides):
        d = dict(boot_epoch=7, prefix="rndbot", account_id=23,
                 account_name="RNDBOT42", account_type=1, selected_guid=912,
                 selected_epoch=7, session_guid=912, session_account_id=23,
                 is_playerbot_session=True)
        d.update(overrides)
        return Snapshot(**d)

    def test_exact_positive_case(self):
        self.assertEqual(classify(self.base()), Verdict.VERIFIED_RANDOMBOT)

    def test_addclass_not_used_doesnt_prove_type1(self):
        self.assertEqual(classify(self.base(account_type=2)),
                         Verdict.VERIFIED_NOT_RANDOMBOT)
        self.assertEqual(classify(self.base(account_type=0)),
                         Verdict.VERIFIED_NOT_RANDOMBOT)
        self.assertEqual(classify(self.base(account_type=None)), Verdict.UNKNOWN)

    def test_name_only_is_never_positive(self):
        for name in ("rndbot", "rndbotabc", "user_rndbot42", "rndbot42extra",
                     "rndbot42.5", "rndbot 42", ""):
            with self.subTest(name=name):
                self.assertEqual(classify(self.base(account_name=name)),
                                 Verdict.UNKNOWN)

    def test_custom_prefix_not_hardcoded_rndbot(self):
        self.assertEqual(classify(self.base(prefix="cosmicbot",
                                             account_name="CosmicBot001")),
                         Verdict.VERIFIED_RANDOMBOT)
        self.assertEqual(classify(self.base(prefix="cosmicbot",
                                             account_name="RNDBOT42")),
                         Verdict.UNKNOWN)

    def test_no_old_boot_selected_bot_reused_after_restart(self):
        self.assertEqual(classify(self.base(boot_epoch=8)), Verdict.UNKNOWN)
        self.assertEqual(classify(self.base(selected_epoch=None)), Verdict.UNKNOWN)
        self.assertEqual(classify(self.base(selected_guid=None)), Verdict.UNKNOWN)

    def test_mismatched_character_and_account_are_unknown(self):
        self.assertEqual(classify(self.base(session_guid=999)), Verdict.UNKNOWN)
        self.assertEqual(classify(self.base(session_account_id=17)), Verdict.UNKNOWN)
        self.assertEqual(classify(self.base(account_id=0)), Verdict.UNKNOWN)
        self.assertEqual(classify(self.base(session_guid=0)), Verdict.UNKNOWN)

    def test_not_yet_selected_pool_account_not_assumed_active(self):
        self.assertEqual(classify(self.base(selected_guid=0)), Verdict.UNKNOWN)
        self.assertEqual(classify(self.base(is_playerbot_session=False)),
                         Verdict.UNKNOWN)

    def test_unassigned_name_stays_not_random_when_bot_use_disabled(self):
        for suffix in range(500):
            s = self.base(account_name="rndbot" + str(suffix), account_type=0)
            self.assertNotEqual(classify(s), Verdict.VERIFIED_RANDOMBOT)


if __name__ == "__main__":
    unittest.main()
