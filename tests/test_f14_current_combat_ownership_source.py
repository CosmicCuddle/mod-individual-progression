"""F14 current combat ownership is insertion-only, observer-only and default OFF."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "8eeede539f64bc07589a940540060ffe3b04c740"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-current-combat-ownership.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCombatOwnership.h",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/f14_current_combat_ownership_test.cpp",
    "tests/test_f14_current_combat_ownership_source.py",
])
actual = subprocess.check_output(
    ["git", "diff", "--name-only", PARENT, "HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected current-combat changed-path scope")

diff = subprocess.check_output(
    ["git", "diff", "-U0", PARENT, "HEAD", "--", "src/IndividualProgressionF14CurrentCoreLifecycle.cpp"],
    cwd=ROOT, text=True)
removed = [line for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
require(not removed, "current Core lifecycle source must remain insertion-only from IP156")

cpp = (ROOT / "src/IndividualProgressionF14CurrentCoreLifecycle.cpp").read_text()
for needle in (
    "COSMIC_IP_F14_CURRENT_COMBAT_OWNERSHIP_EXPERIMENT",
    "TryReadF14VerifiedActorPublication(",
    "PLAYERHOOK_ON_PLAYER_ENTER_COMBAT",
    "PLAYERHOOK_ON_PLAYER_LEAVE_COMBAT",
    "PLAYERHOOK_ON_UPDATE",
    "PLAYERHOOK_ON_PLAYER_JUST_DIED",
    "UNITHOOK_ON_UNIT_DEATH",
    "OnCreatureRemoveWorld(Creature* creature) override",
    "player->GetVictim()",
    "CombatTracker().Observe(",
    "CombatTracker().EndActor(",
    "CombatTracker().RetireCreature(",
):
    require(needle in cpp, "missing current combat source contract: " + needle)

enter = cpp.split("void OnPlayerEnterCombat(Player* player", 1)[1].split("void OnPlayerUpdate", 1)[0]
require("ObserveCombatTarget(player, player ? player->GetVictim() : nullptr);" in enter,
        "enter-combat must not treat the enemy argument as proof that the bot initiated an attack")

for forbidden in (
    ".Open(", ".Reserve(", ".Commit(",
    "UpdateObjectVisibility(", "DestroyForPlayer(",
    "SendDirectMessage(", "OnF14ShouldSuppress",
):
    require(forbidden not in cpp,
            "combat observer unexpectedly performs positive visibility/restoration action: " + forbidden)

register_diff = subprocess.check_output(
    ["git", "diff", "-U0", PARENT, "HEAD", "--", "docs/CUSTOM_MODIFICATIONS_REGISTER.md"],
    cwd=ROOT, text=True)
old_register = subprocess.check_output(["git", "show", PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve parent bytes as exact prefix")

print("PASS insertion-only current Core combat observer, corrected type name, exact scope and no positive visibility/store action")