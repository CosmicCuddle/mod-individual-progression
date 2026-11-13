const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");
const model = fs.readFileSync(path.join(root, "src/IndividualProgressionF14GroupEraConsumer.h"), "utf8");
const test = fs.readFileSync(path.join(root, "tests/f14_group_era_consumer_test.cpp"), "utf8");
function check(condition, message) { if (!condition) throw new Error(message); }

for (const field of ["botGuid", "sessionEpoch", "mapGeneration", "mapId", "groupGuid",
                     "membershipGeneration", "leaderGuid", "leaderRevision"])
    check(model.includes(field), "missing exact context field: " + field);
for (const event of ["Leave", "Disband", "Kick", "LeaderChange", "Relog", "MapChange",
                     "SessionInvalidation"])
    check(model.includes(event) && test.includes("Departure::" + event), "missing departure: " + event);
for (const forbidden of ["Player*", "Group*", "WorldSession", "ForceUpdateProgressionState",
                         "UpdateProgressionState", "RewardedQuest", "Database", "SetPhaseMask",
                         "SendPacket", "ShouldSuppress", "ViewerRestoreProtocol", "PriorHiddenLedger"])
    check(!model.includes(forbidden), "pure consumer gained runtime/write/visibility authority: " + forbidden);

function hasExactRevocationFence(source) {
    const revoke = source.split("View Revoke(")[1]?.split("private:")[0];
    return revoke?.includes("if (order < 0)") && revoke.includes("Status::Stale") &&
           revoke.includes("if (order == 0 && !Same(identity, entry->identity))") &&
           revoke.includes("entry->active = false;");
}
check(hasExactRevocationFence(model), "stale and ambiguous revocation must be explicit");
let mutant = model.replace(/(View Revoke\([\s\S]*?)if \(order < 0\)/,
                           "$1if (false)");
check(mutant !== model && !hasExactRevocationFence(mutant), "stale-revoke mutation escaped");

function hasEarnedDeparture(source) {
    const revoke = source.split("View Revoke(")[1]?.split("private:")[0];
    return revoke?.includes("EarnedView(true, race, earnedStage)") &&
           !revoke.includes("Floor::WhileGrouped(");
}
check(hasEarnedDeparture(model), "departure must expose exact earned evidence");
mutant = model.replace("EarnedView(true, race, earnedStage)",
                       "{Status::Temporary, Era::BurningCrusade, true}");
check(mutant !== model && !hasEarnedDeparture(mutant), "TBC-on-departure mutation escaped");

check(model.includes("std::array<Entry, Capacity>") && !model.includes("std::vector"),
      "consumer storage must remain bounded");
check(test.includes("combinations") && test.includes("Status::Capacity") &&
      test.includes("Status::Ambiguous") && test.includes("Status::Stale"),
      "missing matrix or failure-state witnesses");
console.log("PASS F14 group-era consumer source: exact contexts, stale revoke, earned departure, no writes");
