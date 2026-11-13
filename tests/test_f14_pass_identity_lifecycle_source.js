// Source-only guard: the pass tracker cannot authorize concealment or retire debt.
const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");
const header = fs.readFileSync(path.join(root, "src/IndividualProgressionF14CurrentPassIdentity.h"), "utf8");
const runtime = fs.readFileSync(path.join(root, "src/IndividualProgressionF14CurrentCoreLifecycle.cpp"), "utf8");
function requireContract(condition, message) {
    if (!condition) throw new Error(message);
}
for (const forbidden of [
    "RestorationStore", "BeforeViewerDeparture", "PriorHidden", "RestoreDebt",
    "SetPhaseMask", "DestroyForPlayer", "SendDirectMessage", "SendPacket",
    "Reserve(", "Commit(", "IsConcealmentAuthorized", "ShouldSuppress",
    "ClientAck", "GrantStage", "SetProgression",
])
    requireContract(!header.includes(forbidden), "pass tracker gained authority/debt dependency: " + forbidden);

requireContract(header.includes("std::array<Slot, Capacity>"), "active evidence must be bounded");
requireContract(header.includes("std::array<Closed, ClosedCapacity>"), "closed evidence must be bounded");
requireContract(header.includes("_admissionsClosed = true"), "exhausted history must refuse new passes");
requireContract(header.includes("std::lock_guard<std::mutex>"), "tracker state must be synchronized");
requireContract(header.includes("RevokeViewer(ViewerIdentity identity)"), "revoke needs exact identity");
const begin = header.split("Result Begin(")[1]?.split("Result ObserveBatch(")[0];
function retainsInvalidBeginToken(source) {
    return source?.includes("RememberClosedLocked(token, {}, false)") &&
           source.includes("prior->quarantined = true;");
}
requireContract(retainsInvalidBeginToken(begin),
                "a nonzero invalid-identity Begin must retain or quarantine its token");
const replayRegression = begin.replace("RememberClosedLocked(token, {}, false);", "");
requireContract(replayRegression !== begin && !retainsInvalidBeginToken(replayRegression),
                "source guard must reject loss of the invalid-Begin replay fence");
function admissionsStopAtSaturation(source) {
    return source?.includes("if (_admissionsClosed)") &&
           source.includes("return RefuseLocked();");
}
requireContract(admissionsStopAtSaturation(begin),
                "new Begin admissions must stop after bounded history saturation");
const saturationRegression = begin.replace("if (_admissionsClosed)", "if (false)");
requireContract(saturationRegression !== begin && !admissionsStopAtSaturation(saturationRegression),
                "source guard must reject bypass of hard saturation");
const legacyRevoke = header.split("std::size_t RevokeViewer(std::uint64_t)")[1]?.split(
    "// Reserved for independently proven whole-table corruption.")[0];
requireContract(legacyRevoke?.includes("return 0;") && !legacyRevoke.includes("_slots"),
                "legacy GUID-only revoke must refuse without erasing evidence");

const revoke = runtime.split("void RevokePassViewer(Player const* viewer)")[1]?.split("#endif")[0];
requireContract(revoke?.includes("RevokeViewer(viewer->GetGUID().GetRawValue())"),
                "runtime observer must remain byte-compatible with Claude's active branch");

const observer = runtime.split("class CurrentPassIdentityObserver final")[1]?.split(
    "class CurrentPassIdentityLifecycle final")[0];
requireContract(Boolean(observer), "current pass identity observer is missing");
for (const forbidden of ["return true;", "RecordConfirmedF14Suppression", ".Open(", ".Reserve(", ".Commit("])
    requireContract(!observer?.includes(forbidden), "pass observer gained positive authority: " + forbidden);

console.log("PASS F14 pass identity source: bounded, legacy revoke refuses, no concealment or restoration-debt authority");
