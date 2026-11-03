"""Source-only F14 per-login RandomBot incarnation token safety contracts.

These are NOT compiler, map-thread, full actor-provenance or gameplay tests.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = (ROOT / "src/IndividualProgression.cpp").read_text()
HEADER = (ROOT / "src/IndividualProgression.h").read_text()
PLAYER = (ROOT / "src/IndividualProgressionPlayer.cpp").read_text()


def function(src, signature):
    start = src.index(signature)
    brace = src.index("{", start)
    level = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            level += 1
        elif src[i] == "}":
            level -= 1
            if level == 0:
                return src[start:i + 1]
    raise AssertionError("Unterminated source function: " + signature)


class F14LoginIncarnationSafety(unittest.TestCase):
    def test_distinct_token_api_is_not_reusable_guid(self):
        self.assertIn("std::uint64_t GetF14VerifiedRandomBotLoginEpoch(ObjectGuid const& guid);", HEADER)
        self.assertIn("std::unordered_map<uint32, std::uint64_t> _f14VerifiedRandomBotLoginEpoch;", HEADER)
        self.assertIn("std::uint64_t _f14NextRandomBotLoginEpoch = 0;", HEADER)
        self.assertIn("std::unordered_map<uint32, bool> _f14VerifiedRandomBotGroupCache;", HEADER)

    def test_writer_has_one_existing_lock_for_both_identity_maps(self):
        m = function(CORE, "void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(")
        self.assertIn("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);", m)
        self.assertLess(m.index("std::unique_lock"), m.index("_f14VerifiedRandomBotGroupCache.erase(guid);"))
        self.assertLess(m.index("std::unique_lock"), m.index("_f14VerifiedRandomBotLoginEpoch.erase(guid);"))
        self.assertIn("uint32 const guid = player->GetGUID().GetCounter();", m)
        self.assertNotIn("sRandomPlayerbotMgr", m)

    def test_unverified_or_logout_eviction_clears_both(self):
        m = function(CORE, "void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(")
        self.assertLess(m.index("_f14VerifiedRandomBotGroupCache.erase(guid);"), m.index("if (!verified)"))
        self.assertLess(m.index("_f14VerifiedRandomBotLoginEpoch.erase(guid);"), m.index("if (!verified)"))
        self.assertIn("if (!verified)\n        return;", m)
        logout = function(CORE, "void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(")
        self.assertIn("RememberF14VerifiedRandomBotGroupIdentity(player, false);", logout)
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);",
                      function(PLAYER, "void OnPlayerLogout(Player* player) override"))

    def test_new_positive_login_always_rotates_epoch_and_refuses_wrap(self):
        m = function(CORE, "void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(")
        self.assertIn("std::numeric_limits<std::uint64_t>::max()", m)
        self.assertIn("if (_f14NextRandomBotLoginEpoch == std::numeric_limits<std::uint64_t>::max())\n        return;", m)
        self.assertIn("const std::uint64_t loginEpoch = ++_f14NextRandomBotLoginEpoch;", m)
        self.assertIn("_f14VerifiedRandomBotGroupCache[guid] = true;", m)
        self.assertIn("_f14VerifiedRandomBotLoginEpoch[guid] = loginEpoch;", m)
        # Established group protections remain verified even if the
        # visibility-only epoch counter is exhausted. The missing epoch
        # makes new concealment fail visible without changing group rules.
        self.assertLess(m.index("_f14VerifiedRandomBotGroupCache[guid] = true;"),
                        m.index("if (_f14NextRandomBotLoginEpoch"))
        self.assertLess(m.index("if (_f14NextRandomBotLoginEpoch"),
                        m.index("_f14VerifiedRandomBotLoginEpoch[guid] = loginEpoch;"))
        self.assertLess(m.index("_f14VerifiedRandomBotGroupCache.erase(guid);"), m.index("_f14VerifiedRandomBotGroupCache[guid] = true;"))
        self.assertNotIn("= player->GetGUID().GetCounter();\n    _f14VerifiedRandomBotLoginEpoch", m)

    def test_reader_requires_both_positive_verification_and_new_epoch(self):
        m = function(CORE, "std::uint64_t IndividualProgression::GetF14VerifiedRandomBotLoginEpoch(")
        self.assertIn("if (!guid || !guid.IsPlayer())\n        return 0;", m)
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);", m)
        self.assertIn("_f14VerifiedRandomBotGroupCache.find(lowGuid)", m)
        self.assertIn("verified == _f14VerifiedRandomBotGroupCache.end() || !verified->second", m)
        self.assertIn("_f14VerifiedRandomBotLoginEpoch.find(lowGuid)", m)
        self.assertIn("epoch == _f14VerifiedRandomBotLoginEpoch.end() ? 0 : epoch->second;", m)
        self.assertNotIn("GetSession()", m)
        self.assertNotIn("sRandomPlayerbotMgr", m)
        self.assertNotIn("CharacterDatabase", m)

    def test_source_verified_login_owns_positive_assignment(self):
        login = function(PLAYER, "void OnPlayerLogin(Player* player) override")
        self.assertLess(login.index("ForgetF14VerifiedRandomBotGroupIdentity(player);"),
                        login.index("RememberF14VerifiedRandomBotGroupIdentity("))
        self.assertIn("player->GetSession()->IsHeadless()", login)
        self.assertIn("player->getRace() == RACE_BLOODELF", login)
        self.assertIn("player->getRace() == RACE_DRAENEI", login)
        self.assertIn("sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter())", login)
        self.assertNotIn("GetF14VerifiedRandomBotLoginEpoch(", login)

    def test_who_and_observer_guid_reader_fail_visible_without_current_epoch(self):
        player_reader = function(CORE, "bool IndividualProgression::HasF14VerifiedRandomBotIdentity(")
        guid_reader = function(CORE, "bool IndividualProgression::HasF14VerifiedRandomBotGuid(")
        for reader in (player_reader, guid_reader):
            self.assertIn("std::shared_lock<std::shared_mutex> lock(_f14GroupCacheLock);", reader)
            self.assertIn("_f14VerifiedRandomBotGroupCache.find(", reader)
            self.assertIn("_f14VerifiedRandomBotLoginEpoch.find(", reader)
            self.assertIn("epoch != _f14VerifiedRandomBotLoginEpoch.end() && epoch->second != 0", reader)
            self.assertNotIn("sRandomPlayerbotMgr", reader)
            self.assertNotIn("CharacterDatabase", reader)
            self.assertNotIn("GetSession()", reader)
        # Historic verified cache stays useful to the group-progress guard
        # even if optional visibility epochs are exhausted.
        group_floor = function(CORE, "bool IndividualProgression::ShouldSkipF14VerifiedRandomBotGroupDowngrade(")
        self.assertIn("_f14VerifiedRandomBotGroupCache.find(", group_floor)
        self.assertNotIn("_f14VerifiedRandomBotLoginEpoch", group_floor)

    def test_epoch_used_only_for_verified_attack_start_not_observer_hot_path(self):
        # Actual attack start/target switch must have both positive identity
        # AND a current login incarnation, never just a recycled GUID.
        attack = function(PLAYER, "void OnF14AttackTargetTransition(")
        self.assertIn("HasF14VerifiedRandomBotIdentity(candidate)", attack)
        self.assertIn("GetF14VerifiedRandomBotLoginEpoch(candidate->GetGUID())", attack)
        self.assertLess(attack.index("HasF14VerifiedRandomBotIdentity(candidate)"),
                        attack.index("GetF14VerifiedRandomBotLoginEpoch(candidate->GetGUID())"))
        self.assertEqual(PLAYER.count("GetF14VerifiedRandomBotLoginEpoch("), 1,
                         "Epoch lookup must be confined to map-event attack-start guard")
        observer = function(PLAYER, "bool OnWorldObjectCanBeSeenByPlayer(")
        who = function(PLAYER, "bool OnWhoListEntryCanBeSeenByPlayer(")
        self.assertNotIn("GetF14VerifiedRandomBotLoginEpoch(", observer)
        self.assertNotIn("GetF14VerifiedRandomBotLoginEpoch(", who)
        self.assertIn("COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", CORE)
        self.assertNotIn("#define COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", CORE)

    def test_no_mutation_of_paid_or_gameplay_data_in_identity_methods(self):
        for signature in ("void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(",
                          "std::uint64_t IndividualProgression::GetF14VerifiedRandomBotLoginEpoch("):
            body = function(CORE, signature)
            for forbidden in ("RewardQuest(", "SetPhaseMask(", "SaveToDB(", "CharacterDatabase",
                              "WorldDatabase", "ModifyMoney(", "_paidExpansionFlags",
                              "_paidExpansionLedgers", "UpdateObjectVisibility("):
                self.assertNotIn(forbidden, body)


if __name__ == "__main__":
    unittest.main()
