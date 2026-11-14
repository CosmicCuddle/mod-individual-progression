"""Grok IP163 N1 correction: release combat actor slots after map-key invalidation."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "d3aea3c9612041b05ff7a669fa7906791b5ad7ae"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-combat-session-release.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCombatOwnership.h",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/f14_current_combat_ownership_test.cpp",
    "tests/test_f14_combat_session_release_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP167 combat session-release changed-path scope")

for path in (
    "src/IndividualProgressionF14CurrentCombatOwnership.h",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/f14_current_combat_ownership_test.cpp",
):
    diff = subprocess.check_output(["git","diff","-U0",PARENT,"HEAD","--",path], cwd=ROOT, text=True)
    removed = [line for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
    require(not removed, "IP167 runtime/model correction must be insertion-only in " + path)

header = (ROOT / "src/IndividualProgressionF14CurrentCombatOwnership.h").read_text()
for needle in (
    "bool ReleaseActorSession(std::uint64_t guid, std::uint64_t loginEpoch)",
    "slot->actor.loginEpoch != loginEpoch",
    "slot->used = false;",
):
    require(needle in header, "missing session release contract: " + needle)

cpp = (ROOT / "src/IndividualProgressionF14CurrentCoreLifecycle.cpp").read_text()
for needle in (
    "mutable std::uint64_t releaseGuid = 0;",
    "mutable std::uint64_t releaseLoginEpoch = 0;",
    "marker->releaseGuid = player->GetGUID().GetRawValue();",
    "marker->releaseLoginEpoch = published.loginEpoch;",
    "CombatTracker().ReleaseActorSession(marker->releaseGuid, marker->releaseLoginEpoch);",
):
    require(needle in cpp, "missing lifecycle session-release evidence: " + needle)

map_leave = cpp.split("void LeaveCombatActorMap(Player const* player",1)[1].split("void EndCombatActorSession",1)[0]
require("releaseGuid = 0" not in map_leave and "releaseLoginEpoch = 0" not in map_leave,
        "map leave must preserve release-only session identity")
session = cpp.split("void EndCombatActorSession(Player const* player)",1)[1].split("Creature* CurrentCombatCreature",1)[0]
require(session.index("ReleaseActorSession") < session.index("marker->releaseGuid = 0;"),
        "session release must happen before release-only identity is cleared")
require("return false;" in cpp.split("OnF14ShouldSuppressCreatureInitialOutForViewer",1)[1],
        "positive concealment must remain fail-visible")

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve IP166 bytes as exact prefix")

print("PASS exact login-epoch actor session release survives map-key invalidation and refuses stale sessions")
