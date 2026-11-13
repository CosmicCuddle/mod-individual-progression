// Source-only contract for the unwired temporary group-era model.
const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");
const model = fs.readFileSync(path.join(root, "src/IndividualProgressionF14GroupSyncEraFloor.h"), "utf8");
const groupPolicy = fs.readFileSync(path.join(root, "src/IndividualProgressionGroupPolicy.h"), "utf8");
const tests = fs.readFileSync(path.join(root, "tests/f14_group_sync_era_floor_test.cpp"), "utf8");
function requireContract(ok, message) {
    if (!ok) throw new Error(message);
}

for (const required of [
    "WhileGrouped(bool eligibleBot", "AfterLeaving(bool eligibleBot",
    "GroupPolicy::GetExpansion(earnedStage)", "GroupPolicy::GetExpansion(groupStage)",
    "GroupPolicy::IsExpansionRace(race)", "IsSupportedRace(race)", "Era::BurningCrusade",
    "Status::UnverifiedBot", "Status::InvalidRace", "Status::InvalidStage",
    "Status::EarnedFloorMissing", "struct DepartureEvidence",
])
    requireContract(model.includes(required), "missing group-floor contract: " + required);
for (const required of ["FirstTbcStage = 8", "FirstWrathStage = 13", "LastSupportedStage = 18"])
    requireContract(groupPolicy.includes(required), "changed expansion boundary: " + required);

for (const forbidden of [
    "Player*", "Group*", "WorldSession", "RandomPlayerbot", "RandomBotMgr",
    "ForceUpdateProgressionState", "UpdateProgressionState", "SetProgression",
    "GetPlayerProgressionFromQuests", "RemoveRewardedQuest", "RewardedQuest",
    "Database", "SQL", "SendPacket", "SetPhaseMask", "ShouldSuppress",
])
    requireContract(!model.includes(forbidden), "pure group-era model gained runtime/write dependency: " + forbidden);
for (const required of [
    "normal bot can downsync to Vanilla", "Blood Elf has a hard TBC minimum",
    "Draenei has a hard TBC minimum", "TBC-race bot can rise to Wrath",
    "leaving exposes earned Wrath era", "No earned quest/stage mutation",
    "missing earned floor is not usable solo evidence",
    "Missing marker is not silently granted", "Race does not establish bot eligibility",
])
    requireContract(tests.includes(required), "missing focused test scenario: " + required);

function departureKeepsEarnedEvidence(source) {
    const start = source.indexOf("constexpr DepartureEvidence AfterLeaving(");
    if (start < 0) return false;
    const body = source.slice(start);
    return body.includes("return {Status::EarnedFloorMissing, earned};") &&
           body.includes("return {Status::Available, earned};") &&
           !body.includes("Era::BurningCrusade");
}
requireContract(departureKeepsEarnedEvidence(model),
                "departure must expose earned evidence, never apply a healthy TBC floor");
const oldM1Behavior = model.replace(
    "return {Status::EarnedFloorMissing, earned};",
    "return {Status::Available, Era::BurningCrusade};"
);
requireContract(oldM1Behavior !== model && !departureKeepsEarnedEvidence(oldM1Behavior),
                "source guard must reject the old TBC-on-leave semantic bug");

console.log("PASS F14 group-era source: explicit bot eligibility, 0/8/13 boundaries, no progression write authority");
