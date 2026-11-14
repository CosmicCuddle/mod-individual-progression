"""IP160 consumes Core18 token nomination only as observer evidence; suppression is forced FALSE."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "013a2e0e63e41bbd7b21ddf31c6fa4f6fc8ea554"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-token-bound-initial-out-observer.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/test_f14_token_bound_initial_out_observer_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP160 changed-path scope")

cpp_path = "src/IndividualProgressionF14CurrentCoreLifecycle.cpp"
diff = subprocess.check_output(["git","diff","-U0",PARENT,"HEAD","--",cpp_path], cwd=ROOT, text=True)
removed = [line for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
require(not removed, "IP160 runtime source must be insertion-only")

cpp = (ROOT / cpp_path).read_text()
for needle in (
    "COSMIC_IP_F14_TOKEN_BOUND_INITIAL_OUT_OBSERVER_EXPERIMENT",
    "COSMIC_F14_TOKEN_BOUND_CREATURE_OUT_QUERY_EXPERIMENT",
    "WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_INITIAL_OUT_TOKEN_QUERY",
    "OnF14ShouldSuppressCreatureInitialOutForViewer(",
    "PassTracker().IsActive(passToken, identity)",
    "CombatTracker().ActiveAttackers(key) == 0",
):
    require(needle in cpp, "missing token-bound observer contract: " + needle)

block = cpp.split("class CurrentTokenBoundInitialOutObserver final",1)[1].split("class CurrentViewerSessionRebind final",1)[0]
require("return true;" not in block, "observer must never return positive suppression")
require(block.count("return false;") >= 5, "observer must fail visible at every missing-proof branch")
for forbidden in (
    ".Open(", ".Reserve(", ".Commit(", "RecordConfirmedF14Suppression",
    "SendDirectMessage(", "UpdateObjectVisibility(", "DestroyForPlayer(",
):
    require(forbidden not in block, "observer gained positive/store/packet authority: " + forbidden)

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve parent bytes as exact prefix")

print("PASS IP160 token-bound nomination proves pass+viewer+combat evidence but is hard-forced visible with no store/packet authority")