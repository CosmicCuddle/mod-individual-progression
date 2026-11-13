"""F14 pair-rights runtime adapter: source contracts, not in-game proof.

Pins that the token-bound initial-OUT observer stays hard-FALSE, that the
adapter is diagnostic-only and macro-gated, that the Core collector reads
only the viewer's/creature's own state through bounded loops (no owned-unit
dereference, no caster lookup, no DB/Playerbots/packets), that the pure
mapping keeps the four unprovable families hard-incomplete, and that every
file outside the eight approved paths is byte-identical to IP #172.
"""
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT = "aed05a92f262362608f5d643c8570b504189aae0"  # IP #172 head
LIFECYCLE = "src/IndividualProgressionF14CurrentCoreLifecycle.cpp"
EVIDENCE = "src/IndividualProgressionF14PairRightsEvidence.h"
ADAPTER = "src/IndividualProgressionF14PairRightsCoreAdapter.h"
MACRO = "COSMIC_IP_F14_PAIR_RIGHTS_ADAPTER_EXPERIMENT"
GATE = "#if defined(" + MACRO + ") && " + MACRO + " >= 1"
SCOPE = {
    EVIDENCE, ADAPTER, LIFECYCLE,
    "tests/f14_pair_rights_evidence_test.cpp",
    "tests/test_f14_pair_rights_adapter_source.py",
    ".github/workflows/f14-pair-rights-adapter.yml",
    "docs/F14_PAIR_RIGHTS_ADAPTER_2026-11-13.md",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
}

# The exact reviewed insertions into the lifecycle file (each exactly once).
INSERT_ERROR = (
    GATE + "\n"
    "#if !defined(COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT) || "
    "COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT < 1\n"
    "#error \"F14 pair-rights adapter is diagnostic-only inside the token-bound initial OUT observer\"\n"
    "#endif\n"
    "#endif\n"
)
INSERT_INCLUDE = (
    GATE + "\n"
    "#include \"IndividualProgressionF14PairRightsCoreAdapter.h\"\n"
    "#include <array>\n"
    "#endif\n"
)
INSERT_CALL = (
    GATE + "\n"
    "            // Diagnostic only: every existing gate above held. The verdict\n"
    "            // is counted, never acted on; the answer stays FALSE (visible).\n"
    "            RecordPairRightsDiagnostic(viewer, creature);\n"
    "#endif\n"
)
INSERT_MEMBER = (
    GATE + "\n"
    "        // Aggregate per-result counters; one generic log line per result at\n"
    "        // most. No GUID, pointer, name or per-pair record is retained.\n"
    "        std::array<std::atomic<std::uint64_t>,\n"
    "            IndividualProgressionF14PairRightsEvidence::ResultCount> _pairRightsResults{};\n"
    "\n"
    "        void RecordPairRightsDiagnostic(Player const* viewer, Creature const* creature)\n"
    "        {\n"
    "            namespace Evidence = IndividualProgressionF14PairRightsEvidence;\n"
    "            namespace Adapter = IndividualProgressionF14PairRightsCoreAdapter;\n"
    "            auto const result = Evidence::Classify(Adapter::Collect(viewer, creature, true));\n"
    "            std::size_t const index = static_cast<std::size_t>(result);\n"
    "            if (index < _pairRightsResults.size() &&\n"
    "                _pairRightsResults[index].fetch_add(1, std::memory_order_relaxed) == 0)\n"
    "                LOG_INFO(\"module\", \"IP F14 pair-rights adapter first {} snapshot; diagnostic only, concealment still forced FALSE\",\n"
    "                    Evidence::ResultName(result));\n"
    "        }\n"
    "#endif\n"
)
INSERTS = (INSERT_ERROR, INSERT_INCLUDE, INSERT_CALL, INSERT_MEMBER)


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def strip_comments(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], check=True, capture_output=True).stdout


def body(src, signature):
    rest = src.split(signature, 1)[1]
    depth, started = 0, False
    for i, ch in enumerate(rest):
        if ch == "{":
            depth += 1
            started = True
        elif ch == "}":
            depth -= 1
            if started and depth == 0:
                return rest[:i + 1]
    raise AssertionError("unbalanced " + signature)


class ObserverStaysHardFalse(unittest.TestCase):
    def setUp(self):
        self.src = read(LIFECYCLE)
        self.observer = body(self.src, "bool OnF14ShouldSuppressCreatureInitialOutForViewer(")

    def test_every_return_is_false(self):
        returns = re.findall(r"\breturn\b[^;]*;", strip_comments(self.observer))
        self.assertGreaterEqual(len(returns), 5)
        self.assertEqual(set(returns), {"return false;"})

    def test_adapter_runs_after_every_existing_gate(self):
        o = self.observer
        call = o.index("RecordPairRightsDiagnostic(viewer, creature);")
        for gate in ("PassTracker().IsActive(passToken, identity)",
                     "creature->IsAlive()",
                     "CombatTracker().ActiveAttackers(key) == 0"):
            self.assertLess(o.index(gate), call, gate)
        self.assertLess(call, o.rindex("return false;"))
        self.assertLess(o.rindex(GATE, 0, call), call)

    def test_lifecycle_is_parent_plus_exact_insertions(self):
        for block in INSERTS:
            self.assertEqual(self.src.count(block), 1, block[:70])
        stripped = self.src
        for block in INSERTS:
            stripped = stripped.replace(block, "", 1)
        self.assertEqual(stripped.encode("utf-8"), git("show", PARENT + ":" + LIFECYCLE))

    def test_macro_requires_token_observer_and_is_never_defined(self):
        self.assertIn(INSERT_ERROR, self.src)
        for path in sorted((ROOT / "src").rglob("*")):
            if path.is_file():
                self.assertNotRegex(path.read_text(encoding="utf-8", errors="replace"),
                                    r"#\s*define\s+" + MACRO, path.name)


class CollectorReadRules(unittest.TestCase):
    def setUp(self):
        self.raw = read(ADAPTER)
        self.src = strip_comments(self.raw)
        self.code = self.src.split("namespace IndividualProgressionF14PairRightsCoreAdapter", 1)[1]

    def test_forbidden_reads_and_writes_absent(self):
        for token in ("const_cast", "mutable", "ObjectAccessor", "GetUnit(", "GetCreature(", "GetPlayer(",
                      "Playerbot", "sRandomPlayerbotMgr", "Database", "QueryResult", "PreparedStatement",
                      "WorldPacket", "SendPacket", "SendDirectMessage", "SetPhaseMask", "UpdateObjectVisibility",
                      "GetGossipMenu", "PlayerTalkClass", "IsEscorted", "GetFirstControlled", "Seats",
                      "GetPassenger", "GetVehicleBase", "Reserve", "Commit", "Ledger", "Store(", "static ",
                      "std::mutex", "thread_local"):
            self.assertNotIn(token, self.code, token)

    def test_owned_units_presence_only(self):
        self.assertEqual(re.findall(r"m_Controlled[^;\n]*", self.code), ["m_Controlled.empty()"])
        self.assertEqual(len(re.findall(r"m_SummonSlot\[slot\]\.IsEmpty\(\)", self.code)), 1)
        self.assertNotRegex(self.code, r"m_SummonSlot\[[^\]]*\]\s*(->|\.(?!IsEmpty))")

    def test_every_loop_is_bounded(self):
        loops = re.findall(r"(?m)^\s*for \((.*)\)\s*$", self.code)
        allowed = (
            "auto const& entry : owner->GetAppliedAuras()",
            "uint32 slot = 0; slot < MAX_SUMMON_SLOT; ++slot",
            "uint32 type = 0; type < CURRENT_MAX_SPELL; ++type",
            "uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot",
            "uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i",
            "uint32 k = 0; k < MAX_KILL_CREDIT; ++k",
        )
        self.assertEqual(sorted(loops), sorted(allowed))
        self.assertNotIn("while", self.code)
        aura = body(self.src, "inline Evidence::AuraScan ScanAppliedAuras(")
        self.assertLess(aura.index("if (out.total == Evidence::MaxAurasPerUnit)"), aura.index("++out.total;"))
        self.assertIn("out.overflow = true;", aura)

    def test_caster_classified_by_guid_only(self):
        aura = body(self.src, "inline Evidence::AuraScan ScanAppliedAuras(")
        self.assertIn("base->GetCasterGUID()", aura)
        self.assertNotIn("GetCaster()", aura)

    def test_uses_anchored_core25_apis(self):
        for api in ("IsGameMaster()", "GetCharmerGUID()", "GetSeer()", "GetTarget()", "GetVictim()",
                    "IsThreatenedBy(viewer)", "IsInCombatWith(viewer)", "IsInCombatWith(creature)",
                    "GetCharmGUID()", "GetMinionGUID()", "GetPetGUID()", "GetCritterGUID()",
                    "GetVehicle()", "GetVehicleKit()", "GetOwnerGUID()", "GetCreatorGUID()",
                    "IsControlledByPlayer()", "ToTempSummon()", "GetSummonerGUID()", "GetGroup()",
                    "GetLootRecipientGUID()", "GetLootRecipientGroupGUID()", "GetAppliedAuras()",
                    "GetBase()", "GetCurrentSpell(", "GetNpcFlags()", "IsServiceProvider()",
                    "GetCurrentVendor()", "GetScriptId()", "GetAIName()", "Instanceable()", "IsDungeon()",
                    "GetFormation()", "isWorldBoss()", "IsDungeonBoss()", "GetQuestSlotQuestId(slot)",
                    "GetQuestStatus(questId)", "GetQuestTemplate(questId)", "RequiredNpcOrGo[i]",
                    "KillCredit[k]", "HaveQuestLootForPlayer(", "LootTemplates_Creature",
                    "LootTemplates_Pickpocketing", "LootTemplates_Skinning"):
            self.assertIn(api, self.code, api)

    def test_collector_only_included_by_the_lifecycle_under_the_macro(self):
        for path in sorted((ROOT / "src").rglob("*")):
            if path.is_file() and path.name not in (Path(ADAPTER).name, Path(LIFECYCLE).name):
                self.assertNotIn("IndividualProgressionF14PairRightsCoreAdapter",
                                 path.read_text(encoding="utf-8", errors="replace"), path.name)


class PureMappingRules(unittest.TestCase):
    def setUp(self):
        self.src = strip_comments(read(EVIDENCE))

    def test_pure_includes(self):
        self.assertEqual(re.findall(r'#include\s+[<"]([^>"]+)[>"]', self.src),
                         ["IndividualProgressionF14CurrentPairRightsRisk.h", "cstdint"])
        body_ = self.src.split("namespace IndividualProgressionF14PairRightsEvidence", 1)[1]
        for word in ("Player", "Creature", "Unit", "WorldObject", "ObjectGuid", "Map"):
            self.assertIsNone(re.search(r"\b" + word + r"\b", body_), word)

    def test_four_families_hard_incomplete(self):
        for bit in ("pendingSpellEvidenceComplete", "serviceInteractionEvidenceComplete",
                    "scriptedRewardEvidenceComplete", "questEvidenceComplete"):
            self.assertEqual(re.findall(r"f\." + bit + r"\s*=\s*([^;]+);", self.src), ["false"], bit)
        self.assertIn("constexpr bool CanReachCleanTerminalOnCore25() { return false; }", self.src)
        self.assertIn("constexpr bool IsConcealmentAuthorized(Raw const&) { return false; }", self.src)

    def test_failing_defaults(self):
        raw = self.src.split("struct Raw", 1)[1].split("};", 1)[0]
        for name in ("observerGatesPassed", "eligibilityScanned", "directScanned", "ownedScanned",
                     "roleScanned", "groupLootScanned", "viewerSeerIsSelf"):
            self.assertIn("bool " + name + " = false;", raw, name)
        for name in ("viewerIsGameMaster", "viewerCharmed", "anyControlledUnit", "anySummonSlot",
                     "anyCharmMinionPetOrCritterGuid", "anyViewerVehicleRelation"):
            self.assertIn("bool " + name + " = true;", raw, name)


class ParentsAndFlagsUnchanged(unittest.TestCase):
    def test_every_parent_file_outside_scope_is_byte_identical(self):
        tracked = git("ls-tree", "-r", "--name-only", PARENT).decode().splitlines()
        self.assertGreater(len(tracked), 50)
        self.assertIn("src/IndividualProgressionF14CurrentPairRightsRisk.h", tracked)
        for name in tracked:
            if name in SCOPE:
                continue
            self.assertEqual((ROOT / name).read_bytes(), git("show", PARENT + ":" + name), name)

    def test_flags_off(self):
        conf = read("conf/individualProgression.conf.dist")
        keys = re.findall(r"(?m)^(IndividualProgression\.(?:F14\.\w+|PaidExpansion\.Enable))\s*=", conf)
        self.assertGreater(len(keys), 3)
        for key in keys:
            self.assertRegex(conf, r"(?m)^" + re.escape(key) + r"\s*=\s*0\s*$", key)


if __name__ == "__main__":
    unittest.main()
