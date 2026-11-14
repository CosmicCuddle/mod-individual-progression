"""IP159 pass-token identity correlation is observer-only, additive and fail-visible."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "d72cb188ce763181b43ceced79fdae9c69b88829"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-current-pass-identity.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "src/IndividualProgressionF14CurrentPassIdentity.h",
    "tests/f14_current_pass_identity_test.cpp",
    "tests/test_f14_current_pass_identity_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP159 changed-path scope")

cpp_path = "src/IndividualProgressionF14CurrentCoreLifecycle.cpp"
diff = subprocess.check_output(["git","diff","-U0",PARENT,"HEAD","--",cpp_path], cwd=ROOT, text=True)
removed = [line for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
require(not removed, "IP159 current lifecycle runtime source must be insertion-only")

old = subprocess.check_output(["git","show",PARENT + ":" + cpp_path], cwd=ROOT, text=True)
new = (ROOT / cpp_path).read_text()
viewer_pass = old.split("    class CurrentViewerPass final",1)[1].split("#if defined(COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT)",1)[0]
require(viewer_pass in new, "established CurrentViewerPass class must remain byte-exact")

for needle in (
    "COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT",
    "IndividualProgressionF14CurrentPassIdentity.h",
    "WORLDOBJECTHOOK_ON_F14_VIEWER_VISIBILITY_PASS_BEGIN",
    "WORLDOBJECTHOOK_ON_F14_VIEWER_VISIBILITY_PASS_BATCH_SEND_ATTEMPT",
    "WORLDOBJECTHOOK_ON_F14_VIEWER_VISIBILITY_PASS_END",
    "PassTracker().Begin(token, CurrentPassViewer(viewer))",
    "PassTracker().ObserveBatch(token, CurrentPassViewer(viewer))",
    "PassTracker().End(token, CurrentPassViewer(viewer))",
    "PassTracker().RevokeViewer(viewer->GetGUID().GetRawValue())",
    "PLAYERHOOK_ON_F14_SESSION_REBOUND",
    "ALLMAPHOOK_ON_F14_BEFORE_PLAYER_MAP_REMOVAL",
):
    require(needle in new, "missing pass-identity contract: " + needle)

pass_block = new.split("class CurrentPassIdentityObserver final",1)[1].split("class CurrentViewerSessionRebind final",1)[0]
for forbidden in (
    ".Open(", ".Reserve(", ".Commit(",
    "OnF14ShouldSuppress", "ShouldF14Suppress",
    "SendDirectMessage(", "UpdateObjectVisibility(", "DestroyForPlayer(",
    "RecordConfirmedF14Suppression",
):
    require(forbidden not in pass_block, "pass correlation gained positive authority: " + forbidden)

header = (ROOT / "src/IndividualProgressionF14CurrentPassIdentity.h").read_text()
for needle in ("Begin(", "ObserveBatch(", "BatchWasObserved(", "End(", "RevokeViewer(", "MismatchLocked", "poisoned"):
    require(needle in header, "missing pure correlation invariant: " + needle)

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve parent bytes as exact prefix")

print("PASS IP159 token identity is additive, exact-viewer bound, lifecycle revoked and has no positive concealment/store authority")