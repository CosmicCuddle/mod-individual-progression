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
    return revoke?.includes("EarnedView(eligibleBot, race, earnedStage)") &&
           !revoke.includes("Floor::WhileGrouped(");
}
check(hasEarnedDeparture(model), "departure must expose exact earned evidence");
mutant = model.replace("EarnedView(eligibleBot, race, earnedStage)",
                       "{Status::Temporary, Era::BurningCrusade, true}");
check(mutant !== model && !hasEarnedDeparture(mutant), "TBC-on-departure mutation escaped");

function exactTeardownWithoutEligibility(source) {
    const revoke = source.split("View Revoke(")[1]?.split("private:")[0];
    return revoke?.indexOf("if (!Complete(identity))") >= 0 &&
           revoke.indexOf("*entry = {identity, race, 0, Era::Invalid, true, false, false};") >= 0 &&
           !revoke.includes("if (!eligibleBot)");
}
check(exactTeardownWithoutEligibility(model), "lost eligibility must not block exact teardown");
mutant = model.replace(/(View Revoke\([\s\S]*?\{)\n\s*\/\/ Teardown/,
                       "$1\n            if (!eligibleBot) return {Status::UnverifiedBot, Era::Invalid, false};\n            // Teardown");
check(mutant !== model && !exactTeardownWithoutEligibility(mutant),
      "lost-eligibility teardown mutation escaped");

check(model.includes("std::array<Entry, Capacity>") && !model.includes("std::vector"),
      "consumer storage must remain bounded");
check(test.includes("combinations") && test.includes("Status::Capacity") &&
      test.includes("Status::Ambiguous") && test.includes("Status::Stale"),
      "missing matrix or failure-state witnesses");
function exactCurrent(source) {
    const current = source.split("View Current(")[1]?.split("View Revoke(")[0];
    return current?.includes("Same(input.identity, entry->identity)") &&
           current.includes("entry->race == input.race") &&
           current.includes("entry->groupStage == input.groupStage");
}
check(exactCurrent(model), "Current must reject a changed session/map/membership context");
mutant = model.replace(/(View Current\([\s\S]*?)Same\(input\.identity, entry->identity\)/,
                       "$1true");
check(mutant !== model && !exactCurrent(mutant), "G1 changed-identity read mutation escaped");
for (const caseName of ["groupB.identity.membershipGeneration = 1",
                       "groupB.identity.membershipGeneration = 5",
                       "++changed.identity.sessionEpoch", "++changed.identity.mapGeneration",
                       "++changed.identity.membershipGeneration",
                       "ordered.identity.mapGeneration = 11", "C::Tracker<1> blind"])
    check(test.includes(caseName), "missing producer-contract fixture: " + caseName);
console.log("PASS F14 group-era consumer source: exact contexts, stale revoke, earned departure, no writes");
