"""IP161 fixes only the macro-ON combat script/helper C++ name collision."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "390040f57551e16ec016c12058c10c9c229922a7"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-combat-script-name-collision.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/test_f14_combat_script_name_collision_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP161 changed-path scope")

path = "src/IndividualProgressionF14CurrentCoreLifecycle.cpp"
diff = subprocess.check_output(["git","diff","-U0",PARENT,"HEAD","--",path], cwd=ROOT, text=True)
removed = [line[1:] for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
added = [line[1:] for line in diff.splitlines() if line.startswith("+") and not line.startswith("+++")]

expected_removed = sorted([
    "    class CurrentCombatCreature final : public AllCreatureScript",
    "        CurrentCombatCreature() : AllCreatureScript(\"IndividualProgression_F14CurrentCombatCreature\") { }",
    "    new CurrentCombatCreature();",
])
expected_added = sorted([
    "    class CurrentCombatCreatureRemoval final : public AllCreatureScript",
    "        CurrentCombatCreatureRemoval() : AllCreatureScript(\"IndividualProgression_F14CurrentCombatCreature\") { }",
    "    new CurrentCombatCreatureRemoval();",
])
require(sorted(removed) == expected_removed, "IP161 removed more/different runtime source than the three colliding identifiers")
require(sorted(added) == expected_added, "IP161 added more/different runtime source than the three replacement identifiers")

cpp = (ROOT / path).read_text()
require("Creature* CurrentCombatCreature(Player const* actor, Unit* target)" in cpp, "existing helper function must remain unchanged")
require("class CurrentCombatCreature final" not in cpp, "colliding class name still present")
require("new CurrentCombatCreature();" not in cpp, "colliding registration still present")
require("class CurrentCombatCreatureRemoval final" in cpp, "renamed removal script class missing")
require("new CurrentCombatCreatureRemoval();" in cpp, "renamed removal script registration missing")
require('AllCreatureScript("IndividualProgression_F14CurrentCombatCreature")' in cpp, "runtime script registration string must stay unchanged")

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve parent bytes as exact prefix")

print("PASS IP161 exact three-identifier collision correction; helper and script registration identity preserved")