"""F14 social isolation: source contracts, not in-game proof.

Pins that the feature is default OFF, uses only the four social PlayerScript
hooks plus the /who-only query (never the paused world-visibility query),
decides identity from IP's verified RandomBot cache (never race alone),
answers through a packet to the human's own session (never Player::Whisper),
and leaves every other file of IP #172 unchanged apart from the exact
reviewed insertions.
"""
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT = "aed05a92f262362608f5d643c8570b504189aae0"  # IP #172 head
POLICY = "src/IndividualProgressionF14SocialIsolationPolicy.h"
SCRIPT = "src/IndividualProgressionF14SocialIsolationScript.cpp"
KEY = "IndividualProgression.F14.RandomBotSocialIsolation"
SCOPE = {
    POLICY, SCRIPT,
    "src/IndividualProgression.h", "src/IndividualProgression.cpp", "src/IndividualProgressionPlayer.cpp",
    "conf/individualProgression.conf.dist",
    "tests/f14_social_isolation_policy_test.cpp", "tests/test_f14_social_isolation_source.py",
    ".github/workflows/f14-social-isolation.yml",
    "docs/F14_SOCIAL_ISOLATION_2026-11-13.md", "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True, capture_output=True).stdout


def parent(name):
    return git("show", PARENT + ":" + name).decode("utf-8")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def added_lines(name):
    out = subprocess.run(["git", "-C", str(ROOT), "diff", "-U0", PARENT, "--", name],
                         check=True, capture_output=True, text=True).stdout
    plus = [l[1:] for l in out.splitlines() if l.startswith("+") and not l.startswith("+++")]
    minus = [l[1:] for l in out.splitlines() if l.startswith("-") and not l.startswith("---")]
    return plus, minus


class DefaultOff(unittest.TestCase):
    def test_conf_defaults(self):
        conf = read("conf/individualProgression.conf.dist")
        self.assertEqual(re.findall(r"(?m)^" + re.escape(KEY) + r"\s*=\s*(\S+)\s*$", conf), ["0"])
        self.assertRegex(conf, r"(?m)^" + re.escape(KEY) + r"\.ReplyCooldownSeconds\s*=\s*60\s*$")
        for key in re.findall(r"(?m)^(IndividualProgression\.(?:F14\.\w+|PaidExpansion\.Enable))\s*=", conf):
            self.assertRegex(conf, r"(?m)^" + re.escape(key) + r"\s*=\s*0\s*$", key)

    def test_member_default_and_forced_off_without_playerbots(self):
        self.assertIn("bool f14RandomBotSocialIsolation = false;", read("src/IndividualProgression.h"))
        core = read("src/IndividualProgression.cpp")
        self.assertIn('"' + KEY + '", false)', core)
        block = core.split('#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")', 1)[1]
        block = block[:block.index("#endif")]
        self.assertIn("sIndividualProgression->f14RandomBotSocialIsolation = false;", block)

    def test_every_hook_checks_the_option_first(self):
        src = strip_comments(read(SCRIPT))
        self.assertIn("return sIndividualProgression->enabled && sIndividualProgression->f14RandomBotSocialIsolation;", src)
        allow = src.split("bool AllowPair(", 1)[1]
        self.assertIn("if (!OptionOn() || !initiator || !other || initiator == other)", allow[:400])
        for sig in ("bool OnPlayerCanGroupInvite(", "bool OnPlayerCanGroupAccept(",
                    "bool OnF14ShouldExcludeWhoEntryForViewer("):
            body = src.split(sig, 1)[1][:400]
            self.assertIn("!OptionOn()", body, sig)


class HooksAndAuthority(unittest.TestCase):
    def setUp(self):
        self.src = strip_comments(read(SCRIPT))

    def test_only_social_hooks_and_who(self):
        hooks = sorted(set(re.findall(r"\b(PLAYERHOOK_[A-Z_]+|WORLDOBJECTHOOK_[A-Z0-9_]+|WORLDHOOK_[A-Z0-9_]+)\b", self.src)))
        self.assertEqual(hooks, sorted([
            "PLAYERHOOK_CAN_GROUP_ACCEPT", "PLAYERHOOK_CAN_GROUP_INVITE", "PLAYERHOOK_CAN_INIT_TRADE",
            "PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT", "WORLDOBJECTHOOK_ON_F14_WHO_GUID_EXCLUSION_QUERY",
            "WORLDHOOK_ON_UPDATE", "PLAYERHOOK_ON_BEFORE_UPDATE"]))
        overrides = sorted(re.findall(r"\b(On\w+)\([^;{]*\)\s*override", self.src))
        self.assertEqual(overrides, sorted([
            "OnF14ShouldExcludeWhoEntryForViewer", "OnPlayerCanGroupAccept", "OnPlayerCanGroupInvite",
            "OnPlayerCanInitTrade", "OnPlayerCanUseChat", "OnUpdate", "OnPlayerBeforeUpdate"]))

    def body(self, signature, end="\n    }\n"):
        return self.src.split(signature, 1)[1].split(end, 1)[0]

    def test_t1_no_cross_map_read_off_the_world_thread(self):
        # The world-update hook only marks the thread and tick; it reads nothing.
        upd = re.search(r"void OnUpdate\(uint32\s*\)\s*override\s*\{([^}]*)\}", self.src)
        self.assertIsNotNone(upd)
        self.assertEqual(upd.group(1).strip(), "WorldThread().Mark();")
        # The per-update record touches only the player this thread updates.
        rec = self.body("void OnPlayerBeforeUpdate(Player* player, uint32 ) override", "\n        }\n")
        self.assertIn("ThisThreadOwnedMap() = {player->FindMap(), WorldThread().Tick(), true};", rec)
        self.assertEqual(self.src.count("->FindMap()"), 1, "FindMap only on the player being updated")
        policy = strip_comments(read(POLICY))
        self.assertIn("return onWorldThread || (ownedScopeValid && firstInOwnedMap && secondInOwnedMap);", policy)
        self.assertIn("return recorded && recordedTick == currentTick;", policy)
        self.assertIn("return id != std::thread::id() && id == std::this_thread::get_id();", policy)

    def test_t1_zero_foreign_access_before_ownership(self):
        # Membership is decided by pointer identity in the owned map's list.
        contains = self.body("bool Contains(Player const* candidate) const", "\n        }\n")
        self.assertIn("return Policy::OwnedListContains(ownedMap->GetPlayers(), candidate);", contains)
        self.assertNotIn("candidate->", contains)
        self.assertLess(contains.index("if (anyPlayer)"), contains.index("ownedMap->GetPlayers()"))
        policy = strip_comments(read(POLICY))
        member = policy.split("bool OwnedListContains(PlayerList const& players, Candidate const* candidate)", 1)[1]
        member = member.split("\n    }\n", 1)[0]
        self.assertIn("if (ref.GetSource() == candidate)", member)
        self.assertNotIn("candidate->", member)
        self.assertNotIn("ref.GetSource()->", member)
        # Off the world thread, resolution uses only the owned map's list.
        resolve = self.body("Player* Resolve(ObjectGuid const& guid) const", "\n        }\n")
        self.assertRegex(resolve, r"if \(anyPlayer\)\s*return ObjectAccessor::FindPlayer\(guid\);")
        self.assertEqual(resolve.count("ObjectAccessor::"), 1)
        self.assertIn("return Policy::OwnedListResolve<Player>(ownedMap->GetPlayers(), guid);", resolve)
        guard = self.body("bool MayReadPair(ReadScope const& scope, Player const* a, Player const* b)")
        self.assertNotRegex(guard, r"\b[ab]->")
        self.assertIn("scope.Contains(a), scope.Contains(b)", guard)
        # Pair rule: nothing of either player before the guard.
        allow = self.body("bool AllowPair(")
        pre = allow[:allow.index("MayReadPair(")]
        self.assertNotRegex(pre, r"\b(initiator|other)->")
        self.assertRegex(allow, r"if \(!MayReadPair\(CurrentReadScope\(\), initiator, other\)\)\s*return true;")
        self.assertLess(allow.index("MayReadPair("), allow.index("ReadHuman("))
        # Hooks: arguments are passed straight through, never dereferenced first.
        for sig, names in (("bool OnPlayerCanUseChat(Player* player", ("player", "receiver")),
                           ("bool OnPlayerCanInitTrade(Player* player", ("player", "target")),
                           ("bool OnPlayerCanGroupInvite(Player* player", ("player", "invitee"))):
            hook = self.body(sig, "\n        }\n")
            for name in names:
                self.assertNotIn(name + "->", hook, sig)
        invite = self.body("bool OnPlayerCanGroupInvite(Player* player", "\n        }\n")
        self.assertIn("ObjectAccessor::FindPlayerByName(membername, false)", invite)  # no IsInWorld read
        # Accept: the acceptor must be ours before anything of it is read.
        accept = self.body("bool OnPlayerCanGroupAccept(Player* player, Group* group) override", "\n        }\n")
        self.assertRegex(accept, r"if \(!scope\.Contains\(player\)\)\s*return true;")
        self.assertNotIn("player->", accept)
        self.assertIn("Player* leader = scope.Resolve(group->GetLeaderGUID());", accept)
        self.assertNotIn("FindPlayerByLowGUID", self.src)
        scan = self.body("bool AllowGroupMembers(", "\n        }\n")
        self.assertIn("Player* member = scope.Resolve(slot.guid);", scan)
        self.assertNotIn("ObjectAccessor::", scan)

    def test_no_world_hiding_or_mutation(self):
        for token in ("OnF14ShouldSuppressPlayerForViewer", "SuppressCreature", "SetPhaseMask", "UpdateObjectVisibility",
                      "DestroyForPlayer", "ForceUpdateProgressionState", "UpdateProgressionState",
                      "RewardQuest", "RemoveRewardedQuest", "sRandomPlayerbotMgr", "Playerbot", "Database",
                      "QueryResult", "const_cast", "GetEffectiveProgression", "isNormalAccount", "isBotAccount"):
            self.assertNotIn(token, self.src, token)

    def test_identity_never_race_alone(self):
        self.assertIn("HasF14VerifiedRandomBotIdentity(player)", self.src)
        self.assertIn("HasF14VerifiedRandomBotGuid(targetGuid)", self.src)
        policy = strip_comments(read(POLICY))
        bot = policy.split("constexpr bool BotIsRestrictedRandomBot(", 1)[1].split("}", 1)[0]
        for token in ("b.verifiedRandomBotIdentity", "b.headlessSession", "!b.gameMasterMode", "IsTbcRace(b.race)"):
            self.assertIn(token, bot)
        who = policy.split("constexpr bool ExcludeFromWho(", 1)[1].split("}", 1)[0]
        self.assertIn("targetGuidVerifiedRandomBot", who)

    def test_reply_is_a_direct_packet_not_a_whisper(self):
        self.assertNotRegex(self.src, r"\bWhisper\(")
        reply = self.src.split("void SendInCharacterReply(", 1)[1].split("\n    }\n", 1)[0]
        self.assertIn("ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_UNIVERSAL, bot, human, text);", reply)
        self.assertIn("human->SendDirectMessage(&data);", reply)
        self.assertLess(reply.index("MayReply("), reply.index("BuildChatPacket("))
        # Operator text is normalized (one line, bounded) right before the sink.
        self.assertIn("Policy::NormalizeReply(sIndividualProgression->GetF14SocialIsolationReply())", reply)
        self.assertLess(reply.index("NormalizeReply("), reply.index("BuildChatPacket("))
        self.assertLess(reply.index("if (text.empty())"), reply.index("BuildChatPacket("))
        self.assertIn("constexpr std::size_t MaxReplyBytes = 512;", read(POLICY))
        self.assertIn("std::lock_guard<std::mutex> lock(ReplyLock());", reply)

    def test_stage_mirror_matches_ip(self):
        mirror = self.src.split("uint8 ReadRawStage(Player const* player)", 1)[1].split("\n    }\n", 1)[0]
        self.assertIn("for (uint8 i = PROGRESSION_MOLTEN_CORE; i <= PROGRESSION_WOTLK_TIER_5; ++i)", mirror)
        self.assertIn("player->GetQuestStatus(66000 + i) == QUEST_STATUS_REWARDED", mirror)
        self.assertIn("return 255;", mirror)  # out of world = unknown, never Vanilla
        ip = read("src/IndividualProgression.cpp").split(
            "uint8 IndividualProgression::GetPlayerProgressionFromQuests(Player* player) const", 1)[1][:500]
        self.assertIn("for (uint8 i = PROGRESSION_MOLTEN_CORE; i <= PROGRESSION_WOTLK_TIER_5; ++i)", ip)
        self.assertIn("uint32 PROGRESSION_QUEST = 66000 + i;", ip)
        self.assertIn("QUEST_STATUS_REWARDED", ip)

    def test_group_accept_scans_every_member(self):
        accept = self.src.split("bool OnPlayerCanGroupAccept(", 1)[1].split("\n        }\n", 1)[0]
        # 1. The leader pair rule is kept, in both directions.
        self.assertIn("if (leader && !AllowPair(Policy::Surface::GroupAccept, player, leader, false))", accept)
        self.assertLess(accept.index("AllowPair("), accept.index("AllowGroupMembers(scope, player, group)"))
        scan = self.src.split("bool AllowGroupMembers(", 1)[1].split("\n        }\n", 1)[0]
        # 2. Only a human still in Vanilla is checked; every current member is read.
        self.assertIn("if (!Policy::HumanStillVanilla(facts))", scan)
        self.assertIn("group->GetMemberSlots()", scan)
        self.assertIn("Player* member = scope.Resolve(slot.guid);", scan)
        # 3. Unresolved members are skipped, never classified from the slot.
        self.assertRegex(scan, r"if \(!member\)\s*continue;")
        self.assertIn("bots.push_back(ReadBot(member));", scan)
        for token in ("getRace", "slot.name", "slot.flags", "IsTbcRace", "RACE_"):
            self.assertNotIn(token, scan, token)
        # 4. The same decision and reply rule as every other surface.
        self.assertIn("Policy::FirstRestrictedGroupMember(true, facts, bots)", scan)
        self.assertIn("Policy::Direction::HumanToBot, facts, bots[index], false)", scan)
        self.assertIn("SendInCharacterReply(players[index], human);", scan)
        policy = strip_comments(read(POLICY))
        first = policy.split("std::size_t FirstRestrictedGroupMember(", 1)[1].split("\n    }\n", 1)[0]
        self.assertIn("if (IsRestrictedPair(optionEnabled, h, b))", first)

    def test_default_reply_has_no_technical_words(self):
        policy = read(POLICY)
        text = re.search(r'DefaultReply =\s*"([^"]+)"', policy).group(1)
        conf = read("conf/individualProgression.conf.dist")
        self.assertIn('"' + text + '"', conf)
        for word in ("tier", "stage", "server", "expansion", "tbc", "vanilla", "level", "progression", "era"):
            self.assertNotRegex(text.lower(), r"\b" + word + r"\b", word)


class IdentitySources(unittest.TestCase):
    """Two different, deliberate identity notions (see the doc's identity-source section).

    Restriction (this feature) must use only the positive verified cache: a
    false positive would cut a real human or Altbot off. Group sync writes to
    every non-normal account; a protective era floor for that sync must cover
    that whole write population, or a Blood Elf/Draenei Altbot could still be
    synced down to Vanilla. If either side changes, this test makes it visible.
    """

    def test_restriction_uses_only_the_verified_cache(self):
        src = strip_comments(read(SCRIPT))
        self.assertEqual(src.count("HasF14VerifiedRandomBotIdentity("), 2)  # ReadHuman, ReadBot
        for token in ("isNormalAccount", "isBotAccount", "isExcludedAccount", "IsRandomBot"):
            self.assertNotIn(token, src, token)

    def test_group_sync_write_population_is_every_non_normal_account(self):
        core = strip_comments(read("src/IndividualProgression.cpp"))
        sync = core.split("void IndividualProgression::SyncBotsProgressionToLeader(Group* group)", 1)[1]
        sync = sync.split("\nvoid IndividualProgression::", 1)[0]
        self.assertRegex(sync, r"if \(!member \|\| isNormalAccount\(member\)\)\s*continue;")
        self.assertIn("ForceUpdateProgressionState(member, static_cast<ProgressionState>(permittedState));", sync)
        normal = core.split("bool IndividualProgression::isNormalAccount(Player* player)", 1)[1][:300]
        self.assertIn("isExcludedAccount(player) || sIndividualProgression->isBotAccount(player)", normal)


class GroupFloorPopulation(unittest.TestCase):
    """CS01 6858469463: existing sync population first, race floor second.

    Pins that the sync population is decided by account match before any
    race is read, that race never widens it, and the shipped account
    regexes (the running server's values are UNKNOWN).
    """

    def sync_loop(self):
        core = strip_comments(read("src/IndividualProgression.cpp"))
        sync = core.split("void IndividualProgression::SyncBotsProgressionToLeader(Group* group)", 1)[1]
        sync = sync.split("\nvoid IndividualProgression::", 1)[0]
        return sync.split("for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())", 1)[1]

    def test_population_gate_precedes_any_race_read(self):
        loop = self.sync_loop()
        gate = loop.index("if (!member || isNormalAccount(member))")
        self.assertEqual(len(re.findall(r"isNormalAccount\(member\)", loop)), 1)
        for race in re.finditer(r"getRace\(\)", loop):
            self.assertGreater(race.start(), gate)
        # Race is used only for the enforcement-ON group-era check, never to
        # add a member to the population.
        self.assertNotRegex(loop, r"(RACE_BLOODELF|RACE_DRAENEI|IsExpansionRace)[^;]*isNormalAccount")
        self.assertNotIn("HasF14VerifiedRandomBot", loop[:gate])

    def test_population_is_account_name_based(self):
        core = strip_comments(read("src/IndividualProgression.cpp"))
        normal = core.split("bool IndividualProgression::isNormalAccount(Player* player)", 1)[1][:300]
        self.assertIn("isExcludedAccount(player) || sIndividualProgression->isBotAccount(player)", normal)
        for fn in ("bool IndividualProgression::isBotAccount(uint32 accountId)",
                   "bool IndividualProgression::isExcludedAccount(uint32 accountId)"):
            body = core.split(fn, 1)[1][:200]
            self.assertIn("GetAccountMatch(accountId).", body, fn)
            self.assertNotIn("getRace", body, fn)

    def test_shipped_account_regex_defaults(self):
        conf = read("conf/individualProgression.conf.dist")
        self.assertRegex(conf, r'(?m)^IndividualProgression\.BotAccountsRegex\s*=\s*"\^RNDBOT\.\*"\s*$')
        self.assertRegex(conf, r'(?m)^IndividualProgression\.ExcludedAccountsRegex\s*=\s*""\s*$')


class ExactInsertions(unittest.TestCase):
    def test_player_cpp_one_or_reason(self):
        plus, minus = added_lines("src/IndividualProgressionPlayer.cpp")
        # Inserted before the pinned last reason so the #80 gate pin holds.
        self.assertEqual(minus, [])
        self.assertEqual(plus, ["             sIndividualProgression->f14RandomBotSocialIsolation ||"])

    def test_core_and_header_additions_only(self):
        for name in ("src/IndividualProgression.cpp", "src/IndividualProgression.h"):
            plus, minus = added_lines(name)
            self.assertEqual(minus, [], name)
            joined = "\n".join(plus)
            for token in ("ForceUpdateProgressionState", "RewardQuest", "enforceGroupRules ="):
                self.assertNotIn(token, joined, name)
        plus, _ = added_lines("src/IndividualProgression.cpp")
        self.assertIn("    AddSC_mod_individual_progression_f14_social_isolation();", plus)

    def test_existing_ip_hooks_untouched(self):
        cur = read("src/IndividualProgressionPlayer.cpp")
        old = parent("src/IndividualProgressionPlayer.cpp")
        for sig in ("bool OnPlayerCanGroupInvite(Player* player, std::string& membername) override",
                    "bool OnPlayerCanGroupAccept(Player* player, Group* group) override"):
            self.assertEqual(cur.split(sig, 1)[1][:3000], old.split(sig, 1)[1][:3000], sig)

    def test_every_other_parent_file_is_byte_identical(self):
        tracked = git("ls-tree", "-r", "--name-only", PARENT).decode().splitlines()
        self.assertGreater(len(tracked), 50)
        for name in tracked:
            if name not in SCOPE:
                self.assertEqual((ROOT / name).read_bytes(), git("show", PARENT + ":" + name), name)

    def test_no_experiment_macro_defined(self):
        for path in sorted((ROOT / "src").rglob("*")):
            if path.is_file():
                text = path.read_text(encoding="utf-8", errors="replace")
                self.assertNotRegex(text, r"#\s*define\s+COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", path.name)
                self.assertNotRegex(text, r"#\s*define\s+COSMIC_F14_WHO_GUID_QUERY_EXPERIMENT", path.name)


if __name__ == "__main__":
    unittest.main()
