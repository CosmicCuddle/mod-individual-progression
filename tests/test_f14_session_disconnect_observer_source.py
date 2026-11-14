"""IP165 closes kept-in-world client disconnect identity before any positive F14 use."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "2b46cc76a23d97a0de66274b4a496a30a57c810f"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-session-disconnect-observer.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "tests/test_f14_session_disconnect_observer_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP165 path scope")

diff = subprocess.check_output(["git","diff","-U0",PARENT,"HEAD","--","src/IndividualProgressionF14CurrentCoreLifecycle.cpp"], cwd=ROOT, text=True)
removed = [line for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
require(not removed, "IP165 current lifecycle source must be insertion-only")

cpp = (ROOT / "src/IndividualProgressionF14CurrentCoreLifecycle.cpp").read_text()
for needle in (
    "COSMIC_IP_F14_SESSION_DISCONNECT_OBSERVER_EXPERIMENT",
    "COSMIC_F14_SESSION_DISCONNECT_EXPERIMENT",
    "PLAYERHOOK_ON_F14_SESSION_DISCONNECTED",
    "class CurrentViewerSessionDisconnect final",
    "void OnF14PlayerSessionDisconnected(Player* player) override",
    "new CurrentViewerSessionDisconnect();",
):
    require(needle in cpp, "missing disconnect observer contract: " + needle)

body = cpp.split("class CurrentViewerSessionDisconnect final",1)[1].split("class CurrentViewerMap final",1)[0]
for needle in ("BeforeDeparture(player);", "RevokePassViewer(player);", "Logout(player);"):
    require(body.count(needle) == 1, "disconnect action missing/duplicated: " + needle)
require(body.index("BeforeDeparture(player);") < body.index("RevokePassViewer(player);") < body.index("Logout(player);"),
        "disconnect order must be debt fence -> pass-token revoke -> viewer epoch logout")
require("LoginIssuer().Issue()" not in body and "BindLogin(" not in body and "Enter(player" not in body,
        "disconnect must not issue fresh viewer identity")
require("IsSocketClosed" not in cpp, "IP map-thread lifecycle must not read mutable WorldSession socket state")

pass_guard = cpp.split("#if defined(COSMIC_IP_F14_CURRENT_PASS_IDENTITY_EXPERIMENT)",1)[1].split("#if defined(COSMIC_IP_F14_CURRENT_RIGHTS_REFRESH_EXPERIMENT)",1)[0]
require("disconnect-safe current viewer identity" in pass_guard, "pass identity must hard-require disconnect-safe observer")

for forbidden in (".Open(", ".Reserve(", ".Commit(", "return true;", "SendDirectMessage(", "DestroyForPlayer("):
    require(forbidden not in body, "disconnect observer gained positive authority: " + forbidden)

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve IP164 bytes as exact prefix")

print("PASS IP165 disconnect: preserve debt, revoke active pass identity, invalidate viewer epoch, no socket read or positive authority")