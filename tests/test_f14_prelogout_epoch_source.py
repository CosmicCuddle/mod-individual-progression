"""Exact source regression for the opt-in human viewer stamp logout seam."""
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PATH = "src/IndividualProgressionF14LifecycleScript.cpp"
PARENT = "300b747a9e59e4e1bdb75224e32209e260e43186"
OLD = """    void OnPlayerLogout(Player* player) override
    {
        if (player)
            if (HumanViewerSessionStamp* state =
                    player->CustomData.Get<HumanViewerSessionStamp>(ViewerEpochStampKey))
                state->stamp.Clear();
    }
};"""
NEW = """    // Core LogoutPlayer invokes this before loot/death/map teardown,
    // including redirecting logout where OnPlayerLogout is skipped.
    // Revoke only observer identity; no restoration debt is retired here.
    void OnPlayerBeforeLogout(Player* player) override
    {
        RevokeViewerStamp(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        RevokeViewerStamp(player);
    }

private:
    static void RevokeViewerStamp(Player* player)
    {
        if (player)
            if (HumanViewerSessionStamp* state =
                    player->CustomData.Get<HumanViewerSessionStamp>(ViewerEpochStampKey))
                state->stamp.Clear();
    }
};"""

def require(ok, message):
    if not ok:
        raise SystemExit(message)

parent = subprocess.check_output(["git", "show", PARENT + ":" + PATH], cwd=ROOT).decode()
current = (ROOT / PATH).read_text()
require(parent.count(OLD) == 1 and current == parent.replace(OLD, NEW), "non-exact observer-only change")
require("void OnPlayerBeforeLogout(Player* player) override" not in parent, "old regression absent")
core = pathlib.Path(sys.argv[1])
session = (core / "src/server/game/Server/WorldSession.cpp").read_text()
logout = session.split("void WorldSession::LogoutPlayer(bool save, bool redirecting)", 1)[1].split("METRIC_EVENT", 1)[0]
pre = logout.index("sScriptMgr->OnPlayerBeforeLogout(_player);")
late = logout.index("sScriptMgr->OnPlayerLogout(_player);")
require(pre < logout.index("if (!redirecting)") < late, "early hook ordering changed")
require(pre < logout.index("DoLootRelease"), "early hook after loot teardown")
require("if (!redirecting)" in logout[logout.rfind("if (!redirecting)", 0, late):late], "late hook redirect guard missing")
script = (core / "src/server/game/Scripting/ScriptDefines/PlayerScript.cpp").read_text()
require("CALL_ENABLED_HOOKS(PlayerScript, PLAYERHOOK_ON_BEFORE_LOGOUT, script->OnPlayerBeforeLogout(player));" in script, "dispatcher missing")
require("if (enabledHooks.empty())" in script and "enabledHooks.emplace_back(i)" in script, "default hook subscription changed")
print("PASS exact observer-only pre-logout change and pinned Core redirect ordering")
