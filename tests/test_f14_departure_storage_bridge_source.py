"""Exact negative lifecycle wiring; no positive admission or old protocol mutation."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "9af3dbb0ef9d15295d20dd77238ca901cbdd5e60"
def require(ok, message):
    if not ok:
        raise SystemExit(message)

protocol = "src/IndividualProgressionF14ViewerRestoreProtocol.h"
old = subprocess.check_output(["git", "show", PARENT + ":" + protocol], cwd=ROOT).decode()
new = (ROOT / protocol).read_text()
anchor = "        bool UnattributedHideRisk() const { return _unattributedHideRisk; }"
addition = """
        // Read-only negative lifecycle routing. A raw GUID deliberately
        // matches every retained login epoch, never grants visibility rights.
        bool HasRecordedViewer(std::uint64_t viewerGuid) const
        {
            if (!viewerGuid)
                return false;
            for (std::size_t i = 0; i < _size; ++i)
                if (_records[i].viewer.guidRaw == viewerGuid)
                    return true;
            return false;
        }"""
require(old.count(anchor) == 1 and new == old.replace(anchor, anchor + "\n" + addition),
        "original protocol modified beyond read-only viewer lookup")
cpp = (ROOT / "src/IndividualProgressionF14CurrentCoreLifecycle.cpp").read_text()
for callback, end in [
    ("void OnPlayerBeforeLogout(Player* player) override", "void OnPlayerLogout"),
    ("void OnPlayerLogout(Player* player) override", "class CurrentViewerMap"),
    ("void OnF14BeforePlayerMapRemoval", "class CurrentViewerPass"),
]:
    body = cpp.split(callback,1)[1].split(end,1)[0]
    require(body.count("BeforeDeparture(player);") == 1, "missing negative lifecycle call " + callback)
    next_op = "Logout(player);" if "Logout" in callback else "GetMarker(player)"
    require(body.index("BeforeDeparture(player);") < body.index(next_op), "negative call after identity teardown")
require("RestorationStore().BeforeViewerDeparture(" in cpp, "missing actual store route")
for forbidden in (".Open(", ".Reserve(", ".Commit(", "OnF14ShouldSuppress", "SendDirectMessage", "UpdateObjectVisibility"):
    require(forbidden not in cpp, "unexpected positive/runtime action " + forbidden)
print("PASS exact original protocol plus read-only lookup and negative-only current Core callbacks")
