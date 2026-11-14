"""Grok M1/L3 combat ownership correction: session-only slot release and poisoned inspection refusal."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "cfed841d56f7a528112c665079cea18266b0a4a0"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-combat-slot-release.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCombatOwnership.h",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/f14_current_combat_ownership_test.cpp",
    "tests/test_f14_combat_slot_release_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected combat slot-release changed-path scope")

for path in (
    "src/IndividualProgressionF14CurrentCombatOwnership.h",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/f14_current_combat_ownership_test.cpp",
):
    diff = subprocess.check_output(["git","diff","-U0",PARENT,"HEAD","--",path], cwd=ROOT, text=True)
    removed = [line for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
    require(not removed, "combat slot-release correction must be insertion-only in " + path)

header = (ROOT / "src/IndividualProgressionF14CurrentCombatOwnership.h").read_text()
for needle in (
    "bool ReleaseActor(ActorKey actor)",
    "slot->used = false;",
    "slot->actor = {};",
    "if (_poisoned)\n                return {};",
):
    require(needle in header, "missing tracker correction: " + needle)

cpp = (ROOT / "src/IndividualProgressionF14CurrentCoreLifecycle.cpp").read_text()
require(cpp.count("CombatTracker().ReleaseActor(actor);") == 1, "actor slot release must occur exactly once")
session = cpp.split("void EndCombatActorSession(Player const* player)",1)[1].split("Creature* CurrentCombatCreature",1)[0]
require("CombatTracker().EndActor(actor);" in session and "CombatTracker().ReleaseActor(actor);" in session,
        "session end must retire target and release exact actor slot")
require(session.index("CombatTracker().EndActor(actor);") < session.index("CombatTracker().ReleaseActor(actor);"),
        "target retirement must precede session slot release")
map_leave = cpp.split("void LeaveCombatActorMap(Player const* player",1)[1].split("void EndCombatActorSession",1)[0]
require("ReleaseActor" not in map_leave, "map handoff must not release session-owned actor slot")
target_end = cpp.split("void EndCombatActorTarget(Player const* player)",1)[1].split("void LeaveCombatActorMap",1)[0]
require("ReleaseActor" not in target_end, "combat/target end must not release session-owned actor slot")

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve IP161 bytes as exact prefix")

print("PASS exact session-only actor slot release, stale-key protection and poisoned InspectActor fail-visible contract")