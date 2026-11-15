"""Source contracts: cache-only stage snapshot (Coordination #59, API v1).
Source-level only; behaviour is covered by tests/stage_snapshot_model_test.cpp."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def code(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def body(src, signature):
    start = src.index(signature)
    brace = src.index("{", start)
    depth = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
    raise AssertionError(signature)


HEADER = read("src/IndividualProgressionStageSnapshot.h")
SCRIPT = code(read("src/IndividualProgressionStageSnapshotScript.cpp"))
CORE = code(read("src/IndividualProgression.cpp"))
EXP = code(read("src/IndividualProgressionExpansion.cpp"))
IPH = read("src/IndividualProgression.h")
ALL_SRC = {p.name: code(p.read_text(encoding="utf-8")) for p in (ROOT / "src").rglob("*.cpp")}


class PureHeader(unittest.TestCase):
    def test_dependency_free_and_versioned(self):
        self.assertEqual(re.findall(r'#include [<"]([^>"]+)[>"]', HEADER), ["cstdint", "string_view", "unordered_map"])
        self.assertIn("#define COSMIC_IP_STAGE_SNAPSHOT_API_VERSION 1", HEADER)
        for token in ("Player", "WorldSession", "Database", "Query"):
            self.assertNotIn(token, code(HEADER), token)

    def test_publish_guards(self):
        publish = code(body(HEADER, "PublishResult Publish(std::uint64_t guid, Ticket const& ticket, PublishInput const& in)"))
        order = [publish.index(t) for t in ("RejectedNotRegistered", "RejectedStaleEpoch",
                                            "RejectedStaleGeneration", "RejectedInvalidatedSince",
                                            "Make(in, ticket.epoch, _generation)")]
        self.assertEqual(order, sorted(order))
        get = code(body(HEADER, "Snapshot Get(std::uint64_t guid) const"))
        self.assertNotIn("[", get)        # never inserts
        self.assertIn("_entries.find(guid)", get)


class IpWiring(unittest.TestCase):
    def test_getter_is_cache_only(self):
        get = body(SCRIPT, "IndividualProgressionStageSnapshot::Snapshot IndividualProgression::GetStageSnapshot(ObjectGuid const& guid) const")
        self.assertIn("std::shared_lock<std::shared_mutex> lock(_stageSnapshotLock);", get)
        self.assertIn("return _stageSnapshots.Get(guid.GetRawValue());", get)
        for token in ("Query", "AccountMgr", "GetAccountMatch", "unique_lock", "FindPlayer", "ObjectAccessor", "->"):
            self.assertNotIn(token, get, token)
        self.assertIn("IndividualProgressionStageSnapshot::Snapshot GetStageSnapshot(ObjectGuid const& guid) const;", IPH)
        self.assertIn("mutable std::shared_mutex _stageSnapshotLock;", IPH)

    def test_full_guid_keys_everywhere(self):
        for sig in ("void IndividualProgression::RegisterStageSnapshot(Player* player)",
                    "void IndividualProgression::EraseStageSnapshot(ObjectGuid const& guid)",
                    "void IndividualProgression::InvalidateStageSnapshot(ObjectGuid const& guid)",
                    "void IndividualProgression::PublishStageSnapshotIfDirty(Player* player)"):
            b = body(SCRIPT, sig)
            self.assertIn("GetRawValue()", b, sig)
            self.assertNotRegex(b, r"_stageSnapshots\.\w+\([^)]*GetCounter\(\)", sig)

    def test_only_owner_update_publishes(self):
        calls = {n: s.count("PublishStageSnapshotIfDirty(") for n, s in ALL_SRC.items() if "PublishStageSnapshotIfDirty(" in s}
        self.assertEqual(calls, {"IndividualProgressionStageSnapshotScript.cpp": 2})  # definition + OnPlayerUpdate
        update = body(SCRIPT, "void OnPlayerUpdate(Player* player, uint32 ) override")
        self.assertIn("sIndividualProgression->PublishStageSnapshotIfDirty(player);", update)
        publishers = [n for n, s in ALL_SRC.items() if "_stageSnapshots.Publish(" in s]
        self.assertEqual(publishers, ["IndividualProgressionStageSnapshotScript.cpp"])
        self.assertEqual(SCRIPT.count("_stageSnapshots.Publish("), 1)

    def test_publisher_no_db_no_lock_while_computing(self):
        pub = body(SCRIPT, "void IndividualProgression::PublishStageSnapshotIfDirty(Player* player)")
        self.assertNotRegex(pub, r"(?<!Try)GetAccountMatch\(")
        for token in ("AccountMgr", "Query", "isNormalAccount", "GetExpansionFacts(", "GetEffectiveProgression("):
            self.assertNotIn(token, pub, token)
        self.assertIn("TryGetAccountMatch(session->GetAccountId(), match)", pub)
        # Shared-lock ticket scope closes before computing; unique lock only at the end.
        first = pub.index("ticket = _stageSnapshots.BeginPublish(key, _stageSnapshots.EpochOf(key));")
        compute = pub.index("input.rawStage = GetPlayerProgressionFromQuests(player);")
        unique = pub.index("std::unique_lock<std::shared_mutex> lock(_stageSnapshotLock);")
        self.assertLess(first, compute)
        self.assertLess(compute, unique)
        self.assertEqual(pub.count("_stageSnapshotLock"), 2)
        self.assertIn("if (!ticket.valid)\n        return;", pub)
        warm = body(SCRIPT, "void IndividualProgression::WarmAccountCacheForOnlineSessions()")
        self.assertNotIn("_stageSnapshotLock", warm)
        tryget = body(SCRIPT, "bool IndividualProgression::TryGetAccountMatch(uint32 accountId, AccountMatch& match)")
        for token in ("AccountMgr", "GetName", "GetSecurity", "unique_lock", "_accountMatchCache["):
            self.assertNotIn(token, tryget, token)

    def test_invalidate_before_changes(self):
        force = body(CORE, "void IndividualProgression::ForceUpdateProgressionState(Player* player, ProgressionState newState)")
        inv = force.index("sIndividualProgression->InvalidateStageSnapshot(player->GetGUID());")
        for marker in ("RemoveRewardedQuest", "RewardQuest(", "AddQuest("):
            if marker in force:
                self.assertLess(inv, force.index(marker), marker)
        upd = body(CORE, "void IndividualProgression::UpdateProgressionState(Player* player, ProgressionState newState) const")
        self.assertLess(upd.index("sIndividualProgression->InvalidateStageSnapshot(player->GetGUID());"), upd.index("player->AddQuest(quest, nullptr);"))
        setter = body(EXP, "void IndividualProgression::SetPaidExpansionFlag(Player* player, uint32 index, uint32 value)")
        self.assertLess(setter.index("InvalidateStageSnapshot(player->GetGUID());"), setter.index("UpdatePlayerSetting("))
        self.assertLess(setter.index("InvalidateStageSnapshot(player->GetGUID());"), setter.index("bits |= bit"))
        before = body(CORE, "void OnBeforeConfigLoad(bool reload) override")
        self.assertLess(before.index("sIndividualProgression->InvalidateAllStageSnapshots();"), before.index("LoadConfig();"))
        after = body(CORE, "void OnAfterConfigLoad(bool reload) override")
        self.assertLess(after.index("sIndividualProgression->InvalidateAllStageSnapshots();"),
                        after.index("sIndividualProgression->WarmAccountCacheForOnlineSessions();"))

    def test_lifecycle_hooks(self):
        script = body(SCRIPT, "class IndividualProgression_StageSnapshotPlayerScript : public PlayerScript")
        for hook in ("PLAYERHOOK_ON_LOGIN", "PLAYERHOOK_ON_LOGOUT", "PLAYERHOOK_ON_UPDATE", "PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST"):
            self.assertIn(hook, script)
        self.assertIn("sIndividualProgression->RegisterStageSnapshot(player);", body(script, "void OnPlayerLogin(Player* player) override"))
        self.assertIn("sIndividualProgression->EraseStageSnapshot(player->GetGUID());", body(script, "void OnPlayerLogout(Player* player) override"))
        quest = body(script, "void OnPlayerCompleteQuest(Player* player, Quest const* quest) override")
        self.assertIn("SnapshotApi::IsStageMarkerQuest(quest->GetQuestId())", quest)
        self.assertIn("InvalidateStageSnapshot(player->GetGUID())", quest)
        cmd = body(SCRIPT, "bool OnTryExecuteCommand(ChatHandler& , std::string_view cmdStr) override")
        self.assertIn("if (SnapshotApi::MentionsStageMarker(cmdStr))\n            sIndividualProgression->InvalidateAllStageSnapshots();", cmd)
        self.assertRegex(cmd, r"return true;")
        self.assertNotIn("return false", cmd)
        self.assertIn("AddSC_mod_individual_progression_stage_snapshot();",
                      body(CORE, "void AddSC_mod_individual_progression()"))

    def test_existing_getters_unchanged(self):
        eff = body(EXP, "uint8 IndividualProgression::GetEffectiveProgression(Player* player)")
        self.assertNotIn("Snapshot", eff)


if __name__ == "__main__":
    unittest.main()
