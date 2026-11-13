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
        allow = src.split("bool AllowPairIn(", 1)[1]
        self.assertIn("if (!OptionOn() || !initiator || !other || initiator == other)", allow[:400])
        for sig in ("bool OnPlayerCanGroupInvite(", "bool OnPlayerCanGroupAccept(",
                    "bool OnF14ShouldExcludeWhoEntryForViewer("):
            body = src.split(sig, 1)[1][:400]
            self.assertIn("!OptionOn()", body, sig)


class HooksAndAuthority(unittest.TestCase):
    def setUp(self):
        self.src = strip_comments(read(SCRIPT))

    def test_only_social_hooks_and_who(self):
        hooks = sorted(set(re.findall(r"\b(PLAYERHOOK_[A-Z0-9_]+|WORLDOBJECTHOOK_[A-Z0-9_]+|WORLDHOOK_[A-Z0-9_]+|GLOBALHOOK_[A-Z0-9_]+|ALLBATTLEGROUNDHOOK_[A-Z0-9_]+|ALLSPELLHOOK_[A-Z0-9_]+)\b", self.src)))
        self.assertEqual(hooks, sorted([
            "PLAYERHOOK_CAN_GROUP_ACCEPT", "PLAYERHOOK_CAN_GROUP_INVITE", "PLAYERHOOK_CAN_INIT_TRADE",
            "PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT", "WORLDOBJECTHOOK_ON_F14_WHO_GUID_EXCLUSION_QUERY",
            "WORLDHOOK_ON_UPDATE", "PLAYERHOOK_ON_BEFORE_UPDATE",
            "PLAYERHOOK_CAN_JOIN_LFG", "PLAYERHOOK_ON_LOGOUT", "GLOBALHOOK_CAN_CREATE_LFG_PROPOSAL",
            "PLAYERHOOK_CAN_JOIN_IN_BATTLEGROUND_QUEUE", "PLAYERHOOK_CAN_BATTLEFIELD_PORT",
            "ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_ADD_PLAYER", "ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_REMOVE_PLAYER_AT_LEAVE",
            "ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY", "ALLBATTLEGROUNDHOOK_CAN_ADD_GROUP_TO_MATCHING_POOL",
            "PLAYERHOOK_CAN_SEND_MAIL", "ALLSPELLHOOK_ON_SPELL_CHECK_CAST", "PLAYERHOOK_ON_F14_CAN_GUILD_INVITE",
            "PLAYERHOOK_ON_LOGIN"]))
        overrides = sorted(re.findall(r"\b(On\w+)\([^;{]*\)\s*override", self.src))
        self.assertEqual(overrides, sorted([
            "OnF14ShouldExcludeWhoEntryForViewer", "OnPlayerCanGroupAccept", "OnPlayerCanGroupInvite",
            "OnPlayerCanInitTrade", "OnPlayerCanUseChat", "OnUpdate", "OnPlayerBeforeUpdate",
            "OnPlayerCanJoinLfg", "OnPlayerLogout", "OnPlayerCanJoinInBattlegroundQueue",
            "OnPlayerCanBattleFieldPort", "OnBattlegroundAddPlayer", "OnBattlegroundRemovePlayerAtLeave",
            "OnBattlegroundDestroy", "OnPlayerCanSendMail", "OnSpellCheckCast", "OnPlayerCanGuildInvite", "OnPlayerLogin"]))
        # The guild hook is overridden only inside the patched-Core guard.
        self.assertEqual(re.findall(r"\bbool (OnPlayerCanGuildInvite)\(Player\* inviter, Player\* invitee\) override", self.src),
                         ["OnPlayerCanGuildInvite"])
        self.assertEqual(sorted(re.findall(r"\b(Can\w+)\([^;{]*\)\s*override", self.src)),
                         ["CanAddGroupToMatchingPool", "CanCreateLfgProposal"])

    def test_battlegrounds_option_c(self):
        # (b) Queue join: world thread only; quiet refusal; only a solo
        # restricted bot is refused; group joins are left to (a).
        join = self.body("bool OnPlayerCanJoinInBattlegroundQueue(Player* player", "\n        }\n")
        self.assertRegex(join, r"if \(!OptionOn\(\) \|\| !player \|\| !WorldThread\(\)\.IsCurrent\(\)\)\s*return true;")
        self.assertLess(join.index("WorldThread().IsCurrent()"), join.index("ReadHuman("))
        self.assertRegex(join, r"if \(!restricted \|\| !solo\)\s*return true;")
        self.assertRegex(join, r"err = ERR_BATTLEGROUND_NONE;[^\n]*\n\s*return false;")
        # Exactly one refusal; the only other `return false;` is inside the
        # bracket-matching lambda (a non-match, not a refusal).
        lam = join.split("[&](Policy::BgQueuedHuman const& where)", 1)[1].split("});", 1)[0]
        self.assertEqual(lam.count("return false;"), 1)
        self.assertEqual(join.count("return false;") - lam.count("return false;"), 1)
        self.assertIn("Policy::HumanStillVanilla(ReadHuman(human))", join)  # live re-validation
        self.assertIn("InBattlegroundQueueForBattlegroundQueueType", join)
        # (a) Enter battle: only "enter" is checked; ownership before any read;
        # only the bot's own state plus the value roster.
        port = self.body("bool OnPlayerCanBattleFieldPort(Player* player", "\n        }\n")
        self.assertRegex(port, r"if \(!OptionOn\(\) \|\| !player \|\| action != 1\)\s*return true;")
        self.assertRegex(port, r"if \(!CurrentReadScope\(\)\.Contains\(player\)\)\s*return true;")
        self.assertLess(port.index("Contains(player)"), port.index("ReadBot(player)"))
        for token in ("ObjectAccessor", "ReadHuman", "sBattlegroundMgr", "GetBattleground"):
            self.assertNotIn(token, port, token)
        self.assertEqual(set(re.findall(r"\b(\w+)->", port)), {"player"})
        self.assertIn("player->IsInvitedForBattlegroundInstance(instance)", port)
        # Roster: humans are read only on the world thread; leaving and
        # destruction use values only.
        add = self.body("void OnBattlegroundAddPlayer(Battleground* bg, Player* player) override", "\n        }\n")
        self.assertRegex(add, r"if \(!OptionOn\(\) \|\| !bg \|\| !player \|\| !WorldThread\(\)\.IsCurrent\(\)\)\s*return;")
        self.assertLess(add.index("IsCurrent()"), add.index("ReadHuman("))
        for sig in ("void OnBattlegroundRemovePlayerAtLeave(Battleground* bg, Player* player) override",
                    "void OnBattlegroundDestroy(Battleground* bg) override"):
            body = self.body(sig, "\n        }\n")
            for token in ("ReadHuman", "ReadBot", "ObjectAccessor", "GetQuestStatus"):
                self.assertNotIn(token, body, sig)
        logout = self.body("void OnPlayerLogout(Player* player) override", "\n        }\n")
        self.assertIn("BgQueued().Erase(player->GetGUID().GetRawValue());", logout)
        policy = strip_comments(read(POLICY))
        self.assertIn("return optionEnabled && restrictedBot && soloJoin && matchingVanillaHumanQueued;", policy)
        self.assertIn("return optionEnabled && restrictedBot && enteringBattle && invitedToInstanceWithVanillaHuman;", policy)

    def test_battleground_matcher_option_d(self):
        # The matcher hook: world thread only, read-only (never mutates the
        # queue, pools or battleground), and refuses only by the pure rule.
        m = self.body("bool CanAddGroupToMatchingPool(BattlegroundQueue* queue, GroupQueueInfo* group", "\n        }\n")
        self.assertRegex(m, r"if \(!OptionOn\(\) \|\| !queue \|\| !group \|\| !WorldThread\(\)\.IsCurrent\(\)\)\s*return true;")
        self.assertLess(m.index("IsCurrent()"), m.index("KindsOfGroup("))
        for token in ("push_back", "erase", "Remove", "KickGroup", "InviteGroupToBG", "AddGroup(", "clear(",
                      "SetBattleground", "Teleport"):
            self.assertNotIn(token, m, token)
        self.assertIn("queue->m_SelectionPools", m)
        self.assertIn("bg->GetPlayers()", m)
        self.assertIn("invited->IsInvitedToBGInstanceGUID == bg->GetInstanceID()", m)
        # Premade: compared with the opposite premade Core will pair it with.
        self.assertIn("if (GroupQueueInfo const* peer = PremadePeer(*queue, group, bracketId))", m)
        peer = self.body("GroupQueueInfo const* PremadePeer(BattlegroundQueue const& queue, GroupQueueInfo const* group,")
        self.assertIn("queue.m_QueuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE + side]", peer)
        self.assertIn("queue.m_QueuedGroups[bracketId][BG_QUEUE_PREMADE_ALLIANCE + (1 - side)]", peer)
        self.assertIn("if (other && !other->IsInvitedToBGInstanceGUID)", peer)
        self.assertRegex(m, r"if \(!candidate\.Any\(\)\)\s*return true;")
        self.assertIn("return !Policy::MatchPoolRejects(true, candidate, others);", m)
        self.assertEqual(len(re.findall(r"\breturn\b[^;]*;", m)), 3)
        # Facts come from the live verified predicates, never race alone.
        kinds = self.body("Policy::MatchKinds KindsOfGuid(ObjectGuid const& guid)")
        self.assertIn("return Policy::KindsOf(ReadHuman(player), ReadBot(player));", kinds)
        self.assertRegex(kinds, r"if \(!player\)\s*return \{\};")
        policy = strip_comments(read(POLICY))
        self.assertIn("((candidate.restrictedBot && others.stillVanillaHuman) ||", policy)
        self.assertIn("(candidate.stillVanillaHuman && others.restrictedBot));", policy)
        self.assertIn("return MatchKinds{HumanStillVanilla(asHuman), BotIsRestrictedRandomBot(asBot)};", policy)

    def test_duel_and_mail(self):
        duel = self.body("void OnSpellCheckCast(Spell* spell, bool ", "\n        }\n")
        # Only duel requests, only when Core has not already refused.
        self.assertRegex(duel, r"if \(res != SPELL_CAST_OK \|\| !OptionOn\(\) \|\| !spell\)\s*return;")
        self.assertRegex(duel, r"if \(!info \|\| !info->HasEffect\(SPELL_EFFECT_DUEL\)\)\s*return;")
        # Off the world thread the scope is the caster's own map; the target
        # is resolved only by pointer identity among that map's players.
        self.assertIn("scope.ownedMap = caster->FindMap();", duel)
        self.assertIn("if (static_cast<Unit const*>(owned) == targetUnit)", duel)
        off = duel.split("else\n                for (auto const& ref : scope.ownedMap->GetPlayers())", 1)
        self.assertEqual(len(off), 2)
        self.assertNotRegex(duel, r"\btargetUnit->(?!ToPlayer)")
        self.assertLess(duel.index("if (scope.anyPlayer)\n                target = targetUnit->ToPlayer();"),
                        duel.index("scope.ownedMap->GetPlayers()"))
        self.assertIn("if (!AllowPairIn(scope, Policy::Surface::Duel, caster, target, false))", duel)
        self.assertIn("res = SPELL_FAILED_BAD_TARGETS;", duel)
        mail = self.body("bool OnPlayerCanSendMail(Player* player", "\n        }\n")
        self.assertRegex(mail, r"if \(!OptionOn\(\) \|\| !player \|\| !WorldThread\(\)\.IsCurrent\(\)\)\s*return true;")
        self.assertRegex(mail, r"if \(receiver\)\s*return AllowPair\(Policy::Surface::Mail, player, receiver, false\);")
        # Offline: only a bot confirmed earlier in this run; no reply, no race.
        off = mail.split("return AllowPair(Policy::Surface::Mail, player, receiver, false);", 1)[1]
        self.assertIn("Confirmed().Contains(receiverGuid.GetRawValue())", off)
        self.assertIn("Policy::OfflineMailRefused(true, Policy::HumanStillVanilla(ReadHuman(player)), confirmed)", off)
        for token in ("SendInCharacterReply", "getRace", "isBotAccount", "IsTbcRace"):
            self.assertNotIn(token, off, token)
        logout = self.body("void OnPlayerLogout(Player* player) override", "\n        }\n")
        self.assertRegex(logout, r"if \(OptionOn\(\) && WorldThread\(\)\.IsCurrent\(\) && Policy::BotIsRestrictedRandomBot\(ReadBot\(player\)\)\)")
        self.assertIn("Confirmed().Remember(player->GetGUID().GetRawValue());", logout)
        login = self.body("void OnPlayerLogin(Player* player) override", "\n        }\n")
        self.assertIn("Confirmed().Forget(player->GetGUID().GetRawValue());", login)
        # The capture relies on load order: these scripts are registered
        # (via AddSC_mod_individual_progression) BEFORE IP's player script,
        # whose logout forgets the verified identity.
        loader = read("src/IndividualProgression_loader.cpp")
        self.assertLess(loader.index("    AddSC_mod_individual_progression();"),
                        loader.index("    AddSC_mod_individual_progression_player();"))
        core = read("src/IndividualProgression.cpp")
        reg = core.split("void AddSC_mod_individual_progression()", 1)[1].split("}", 1)[0]
        self.assertIn("AddSC_mod_individual_progression_f14_social_isolation();", reg)
        player = read("src/IndividualProgressionPlayer.cpp")
        out = player.split("void OnPlayerLogout(Player* player) override", 1)[1][:200]
        self.assertIn("ForgetF14VerifiedRandomBotGroupIdentity(player);", out)

    def test_guild_invite_only_with_patched_core(self):
        guard = "#if defined(COSMIC_F14_GUILD_INVITE_HOOK_VERSION) && COSMIC_F14_GUILD_INVITE_HOOK_VERSION >= 1"
        raw = read(SCRIPT)
        self.assertEqual(raw.count(guard), 2)  # the class and its registration
        for block in raw.split(guard)[1:]:
            self.assertIn("#endif", block)
            inside = block.split("#endif", 1)[0]
            self.assertTrue("F14SocialIsolationGuildScript" in inside)
        cls = raw.split(guard, 1)[1].split("#endif", 1)[0]
        self.assertIn("{PLAYERHOOK_ON_F14_CAN_GUILD_INVITE}", cls)
        self.assertIn("return AllowPair(Policy::Surface::GuildInvite, inviter, invitee, false);", cls)
        reg = raw.split(guard, 2)[2].split("#endif", 1)[0]
        self.assertIn("new F14SocialIsolationGuildScript();", reg)
        # The module never defines the Core macro itself.
        self.assertNotRegex(raw, r"#\s*define\s+COSMIC_F14_GUILD_INVITE_HOOK_VERSION")
        self.assertIn("GuildInvite", strip_comments(read(POLICY)))

    def test_dungeon_finder_value_only(self):
        # The matcher hook runs on the LFG worker thread beside the maps: it
        # reads only GUIDs and the facts table, never a Player.
        veto = self.body("bool CanCreateLfgProposal(lfg::Lfg5Guids const& proposal) override", "\n        }\n")
        self.assertRegex(veto, r"if \(!OptionOn\(\)\)\s*return true;")
        for token in ("Player*", "Player const", "->", "ObjectAccessor", "ReadHuman", "ReadBot", "GetSession", "FindMap",
                      "sIndividualProgression->Has"):
            self.assertNotIn(token, veto, token)
        self.assertIn("guids.push_back(guid.GetRawValue());", veto)
        # A full proposal has no roles map: the raw GUIDs (players AND party
        # group GUIDs) are always passed, never filtered to players only.
        self.assertNotIn("IsPlayer()", veto)
        self.assertRegex(veto, r"for \(ObjectGuid const& guid : proposal\.guids\)\s*if \(guid\)\s*guids\.push_back")
        pass
        self.assertIn("std::lock_guard<std::mutex> lock(QueueFactsLock());", veto)
        self.assertIn("return !QueueFacts().Rejects(true, guids);", veto)
        # Facts are recorded only on the world thread, and joining is never blocked.
        join = self.body("bool OnPlayerCanJoinLfg(Player* player", "\n        }\n")
        self.assertRegex(join, r"if \(!OptionOn\(\) \|\| !player \|\| !WorldThread\(\)\.IsCurrent\(\)\)\s*return true;")
        self.assertLess(join.index("WorldThread().IsCurrent()"), join.index("ReadHuman("))
        self.assertEqual(set(re.findall(r"\breturn\b[^;]*;", join)), {"return true;"})  # never blocks a queue
        self.assertIn("Policy::MakeQueueFacts(ReadHuman(player), ReadBot(player))", join)
        self.assertIn("group->GetFirstMember()", join)
        self.assertIn("QueueFacts().RecordGroup(groupGuid, std::move(members));", join)
        logout = self.body("void OnPlayerLogout(Player* player) override", "\n        }\n")
        self.assertIn("QueueFacts().Erase(player->GetGUID().GetRawValue());", logout)
        policy = strip_comments(read(POLICY))
        self.assertIn("return optionEnabled && anyStillVanillaHuman && anyRestrictedBot;", policy)
        self.assertIn("return QueueFacts{BotIsRestrictedRandomBot(asBot), HumanStillVanilla(asHuman)};", policy)

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
        # FindMap only on an owned player: the one being updated, or the
        # caster whose own map update is processing the spell.
        self.assertEqual(sorted(re.findall(r"\b(\w+)->FindMap\(\)", self.src)), ["caster", "player"])
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
        allow = self.body("bool AllowPairIn(")
        pre = allow[:allow.index("MayReadPair(")]
        self.assertNotRegex(pre, r"\b(initiator|other)->")
        self.assertRegex(allow, r"if \(!MayReadPair\(scope, initiator, other\)\)\s*return true;")
        wrapper = self.body("bool AllowPair(Policy::Surface surface")
        self.assertIn("return AllowPairIn(CurrentReadScope(), surface, initiator, other, addonMessage);", wrapper)
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
        # CS01 6865093100: the global name map only on the world thread;
        # off it, the name is resolved only among the owned map's players.
        self.assertIn("Player* invitee = scope.anyPlayer ? ObjectAccessor::FindPlayerByName(membername, false)\n"
                      "                                              : scope.ResolveByName(membername);", invite)
        self.assertIn("return AllowPairIn(scope, Policy::Surface::GroupInvite, player, invitee, false);", invite)
        byname = self.body("Player* ResolveByName(std::string const& name) const", "\n        }\n")
        self.assertIn("ownedMap->GetPlayers()", byname)
        self.assertNotIn("ObjectAccessor", byname)
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
                self.assertNotRegex(text, r"#\s*define\s+COSMIC_F14_GUILD_INVITE_HOOK_VERSION", path.name)


if __name__ == "__main__":
    unittest.main()
