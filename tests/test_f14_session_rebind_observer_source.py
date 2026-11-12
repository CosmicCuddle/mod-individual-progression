"""IP158 consumes Core17 reconnect-session boundary without granting concealment."""
import pathlib, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "394b3017820a39bf3b7c543602cfc2cd3dc1a9fc"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-session-rebind-observer.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "src/IndividualProgressionF14LifecycleScript.cpp",
    "tests/test_f14_session_rebind_observer_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP158 path scope")

for path in (
    "src/IndividualProgressionF14CurrentCoreLifecycle.cpp",
    "src/IndividualProgressionF14LifecycleScript.cpp",
):
    diff = subprocess.check_output(["git","diff","-U0",PARENT,"HEAD","--",path], cwd=ROOT, text=True)
    removed = [line for line in diff.splitlines() if line.startswith("-") and not line.startswith("---")]
    require(not removed, "IP158 runtime source must be insertion-only: " + path)

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve parent bytes as exact prefix")

cpp = (ROOT / "src/IndividualProgressionF14CurrentCoreLifecycle.cpp").read_text()
for needle in (
    "COSMIC_IP_F14_SESSION_REBIND_OBSERVER_EXPERIMENT",
    "COSMIC_F14_SESSION_REBIND_EXPERIMENT",
    "PLAYERHOOK_ON_F14_SESSION_REBOUND",
    "void OnF14PlayerSessionRebound(Player* player) override",
):
    require(needle in cpp, "missing current rebind contract: " + needle)

body = cpp.split("void OnF14PlayerSessionRebound(Player* player) override",1)[1].split("void OnPlayerBeforeLogout",1)[0]
require(body.count("BeforeDeparture(player);") == 1, "rebind must preserve old restoration obligations first")
require(body.count("Logout(player);") == 1, "rebind must invalidate old current viewer identity")
require(body.index("BeforeDeparture(player);") < body.index("Logout(player);"), "departure must precede identity teardown")
require(body.index("Logout(player);") < body.index("isNormalAccount(player)"), "new session account class must be rechecked after teardown")
require(body.index("isNormalAccount(player)") < body.index("LoginIssuer().Issue()"), "fresh epoch only after eligibility recheck")
require(body.index("LoginIssuer().Issue()") < body.index("BindLogin("), "fresh epoch must bind new identity")
require(body.index("BindLogin(") < body.index("Enter(player, player->GetMap())"), "fresh login identity must precede map generation")

legacy = (ROOT / "src/IndividualProgressionF14LifecycleScript.cpp").read_text()
legacy_body = legacy.split("void OnF14PlayerSessionRebound(Player* player) override",1)[1].split("private:",1)[0]
require("RevokeViewerStamp(player);" in legacy_body and "OnPlayerLogin(player);" in legacy_body, "legacy epoch must also refresh on reconnect")
require(legacy_body.index("RevokeViewerStamp(player);") < legacy_body.index("OnPlayerLogin(player);"), "legacy old epoch must be revoked first")

for forbidden in (
    ".Open(", ".Reserve(", ".Commit(", "OnF14ShouldSuppress",
    "SendDirectMessage(", "UpdateObjectVisibility(", "DestroyForPlayer(",
):
    require(forbidden not in body, "rebind observer gained positive visibility/restoration authority: " + forbidden)

print("PASS IP158 reconnect: old debt fenced, old epoch revoked, account rechecked, fresh viewer/map identity issued; no hide authority")