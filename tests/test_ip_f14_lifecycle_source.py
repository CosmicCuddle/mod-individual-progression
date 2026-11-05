"""F14 attack/corpse lifecycle consumer: source contracts, not in-game proof."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


KEY = "IndividualProgression.F14.ExperimentalAttackLifecycleRegistry"
SCRIPT = "src/IndividualProgressionF14LifecycleScript.cpp"
REGISTRY = "src/IndividualProgressionF14LifecycleRegistry.h"


class DefaultOff(unittest.TestCase):
    def test_conf_header_and_fallback_are_off(self):
        conf = read("conf/individualProgression.conf.dist")
        self.assertEqual(len(re.findall(r"^" + re.escape(KEY) + r"\s*=\s*0\s*$", conf, re.M)), 1)
        self.assertNotRegex(conf, r"(?m)^" + re.escape(KEY) + r"\s*=\s*[1-9]")
        header = read("src/IndividualProgression.h")
        self.assertIn("bool f14ExperimentalAttackLifecycleRegistry = false;", header)
        core = read("src/IndividualProgression.cpp")
        self.assertIn('"' + KEY + '", false)', core)

    def test_forced_off_without_hook_or_playerbots_only(self):
        core = read("src/IndividualProgression.cpp")
        start = core.index('"' + KEY + '", false)')
        part = core[start:core.index("sIndividualProgression->fishingFix", start)]
        self.assertIn("!defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION) || COSMIC_F14_ATTACK_EVENT_HOOK_VERSION < 1", part)
        self.assertIn('#if !__has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")', part)
        self.assertEqual(part.count("f14ExperimentalAttackLifecycleRegistry = false;"), 2)
        self.assertIn("sIndividualProgression->ResetF14LifecycleRegistry();", part)
        # Independent opt-in: no coupling to unrelated group/observer features.
        self.assertNotIn("f14ProtectVerifiedRandomBotGroupFloor", part)
        self.assertNotIn("f14ExperimentalRndbotPlayerVisibility", part)
        self.assertNotIn("requires ProtectVerifiedRandomBotGroupFloor", core)
        # The forcing block comes AFTER both identity flags are final.
        self.assertGreater(start, core.index('"IndividualProgression.F14.ExperimentalRndbotPlayerVisibility", false)'))
        self.assertGreater(start, core.index('"IndividualProgression.F14.ProtectVerifiedRandomBotGroupFloor", false)'))

    def test_refresh_bridge_never_defined_here(self):
        for path in sorted((ROOT / "src").rglob("*")):
            if path.suffix in (".h", ".cpp"):
                text = path.read_text(encoding="utf-8", errors="replace")
                self.assertNotRegex(text, r"#\s*define\s+COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION", str(path))
                self.assertNotRegex(text, r"#\s*define\s+COSMIC_F14_ATTACK_EVENT_HOOK_VERSION", str(path))


class ConsumerIsBookkeepingOnly(unittest.TestCase):
    def test_hook_class_only_with_core_prototype(self):
        src = read(SCRIPT)
        guard = "#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION) && COSMIC_F14_ATTACK_EVENT_HOOK_VERSION >= 1"
        self.assertEqual(src.count(guard), 2)
        cls = src.index("class IndividualProgressionF14LifecycleScript : public UnitScript")
        self.assertLess(src.index(guard), cls)
        self.assertLess(cls, src.index("#endif", cls))
        for hook in ("UNITHOOK_ON_F14_ATTACK_TARGET_TRANSITION",
                     "UNITHOOK_ON_F14_CREATURE_CORPSE_READY",
                     "UNITHOOK_ON_F14_CREATURE_RESPAWNED"):
            self.assertEqual(src.count(hook), 1)

    def test_no_side_effects_playerbots_or_visibility(self):
        code = strip_comments(read(SCRIPT)) + strip_comments(read(REGISTRY))
        for forbidden in ("sRandomPlayerbotMgr", "RandomPlayerbotMgr", "ObjectAccessor",
                          "UpdateObjectVisibility", "UpdateVisibility", "DestroyForPlayer",
                          "DestroyForNearbyPlayers", "SendUpdateToPlayer", "SetVisibility",
                          "SetPhaseMask(", "AddThreat(", "ClearThreat(", "SetLootRecipient(",
                          "SetVisible", "SetPhaseMask", "WorldPacket", "SendDirectMessage",
                          "ForceUpdateProgressionState", "UpdateProgressionState",
                          "SetPlayerSetting", "CharacterDatabase", "WorldDatabase",
                          "LoginDatabase", "RewardQuest", "ModifyMoney", "SaveToDB",
                          "IsConcealmentCandidate(", "BuildCreatureSnapshot(", "GetConservativeEvidence(", "ShouldConcealCreatureFromObserver", "Kill(", "DespawnOrUnsummon",
                          "CombatStop(", "AttackStop(", "Attack("):
            if forbidden in ("IsConcealmentCandidate(", "BuildCreatureSnapshot(", "GetConservativeEvidence(", "ShouldConcealCreatureFromObserver"):
                # Pure-header API only; never CALLED by the runtime consumer.
                self.assertNotIn(forbidden, strip_comments(read(SCRIPT)))
                continue
            if forbidden == "UpdateObjectVisibility":
                # Approved experimental successor to the ORIGINAL no-refresh
                # invariant: EXACTLY one call, in owning-map OnUnitUpdate
                # after LifecycleLock is released. Never inside the lifecycle
                # callback or logout event. The NEW source-contract suite
                # independently verifies this exclusive opt-in scope.
                script = strip_comments(read(SCRIPT))
                self.assertEqual(script.count("creature->UpdateObjectVisibility(true);"), 1)
                update = script.split("void OnUnitUpdate(", 1)[1]  # strip_comments removes /*diff*/
                update = update.split("\n    }", 1)[0]
                self.assertIn("creature->UpdateObjectVisibility(true);", update)
                self.assertLess(update.index("\n        }"), update.index("creature->UpdateObjectVisibility(true);"))
                continue
            self.assertNotIn(forbidden, code, forbidden)

    def test_attack_handler_checks_enable_first(self):
        src = strip_comments(read(SCRIPT))
        body = src.split("void OnF14AttackTargetTransition(", 1)[1]
        body = body[:body.index("void OnF14CreatureCorpseReady(")]
        first = body.index("{") + 1
        self.assertTrue(body[first:].lstrip().startswith("if (!LifecycleEnabled() || !attacker"))
        self.assertIn("return sIndividualProgression->enabled && sIndividualProgression->f14ExperimentalAttackLifecycleRegistry;", src)

    def test_no_retained_unit_pointers(self):
        src = strip_comments(read(SCRIPT))
        # No namespace-scope or static object of a game-object pointer type.
        self.assertNotRegex(src, r"\bstatic\s+(?:\w+::)*(Unit|Creature|Player|Map|WorldObject)\s*\*")
        self.assertNotRegex(src, r"std::(vector|set|unordered_set|unordered_map|map)<\s*(Unit|Creature|Player|Map|WorldObject)\s*\*")
        # The per-creature state is a value token in the creature's own CustomData.
        self.assertIn("struct F14CreatureIncarnation : public DataMap::Base", src)
        self.assertIn("std::uint64_t token = 0;", src)
        # Bounded, allocation-free attacker snapshot under the mutex.
        self.assertIn("std::array<std::uint64_t, Policy::MaxTrackedAttackers + 1> others{};", src)
        self.assertIn("if (otherCount == others.size())", src)
        for unbounded in ("std::vector", ".reserve(", "push_back(", "new std::", "malloc("):
            self.assertNotIn(unbounded, src)
        # Pure header never names game-object types at all.
        reg = strip_comments(read(REGISTRY))
        self.assertNotRegex(reg, r"\b(Unit|Creature|Player|Map|WorldObject|ObjectGuid)\b")
        includes = re.findall(r'#include\s+[<"]([^>"]+)[>"]', reg)
        self.assertEqual(sorted(includes), sorted([
            "IndividualProgressionF14CombatVisibilityPolicy.h",
            "IndividualProgressionF14MultiAttackerPolicy.h", "array", "cstddef", "cstdint",
            "functional", "limits", "unordered_map"]))

    def test_actor_identity_is_cache_only_and_players_only(self):
        src = strip_comments(read(SCRIPT))
        fn = src.split("std::uint64_t ActorTokenOf(Unit const* unit)", 1)[1].split("bool LifecycleEnabled()", 1)[0]
        self.assertIn("if (!unit || !unit->IsPlayer())", fn)
        self.assertIn("GetF14VerifiedRandomBotLoginEpoch(unit->GetGUID())", fn)
        # Pets/minions/charmed units of players are never world-creature entries.
        world = src.split("Creature* AsWorldCreature(Unit* unit)", 1)[1].split("Registry::CreatureKey KeyOf(", 1)[0]
        for token in ("IsPet()", "IsTotem()", "IsControlledByPlayer()", "GetCharmerOrOwnerPlayerOrPlayerItself()", "IsInWorld()"):
            self.assertIn(token, world)

    def test_every_registry_use_holds_the_mutex(self):
        src = strip_comments(read(SCRIPT))
        for fn in ("void IndividualProgression::ResetF14LifecycleRegistry()",
                   "void IndividualProgression::ExpireF14LifecycleActor(",
                   "IndividualProgression::F14LifecycleStatus IndividualProgression::GetF14LifecycleStatus()",
                   "void OnF14AttackTargetTransition(",
                   "static void RetireIncarnation(",
                   "void OnCreatureRemoveWorld(Creature* creature) override",
                   "void OnUnitEnterCombat(Unit* unit, Unit* victim) override"):
            body = src.split(fn, 1)[1]
            body = body[:body.index("\n}\n") if fn.startswith(("void Ind", "Ind")) else body.index("\n    }\n")]
            self.assertIn("std::lock_guard<std::mutex> guard(LifecycleLock());", body, fn)
            self.assertLess(body.index("std::lock_guard<std::mutex> guard(LifecycleLock());"),
                            body.index("LifecycleRegistry()"), fn)


class LockOrderAndRegistration(unittest.TestCase):
    def test_logout_expiry_runs_after_identity_lock_released(self):
        core = read("src/IndividualProgression.cpp")
        seg = core.split("void IndividualProgression::RememberF14VerifiedRandomBotGroupIdentity(", 1)[1].split(
            "void IndividualProgression::ForgetF14VerifiedRandomBotGroupIdentity(", 1)[0]
        lam_open = seg.index("[&]()")
        lock = seg.index("std::unique_lock<std::shared_mutex> lock(_f14GroupCacheLock);")
        lam_close = seg.index("}();")
        expire = seg.index("ExpireF14LifecycleActor(expiredLoginEpoch);")
        self.assertLess(lam_open, lock)
        self.assertLess(lock, lam_close)
        self.assertLess(lam_close, expire)
        self.assertEqual(seg.count("ExpireF14LifecycleActor("), 1)
        # Old epoch captured under the lock before it is erased.
        self.assertLess(seg.index("expiredLoginEpoch = previousEpoch->second;"),
                        seg.index("_f14VerifiedRandomBotLoginEpoch.erase(guid);"))
        self.assertNotIn("sRandomPlayerbotMgr", seg)

    def test_registered_without_touching_upstream_loader(self):
        core = read("src/IndividualProgression.cpp")
        reg = core.split("void AddSC_mod_individual_progression()", 1)[1]
        self.assertIn("AddSC_mod_individual_progression_f14_lifecycle();", reg[:reg.index("}")])
        self.assertNotIn("f14_lifecycle", read("src/IndividualProgression_loader.cpp"))
        src = read(SCRIPT)
        self.assertIn("void AddSC_mod_individual_progression_f14_lifecycle()", src)
        tail = src.split("void AddSC_mod_individual_progression_f14_lifecycle()", 1)[1]
        self.assertIn("#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION)", tail)
        self.assertIn("new IndividualProgressionF14LifecycleScript();", tail)

    def test_removal_uses_existing_core_hook_and_always_retires(self):
        src = strip_comments(read(SCRIPT))
        self.assertIn('#include "AllCreatureScript.h"', read(SCRIPT))
        cls = src.split("class IndividualProgressionF14LifecycleRemovalScript : public AllCreatureScript", 1)[1]
        body = cls.split("void OnCreatureRemoveWorld(Creature* creature) override", 1)[1]
        body = body[:body.index("\n    }\n")]
        self.assertIn("OnCreatureRemovedFromWorld(KeyOf(creature), stamp);", body)
        self.assertIn("SetStamp(creature, 0);", body)
        self.assertIn("if (!stamp)", body)
        self.assertNotIn("f14ExperimentalAttackLifecycleRegistry", body)
        tail = src.split("void AddSC_mod_individual_progression_f14_lifecycle()", 1)[1]
        self.assertIn("new IndividualProgressionF14LifecycleRemovalScript();", tail)
        guard = tail.index("#if defined(COSMIC_F14_ATTACK_EVENT_HOOK_VERSION)")
        self.assertLess(guard, tail.index("new IndividualProgressionF14LifecycleRemovalScript();"))
        self.assertLess(tail.index("new IndividualProgressionF14LifecycleRemovalScript();"), tail.index("#endif"))

    def test_corpse_and_respawn_always_clear_stamp(self):
        src = strip_comments(read(SCRIPT))
        body = src.split("static void RetireIncarnation(Creature* creature, bool respawn)", 1)[1]
        body = body[:body.index("\n    }\n")]
        self.assertIn("OnCreatureCorpseOrRespawn(KeyOf(creature), stamp, respawn);", body)
        self.assertIn("SetStamp(creature, 0);", body)
        self.assertNotIn("f14ExperimentalAttackLifecycleRegistry", body)


class IndependentVerifiedLogin(unittest.TestCase):
    def login_block(self):
        player = read("src/IndividualProgressionPlayer.cpp")
        login = player.split("void OnPlayerLogin(Player* player) override", 1)[1].split(
            "void OnPlayerLogout(Player* player) override", 1)[0]
        start = login.index("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE")
        return player, login[start:login.index("#endif", start)]

    def test_registry_flag_alone_enables_positive_identity_capture(self):
        player, block = self.login_block()
        cond = block[block.index("if ((") : block.index("{")]
        for flag in ("f14ProtectVerifiedRandomBotGroupFloor",
                     "f14ExperimentalRndbotPlayerVisibility",
                     "f14ExperimentalAttackLifecycleRegistry"):
            self.assertIn("sIndividualProgression->" + flag, cond)
        # The three reasons are OR-ed; every identity gate is still AND-ed.
        reasons = cond[cond.index("((") + 2 : cond.index(") &&")]
        self.assertEqual(reasons.count("||"), 2)
        self.assertNotIn("&&", reasons)
        for gate in ("sIndividualProgression->isBotAccount(player)",
                     "(player->getRace() == RACE_DRAENEI || player->getRace() == RACE_BLOODELF)",
                     "player->GetSession() && player->GetSession()->IsHeadless()"):
            self.assertIn(gate, cond)
        self.assertIn("RememberF14VerifiedRandomBotGroupIdentity(\n                player, sRandomPlayerbotMgr.IsRandomBot(player->GetGUID().GetCounter()));", block)
        # Humans/Altbots (non-bot accounts), GMs and non-headless sessions never
        # reach the positive branch; the forget call runs first on every login.
        login = player.split("void OnPlayerLogin(Player* player) override", 1)[1]
        self.assertLess(login.index("ForgetF14VerifiedRandomBotGroupIdentity(player);"),
                        login.index("#if COSMIC_IP_F14_PLAYERBOTS_AVAILABLE"))

    def test_registry_flag_used_nowhere_else_in_player_logic(self):
        player, _ = self.login_block()
        self.assertEqual(player.count("f14ExperimentalAttackLifecycleRegistry"), 1)
        for path in ("src/cs_individualProgression.cpp", "src/IndividualProgressionExpansion.cpp"):
            self.assertNotIn("f14ExperimentalAttackLifecycleRegistry", read(path))
        core = strip_comments(read("src/IndividualProgression.cpp"))
        # Only the config load reads/forces it; no stage/group/visibility branch.
        lo = core.index('"' + KEY + '", false)')
        hi = core.index("sIndividualProgression->ResetF14LifecycleRegistry();")
        hits = [m.start() for m in re.finditer(r"f14ExperimentalAttackLifecycleRegistry", core)]
        self.assertEqual(len(hits), 5)
        self.assertTrue(all(lo - 200 < h < hi for h in hits), hits)


class Stage2ParticipantSnapshot(unittest.TestCase):
    def script(self):
        return strip_comments(read(SCRIPT))

    def fn(self, src, signature, end="\n    }\n"):
        body = src.split(signature, 1)[1]
        return body[:body.index(end)]

    def test_invalidation_hooks_registered_once_and_values_untouched(self):
        src = read(SCRIPT)
        for hook in ("UNITHOOK_ON_DAMAGE", "UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK",
                     "UNITHOOK_ON_HEAL", "UNITHOOK_ON_UNIT_ENTER_COMBAT",
                     "UNITHOOK_ON_UNIT_EXIT_COMBAT", "UNITHOOK_ON_UNIT_ENTER_EVADE_MODE"):
            self.assertEqual(src.count(hook), 1, hook)
        # Damage/heal amounts are unnamed parameters: they cannot be modified.
        for sig in ("void OnDamage(Unit* attacker, Unit* victim, uint32& /*damage*/) override",
                    "void ModifyPeriodicDamageAurasTick(Unit* target, Unit* caster, uint32& /*amount*/, SpellInfo const* /*spellInfo*/) override",
                    "void OnHeal(Unit* healer, Unit* receiver, uint32& /*gain*/) override"):
            self.assertIn(sig, src)

    def test_snapshot_only_after_rostered_source_never_same_event(self):
        body = self.fn(self.script(), "void HandleParticipantEvent(Unit* source, Unit* target, bool mayBeAssist)", "\n    }\n\n")
        self.assertTrue(body.split("{", 1)[1].lstrip().startswith("if (!LifecycleEnabled() || !target)"))
        self.assertIn("bool evidenceChanged = registry.OnParticipantEvidence(key, stamp, sourceToken);", body)
        self.assertIn("if (!evidenceChanged && registry.NeedsSnapshot(key, stamp))", body)
        self.assertIn("evidenceChanged = true;", body)  # strip_comments() already removed source comment
        self.assertIn("if (evidenceChanged)", body)
        self.assertIn("pending.Remember(key, stamp);", body)
        self.assertIn("OnAssistOfActor(receiver, ActorTokenOf(source),", body)
        self.assertEqual(body.count("ApplyParticipantSnapshot("), 1)
        self.assertEqual(body.count("std::lock_guard<std::mutex> guard(LifecycleLock());"), 2)
        self.assertIn("AsWorldCreature(source) ? 0 : ActorTokenOf(source)", body)
        self.assertIn("(source->IsPlayer() || source->IsControlledByPlayer())", body)
        self.assertIn("OnAssistOfActor(receiver, ActorTokenOf(source))", body)
        # Untracked creatures return before any lock is taken.
        self.assertLess(body.index("if (!StampOf(tracked))"), body.index("LifecycleLock()"))

    def test_snapshot_reads_threat_combat_attackers_and_tap_with_bounded_loops(self):
        body = self.fn(self.script(), "Registry::ParticipantSnapshot CollectSnapshot(Creature* creature)", "\n    }\n\n")
        for token in ("GetThreatMgr().GetUnsortedThreatList()", "ref->GetVictim()",
                      "GetCombatManager().GetPvECombatRefs()", "GetOther(creature)",
                      "GetCombatManager().GetPvPCombatRefs().empty()",
                      "GetThreatMgr().CanHaveThreatList()", "creature->getAttackers()",
                      "GetLootRecipientGroupGUID() == 0", "GetLootRecipientGUID()",
                      "Registry::SnapshotCollector collector;", "collector.Fail();",
                      "collector.Take();"):
            self.assertIn(token, body)
        # Every one of the three range loops is guarded by the collector and
        # breaks IMMEDIATELY when it says stop (CS01 bounded-lock rule).
        loops = re.findall(r"if \(!collector\.Stopped\(\)\)\n\s+for \(([^\n]+)\)\n\s+if \(!VisitUnit\(collector, ([^\n]+), (true|false)\)\)\n\s+break;", body)
        self.assertEqual(len(loops), 3, loops)
        self.assertEqual([l[2] for l in loops], ["false", "false", "true"])
        self.assertEqual(len(re.findall(r"\bfor \(", body)), 3)
        # Offline threat references are NOT filtered out; no allocation.
        for forbidden in ("IsOffline()", "IsOnline()", "IsAvailable()", "std::vector", "new ",
                          "continue;", "addParticipant"):
            self.assertNotIn(forbidden, body)

    def test_collector_has_total_visit_budget(self):
        reg = strip_comments(read(REGISTRY))
        self.assertIn("constexpr std::size_t MaxSnapshotVisits = 64;", reg)
        add = reg.split("bool AddParticipant(std::uint64_t token)", 1)[1].split("\n        }\n", 1)[0]
        # The visit counter is incremented BEFORE the duplicate check, so
        # duplicate flooding consumes the budget.
        self.assertLess(add.index("++_visits > MaxSnapshotVisits"), add.index("return true; "))
        self.assertIn("if (_stopped)", add)
        attacker = reg.split("bool AddAttacker(std::uint64_t token)", 1)[1].split("\n        }\n", 1)[0]
        self.assertIn("if (!AddParticipant(token))", attacker)

    def test_hostility_never_claimed(self):
        reg = strip_comments(read(REGISTRY))
        self.assertIn("out.plainUnscriptedNonEliteHostileNpc = false;", reg)
        self.assertEqual(reg.count("plainUnscriptedNonEliteHostileNpc"), 1)
        self.assertNotIn("HostileNpc", self.script())

    def test_plain_creature_scope_is_conservative(self):
        body = self.fn(self.script(), "bool IsPlainUnscriptedNonEliteNpc(Creature const* creature)")
        for token in ("rank == CREATURE_ELITE_NORMAL", "!creature->isWorldBoss()", "!creature->IsDungeonBoss()",
                      "!creature->IsSummon()", "!creature->IsVehicle()", "!creature->IsCharmed()",
                      "!creature->IsPet()", "!creature->IsGuardian()", "!creature->IsTotem()",
                      "!creature->IsTrigger()", "!creature->IsGuard()", "!creature->IsCivilian()",
                      "GetScriptId() == 0", "GetAIName().empty()", "GetNpcFlags() == UNIT_NPC_FLAG_NONE"):
            self.assertIn(token, body)

    def test_retirement_runs_even_when_ip_or_option_disabled(self):
        src = self.script()
        for sig in ("static void RetireIncarnation(Creature* creature, bool respawn)",
                    "void OnCreatureRemoveWorld(Creature* creature) override",
                    "void RetireOnCombatEnd(Unit* unit)"):
            body = self.fn(src, sig)
            self.assertNotIn("->enabled", body, sig)
            self.assertNotIn("LifecycleEnabled()", body, sig)
            self.assertIn("SetStamp(creature, 0);", body, sig)
        exit_ = self.fn(src, "void OnUnitExitCombat(Unit* unit) override")
        evade = self.fn(src, "void OnUnitEnterEvadeMode(Unit* unit, uint8 ) override")
        self.assertIn("RetireOnCombatEnd(unit);", exit_)
        self.assertIn("RetireOnCombatEnd(unit);", evade)

    def test_registry_never_sets_participant_or_rights_proof(self):
        reg = strip_comments(read(REGISTRY))
        # CS01 acceptance rule: Core threat/combat/tap snapshots are not a
        # complete census. The registry never calls the proof/rebuild APIs.
        self.assertNotIn("ReverifyCompleteParticipantFacts", reg)
        self.assertNotIn("ReplaceWithFullyVerifiedRoster", reg)
        self.assertNotIn("completeCurrentParticipantProof", reg)
        body = reg.split("bool BuildCreatureSnapshot(", 1)[1].split("\n        }\n", 1)[0]
        for field in ("complete", "eligibleHiddenRandomBotCurrentlyAttackingThisCreature",
                      "everyParticipantVerifiedTbcRaceRandomBotOrItsMinion",
                      "noRealPlayerCombatThreatTapOrRights", "noPendingThreatOrCombatantChanges"):
            self.assertIn("out." + field + " = false;", body)
            self.assertEqual(body.count("out." + field + " ="), 1, field)
        cand = reg.split("bool IsConcealmentCandidate(", 1)[1].split("\n        }\n", 1)[0]
        self.assertIn("Policy::HasVerifiedAttackerCandidates(", cand)
        # Snapshots may only ADD uncertainty and never create entries.
        apply_ = reg.split("void ApplyParticipantSnapshot(", 1)[1].split("bool GetConservativeEvidence(", 1)[0]
        self.assertIn("Apply(*entry, Policy::Event::UnverifiedOrHumanParticipant, 0);", apply_)
        self.assertNotIn("uncertain = false", apply_)
        self.assertNotIn("Create(", apply_)
        self.assertNotIn("state =", apply_.replace("entry->state.generation", ""))
        # The registry never fills any observer-side fact.
        for observer_only in ("ObserverSnapshot", "coreVisibilityAndThreatRefreshVerified",
                              "explicitlyEnabled", "ShouldConcealCreatureFromObserver"):
            self.assertNotIn(observer_only, reg)
        self.assertIn("struct ConservativeEvidence", reg)


class Stage3MinionOwnerEvidence(unittest.TestCase):
    def script(self):
        return strip_comments(read(SCRIPT))

    def fn(self, signature):
        body = self.script().split(signature, 1)[1]
        return body[:body.index("\n    }\n")]

    def test_lineage_is_value_only_and_delegates_to_pure_rule(self):
        body = self.fn("std::uint64_t MinionOwnerEpochOf(Unit const* unit)")
        for token in ("GetOwnerGUID()", "GetCharmerGUID()", "GetCreatorGUID()", "ToTempSummon()",
                      "GetSummonerGUID()", "isPossessed()", "IsVehicle()", "IsPet()", "IsGuardian()",
                      "IsTotem()", "IsSummon()", "owner.IsPlayer()",
                      "Registry::LineageMatchesSingleOwner(lineage)",
                      "Registry::MinionOwnerEvidence(lineage,",
                      "GetF14VerifiedRandomBotLoginEpoch(owner)"):
            self.assertIn(token, body)
        # Never dereference owner/charmer/summoner, never ask Playerbots.
        for forbidden in ("GetOwner()", "GetCharmer()", "GetCharmerOrOwner(", "GetSummonerUnit(",
                          "GetSummoner()", "ObjectAccessor", "sRandomPlayerbotMgr", "ToPlayer("):
            self.assertNotIn(forbidden, body)
        # The epoch lookup happens only after the lineage matched.
        self.assertLess(body.index("if (!Registry::LineageMatchesSingleOwner(lineage))"),
                        body.index("GetF14VerifiedRandomBotLoginEpoch(owner)"))

    def test_pure_lineage_rule_checks_every_guid(self):
        reg = strip_comments(read(REGISTRY))
        rule = reg.split("inline bool LineageMatchesSingleOwner(MinionLineage const& l)", 1)[1].split("\n    }\n", 1)[0]
        for token in ("!l.minionType", "l.vehicleOrPossessed", "!l.ownerIsPlayer", "l.owner == 0",
                      "l.charmer != 0 && l.charmer != l.owner", "l.creator != 0 && l.creator != l.owner",
                      "!l.summonDataAvailable", "l.summoner != 0 && l.summoner != l.owner"):
            self.assertIn(token, rule)
        lineage = reg.split("struct MinionLineage", 1)[1].split("};", 1)[0]
        self.assertIn("bool vehicleOrPossessed = true;", lineage)   # defaults fail
        self.assertIn("bool minionType = false;", lineage)

    def test_visit_order_bot_then_minion_then_unknown(self):
        body = self.fn("bool VisitUnit(Registry::SnapshotCollector& collector, Unit const* unit, bool attacking)")
        a = body.index("ActorTokenOf(unit)")
        m = body.index("MinionOwnerEpochOf(unit)")
        u = body.index("return collector.AddParticipant(0);")
        self.assertLess(a, m)
        self.assertLess(m, u)
        self.assertIn("return collector.AddMinion(owner);", body)
        # A minion is never added as a participant or attacker token.
        self.assertNotIn("AddAttacker(owner)", body)
        self.assertNotIn("AddParticipant(owner)", body)

    def test_minions_never_reduce_uncertainty_or_reach_proof(self):
        reg = strip_comments(read(REGISTRY))
        apply_ = reg.split("void ApplyParticipantSnapshot(", 1)[1].split("bool GetConservativeEvidence(", 1)[0]
        self.assertIn("bool const onlyVerified = noUnknown && snap.participantCount > 0 && !anyMinion;", apply_)
        self.assertIn("if (!onlyVerified || !snap.alive)\n                Apply(*entry, Policy::Event::UnverifiedOrHumanParticipant, 0);", apply_)
        for forbidden in ("ReplaceWithFullyVerifiedRoster", "ReverifyCompleteParticipantFacts", "VerifiedAttackBegin"):
            self.assertNotIn(forbidden, apply_)
        expire = reg.split("std::size_t ExpireActor(std::uint64_t actorToken,", 1)[1].split("\n        }\n", 1)[0]
        self.assertIn("std::function<void(CreatureKey const&, std::uint64_t)> onAffected = {}", expire)
        self.assertIn("f.minionOwners[i] == actorToken", expire)
        self.assertIn("Policy::Event::ThreatRightsOrIdentityChanged", expire)
        minion = reg.split("bool AddMinion(std::uint64_t ownerEpoch)", 1)[1].split("\n        }\n", 1)[0]
        self.assertLess(minion.index("++_visits > MaxSnapshotVisits"), minion.index("minionOwners[i] == ownerEpoch"))
        self.assertIn("if (ownerEpoch == 0)", minion)


if __name__ == "__main__":
    unittest.main()
