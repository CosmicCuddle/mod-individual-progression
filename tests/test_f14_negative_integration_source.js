const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");
const witness = fs.readFileSync(path.join(root, "tests/f14_negative_integration_witness.cpp"), "utf8");
const ledger = fs.readFileSync(path.join(root, "src/IndividualProgressionF14PriorHiddenLedger.h"), "utf8");
const protocol = fs.readFileSync(path.join(root, "src/IndividualProgressionF14ViewerRestoreProtocol.h"), "utf8");
function check(ok, message) { if (!ok) throw new Error(message); }
const harness = witness.split("struct Witness")[1]?.split("static Restore::DecisionFacts")[0];
function preservesRawGuid(source) {
    return source.includes("restore.ObserveViewerSessionEnded(old.guid);") &&
           source.includes("pass.RevokeViewer(old);") &&
           source.includes("if (current == old)");
}
check(preservesRawGuid(harness), "exact ephemeral revoke must coexist with raw-GUID ledger revoke");
const mutant = harness.replace("restore.ObserveViewerSessionEnded(old.guid);",
                               "restore.ObserveViewerSessionEnded(old.loginEpoch);");
check(mutant !== harness && !preservesRawGuid(mutant), "epoch-qualified ledger mutation escaped");
check(ledger.includes("_pairs[i].viewer.guidRaw == viewerGuid") &&
      protocol.includes("_ledger.RevokeViewer(viewerGuid)"),
      "raw-GUID revoke contract changed");
check(harness.includes("pass.BatchWasObserved(token, identity)") &&
      harness.includes("match.incarnation != currentIncarnation"),
      "pass or incarnation guard absent");
for (const forbidden of ["ObserveF14SuppressionDecision", "MayAnswerPositiveSuppression",
                         "RecordConfirmedF14Suppression", "CompleteRestoreOnlyWithClientProof",
                         "ClearFrozenAfterGlobalProof", "SetVisible", "SetPhaseMask"])
    check(!harness.includes(forbidden), "witness gained positive/restore-completion authority: " + forbidden);
check(witness.includes("PendingRestores() == 3") &&
      witness.includes("protocol.GlobalRestoreRequired()") &&
      witness.includes("Pass::Result::Mismatch") &&
      witness.includes("Corr::Result::Capacity") &&
      witness.includes("Corr::Result::RecycledIncarnation"),
      "missing negative debt/ambiguity/incarnation witness");
console.log("PASS F14 negative integration source: exact ephemeral evidence, raw-GUID debt, no completion");
