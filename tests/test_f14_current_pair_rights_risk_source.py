"""Current F14 pair-rights risk is pure negative evidence, never concealment authority."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "455597329f2578c634091b8c3a79d83b31181d40"

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
require(sorted(actual) == expected, "unexpected current rights-risk changed-path scope")

h = (ROOT / "src/IndividualProgressionF14CurrentPairRightsRisk.h").read_text()
for needle in (
    "NoKnownVeto_NotAuthority",
    "viewerTargetsCreature",
    "viewerAttacksCreature",
    "creatureAttacksViewer",
    "ownedUnitTargetsOrAttacksCreature",
    "creatureAttacksOwnedUnit",
    "viewerIsGrouped",
    "viewerIsLootRecipient",
    "anyGroupLootRecipient",
    "auraEnumerationOverflow",
    "auraEvidenceUnknown",
    "anyPlayerCastAura",
    "activeQuestTemplateMissing",
    "activeQuestTargetsCreatureOrKillCredit",
    "viewerHasQuestLootForCreature",
    "constexpr bool IsConcealmentAuthorized(Facts const&) { return false; }",
):
    require(needle in h, "missing negative rights-risk contract: " + needle)

for forbidden in (
    "Player.h", "Creature.h", "WorldSession", "ScriptMgr",
    "ObjectMgr", "LootTemplates_Creature", "GetAppliedAuras",
    "Reserve(", "Commit(", "DestroyForPlayer", "SendDirectMessage",
):
    require(forbidden not in h, "pure rights-risk model gained runtime/debt dependency: " + forbidden)

# Existing live/hypothetical joint gate remains byte-identical to parent.
gate = "src/IndividualProgressionF14ViewerRightsGate.h"
old_gate = subprocess.check_output(["git","show",PARENT + ":" + gate], cwd=ROOT)
new_gate = (ROOT / gate).read_bytes()
require(new_gate == old_gate, "current rights-risk child must not modify the existing joint gate")

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom register must preserve IP168 bytes as exact prefix")

print("PASS current F14 pair-rights risk: historical negative evidence preserved; clean result is explicitly not authority")
