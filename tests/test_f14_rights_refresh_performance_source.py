"""IP168: coalesce F14 rights refresh and remove hot-path temporary keys/calc-only arming."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "b4a0ee2a6adf1a6ee391d2226f70fa1bc72136bd"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-current-rights-refresh.yml",
    ".github/workflows/f14-rights-refresh-performance.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/test_f14_rights_refresh_performance_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP168 changed-path scope")

path = "src/IndividualProgressionF14CurrentCoreLifecycle.cpp"
old = subprocess.check_output(["git","show",PARENT + ":" + path], cwd=ROOT, text=True)
new = (ROOT / path).read_text()
diff = subprocess.check_output(["git","diff","-U1",PARENT,"HEAD","--",path], cwd=ROOT, text=True)

# Three DataMap keys become process-lifetime std::string objects, avoiding a
# temporary std::string conversion at every Get/GetDefault call.
for key in (
    "MarkerKey",
    "CombatActorMarkerKey",
    "CombatCreatureMarkerKey",
):
    require("std::string const " + key in new, "missing static-lifetime string key: " + key)
    require("constexpr char const* " + key not in new, "old char-pointer key retained: " + key)
require("#include <string>" in new, "std::string include missing")

# Current Core Unit::UpdateObjectVisibility(false) is the delayed/coalesced
# NOTIFY_VISIBILITY_CHANGED path. The immediate true sweep must be gone from
# the rights-refresh consumer only.
def section(text, start, end):
    return text.split(start,1)[1].split(end,1)[0]

rights = section(new, "class CurrentRightsRefreshUnit final", "class CurrentCombatUnit final")
require(rights.count("creature->UpdateObjectVisibility(false);") == 1,
        "rights refresh must use exactly one coalesced visibility request")
require("creature->UpdateObjectVisibility(true);" not in rights,
        "rights refresh must not retain immediate visibility sweep")

# Calculation-only hooks can fire for estimates; applied damage/heal and
# periodic damage remain event sources.
for forbidden in (
    "UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN",
    "UNITHOOK_MODIFY_HEAL_RECEIVED",
    "void ModifySpellDamageTaken(",
    "void ModifyHealReceived(",
):
    require(forbidden not in rights, "calculation-only refresh path retained: " + forbidden)
for required in (
    "UNITHOOK_ON_DAMAGE",
    "UNITHOOK_ON_HEAL",
    "UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK",
    "UNITHOOK_ON_AURA_APPLY",
    "UNITHOOK_ON_AURA_REMOVE",
    "UNITHOOK_ON_UNIT_ENTER_COMBAT",
    "UNITHOOK_ON_UNIT_EXIT_COMBAT",
    "UNITHOOK_ON_UNIT_DEATH",
):
    require(required in rights, "required applied/transition refresh path missing: " + required)

# Positive nomination authority remains byte-identical to IP167 and false.
start = "class CurrentTokenBoundInitialOutObserver final"
end = "#if defined(COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT)"
require(section(new,start,end) == section(old,start,end),
        "IP168 must not alter token-bound suppression callback")
require("return true;" not in section(new,start,end),
        "token-bound observer gained positive concealment authority")

# Historical IP164 workflow is evidence-only hygiene: replay source guard on
# immutable reviewed head, not against this intentional descendant delta.
workflow = (ROOT / ".github/workflows/f14-current-rights-refresh.yml").read_text()
require("git worktree add --detach /tmp/ip164 2b46cc76a23d97a0de66274b4a496a30a57c810f" in workflow,
        "historical IP164 source contract is not pinned to immutable head")
require("python3 /tmp/ip164/tests/test_f14_current_rights_refresh_source.py" in workflow,
        "historical IP164 source guard is not replayed from immutable worktree")

# No positive/debt authority is introduced in added runtime lines.
added = "\n".join(line[1:] for line in diff.splitlines()
                  if line.startswith("+") and not line.startswith("+++"))
for forbidden in (".Open(", ".Reserve(", ".Commit(", "DestroyForPlayer(",
                  "SendDirectMessage(", "SetPhaseMask(", "return true;"):
    require(forbidden not in added, "IP168 added forbidden positive/debt action: " + forbidden)

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve IP167 bytes as exact prefix")

print("PASS IP168 rights-refresh performance hardening: coalesced Unit visibility, stable DataMap keys, calc-only hooks removed, hard-FALSE preserved")
