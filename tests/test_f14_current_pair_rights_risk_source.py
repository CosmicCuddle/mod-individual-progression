"""Current F14 pair-rights risk is pure negative evidence, with missing evidence fail-visible."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "5ed525802273178a721f4eec9f4fb05bdac59964"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-current-pair-rights-risk.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentPairRightsRisk.h",
    "tests/f14_current_pair_rights_risk_test.cpp",
    "tests/test_f14_current_pair_rights_risk_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected current rights-completeness changed-path scope")

h = (ROOT / "src/IndividualProgressionF14CurrentPairRightsRisk.h").read_text()
for needle in (
    "EvidenceIncomplete",
    "NoKnownVeto_NotAuthority",
    "viewerEligibilityEvidenceComplete",
    "creatureEligibilityEvidenceComplete",
    "directInteractionEvidenceComplete",
    "ownedUnitEvidenceComplete",
    "creatureRoleEvidenceComplete",
    "groupLootEvidenceComplete",
    "auraEvidenceComplete",
    "pendingSpellEvidenceComplete",
    "serviceInteractionEvidenceComplete",
    "scriptedRewardEvidenceComplete",
    "questEvidenceComplete",
    "viewerOnCreatureThreatOrCombatReferences",
    "ownedUnitOnCreatureThreatOrCombatReferences",
    "creatureIsPlayerOwnedOrSummoned",
    "creatureIsVehicleOrHasPlayerPassenger",
    "creatureCastAuraOnViewer",
    "viewerOrOwnedUnitHasPendingSpellOrChannelOnCreature",
    "viewerUsesCreatureServiceOrEscort",
    "skinningOrScriptedRewardInterest",
    "encounterOrScriptedInteractionInterest",
    "constexpr bool IsConcealmentAuthorized(Facts const&) { return false; }",
):
    require(needle in h, "missing pair-rights completeness contract: " + needle)

# Every completeness proof must default false. A future adapter must opt in
# family-by-family rather than inheriting a permissive default.
for needle in (
    "bool viewerEligibilityEvidenceComplete = false;",
    "bool creatureEligibilityEvidenceComplete = false;",
    "bool directInteractionEvidenceComplete = false;",
    "bool ownedUnitEvidenceComplete = false;",
    "bool creatureRoleEvidenceComplete = false;",
    "bool groupLootEvidenceComplete = false;",
    "bool auraEvidenceComplete = false;",
    "bool pendingSpellEvidenceComplete = false;",
    "bool serviceInteractionEvidenceComplete = false;",
    "bool scriptedRewardEvidenceComplete = false;",
    "bool questEvidenceComplete = false;",
):
    require(needle in h, "evidence-complete fact became permissive: " + needle)

for forbidden in (
    "Player.h", "Creature.h", "WorldSession", "ScriptMgr",
    "ObjectMgr", "LootTemplates_Creature", "GetAppliedAuras",
    "Reserve(", "Commit(", "DestroyForPlayer", "SendDirectMessage",
):
    require(forbidden not in h, "pure rights-risk model gained runtime/debt dependency: " + forbidden)

# The existing live/hypothetical joint gate remains byte-identical to IP171.
gate = "src/IndividualProgressionF14ViewerRightsGate.h"
old_gate = subprocess.check_output(["git","show",PARENT + ":" + gate], cwd=ROOT)
new_gate = (ROOT / gate).read_bytes()
require(new_gate == old_gate, "IP172 must not modify the existing joint gate")

# Register is append-only relative to the reviewed IP171 parent.
old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom register must preserve IP171 bytes as exact prefix")

print("PASS current F14 pair-rights completeness: omitted evidence fails visible; clean result remains not authority")
