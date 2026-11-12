"""Current-lineage F14 rights-change refresh request: default OFF, no positive concealment authority."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "efe91f3fbd9b12643e903bbbf25f9b59e8c2b624"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-current-rights-refresh.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "src/IndividualProgressionF14CurrentRightsRefresh.h",
    "tests/f14_current_rights_refresh_test.cpp",
    "tests/test_f14_current_rights_refresh_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP164 changed-path scope")

path = "src/IndividualProgressionF14CurrentCoreLifecycle.cpp"
diff = subprocess.check_output(["git","diff","-U0",PARENT,"HEAD","--",path], cwd=ROOT, text=True)
removed = [line for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
require(not removed, "IP164 current lifecycle runtime source must be insertion-only")

old_cpp = subprocess.check_output(["git","show",PARENT + ":" + path], cwd=ROOT, text=True)
cpp = (ROOT / path).read_text()

def section(text, start, end):
    return text.split(start,1)[1].split(end,1)[0]

# Existing positive-query observer must be byte-identical and permanently visible.
start = "class CurrentTokenBoundInitialOutObserver final"
end = "#if defined(COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT)"
require(section(cpp,start,end) == section(old_cpp,start,end),
        "IP164 must not modify the token-bound suppression callback")
require("return true;" not in section(cpp,start,end), "token-bound callback gained positive authority")

for needle in (
    "COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT",
    "IndividualProgressionF14CurrentRightsRefresh::Pending rightsRefresh;",
    "RequestActorCurrentCreatureRefresh(",
    "ResolveCurrentCombatCreature(",
    "RequestInteractionRightsRefresh(",
    "class CurrentRightsRefreshUnit final",
    "UNITHOOK_ON_DAMAGE",
    "UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK",
    "UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN",
    "UNITHOOK_ON_HEAL",
    "UNITHOOK_MODIFY_HEAL_RECEIVED",
    "UNITHOOK_ON_AURA_APPLY",
    "UNITHOOK_ON_AURA_REMOVE",
    "UNITHOOK_ON_UNIT_UPDATE",
    "creature->UpdateObjectVisibility(true);",
):
    require(needle in cpp, "missing current rights-refresh contract: " + needle)

rights = section(cpp, "class CurrentRightsRefreshUnit final", "class CurrentCombatUnit final")
require(rights.count("creature->UpdateObjectVisibility(true);") == 1,
        "rights observer must have exactly one visibility call")
update = section(rights, "void OnUnitUpdate", "void OnUnitEnterEvadeMode")
require("creature->UpdateObjectVisibility(true);" in update,
        "visibility request must be consumed only from owning creature Unit::Update")
for event in ("void OnHeal", "void OnDamage", "void ModifyPeriodicDamageAurasTick",
              "void ModifySpellDamageTaken", "void ModifyHealReceived",
              "void OnAuraApply", "void OnAuraRemove"):
    piece = rights.split(event,1)[1].split("}",1)[0]
    require("UpdateObjectVisibility" not in piece, event + " must only mark pending work")

added = "\n".join(line[1:] for line in diff.splitlines()
                  if line.startswith("+") and not line.startswith("+++"))
for forbidden in (".Open(", ".Reserve(", ".Commit(", "DestroyForPlayer(",
                  "SendDirectMessage(", "SetPhaseMask(", "return true;"):
    require(forbidden not in added, "IP164 added forbidden positive/debt action: " + forbidden)

# Target stop/session/map teardown must request old-target reevaluation before retirement.
end_target = cpp.split("void EndCombatActorTarget",1)[1].split("void LeaveCombatActorMap",1)[0]
require("RequestActorCurrentCreatureRefresh" in end_target, "ordinary combat end lacks old-target refresh request")
leave_map = cpp.split("void LeaveCombatActorMap",1)[1].split("void EndCombatActorSession",1)[0]
session_end = cpp.split("void EndCombatActorSession",1)[1].split("Creature* CurrentCombatCreature",1)[0]
for name, piece in (("map leave", leave_map), ("session end", session_end)):
    require("RequestActorKeyCreatureRefresh" in piece, name + " must use already-held actor key")
    require("RequestActorCurrentCreatureRefresh" not in piece, name + " must not re-lock combat actor marker")
retire = section(cpp, "void RetireCombatCreature", "#endif\n    Marker const* GetMarker")
require(retire.index("RequestCreatureRightsRefresh") < retire.index("CombatTracker().RetireCreature"),
        "creature death must mark reevaluation before incarnation retirement")

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve IP163 bytes as exact prefix")

print("PASS IP164 current rights refresh: insertion-only, event callbacks mark only, owning Unit::Update is sole visibility call, suppression remains hard-FALSE")