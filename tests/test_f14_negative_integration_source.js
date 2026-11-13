const fs = require("node:fs");
const path = require("node:path");
const root = path.resolve(__dirname, "..");
const witness = fs.readFileSync(path.join(root, "tests/f14_negative_integration_witness.cpp"), "utf8");
const ledger = fs.readFileSync(path.join(root, "src/IndividualProgressionF14PriorHiddenLedger.h"), "utf8");
const protocol = fs.readFileSync(path.join(root, "src/IndividualProgressionF14ViewerRestoreProtocol.h"), "utf8");
const pass = fs.readFileSync(path.join(root, "src/IndividualProgressionF14CurrentPassIdentity.h"), "utf8");
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
function ledgerRevokesEveryEpoch(source) {
    const revoke = source.split("void RevokeViewer(std::uint64_t viewerGuid)")[1]?.split(
        "// Config disable")[0];
    return revoke?.includes("_pairs[i].viewer.guidRaw == viewerGuid") &&
           revoke.includes("_pairs[i].restorePending = true") &&
           !revoke.includes("loginEpoch");
}
check(ledgerRevokesEveryEpoch(ledger) && protocol.includes("_ledger.RevokeViewer(viewerGuid)"),
      "raw-GUID revoke contract changed");
const ledgerEpochMutant = ledger.replace("_pairs[i].viewer.guidRaw == viewerGuid",
                                         "_pairs[i].viewer.loginEpoch == viewerGuid");
check(ledgerEpochMutant !== ledger && !ledgerRevokesEveryEpoch(ledgerEpochMutant),
      "ledger raw-GUID-to-epoch mutation escaped");
function staleRevokeKeepsNewer(source) {
    const revoke = source.split("void RevokeExact(")[1]?.split("Corr::Correlation TrySend")[0];
    return revoke?.includes("if (current == old)") &&
           revoke.includes("correlation.ObserveViewerSessionEnded(old.guid);");
}
check(staleRevokeKeepsNewer(harness), "stale revoke may drop newer correlation evidence");
const staleMutant = harness.replace("if (current == old)", "if (true)");
check(staleMutant !== harness && !staleRevokeKeepsNewer(staleMutant),
      "stale-revoke broad correlation cleanup mutation escaped");
check(harness.includes("pass.BatchWasObserved(token, identity)") &&
      harness.includes("match.incarnation != currentIncarnation"),
      "pass or incarnation guard absent");
for (const forbidden of ["ObserveF14SuppressionDecision", "MayAnswerPositiveSuppression",
                         "RecordConfirmedF14Suppression", "CompleteRestoreOnlyWithClientProof",
                         "ClearFrozenAfterGlobalProof", "SetVisible", "SetPhaseMask"])
    check(!harness.includes(forbidden), "witness gained positive/restore-completion authority: " + forbidden);
function debtCannotBeCompleted(source) {
    return !source.includes("CompleteRestoreOnlyWithClientProof(") &&
           !source.includes("ClearFrozenAfterGlobalProof(") &&
           !source.includes("RequestGlobalRestore(false)");
}
check(debtCannotBeCompleted(harness) &&
      !protocol.includes("_ledger.CompleteRestoreOnlyWithClientProof(") &&
      !protocol.includes("_ledger.ClearFrozenAfterGlobalProof("),
      "pass/correlation/protocol acquired restoration-completion authority");
const debtMutant = harness.replace("pass.RevokeViewer(old);",
                                   "pass.RevokeViewer(old); restore.ClearFrozenAfterGlobalProof(true);");
check(debtMutant !== harness && !debtCannotBeCompleted(debtMutant),
      "global recovery reset mutation escaped");
const completionMutant = harness.replace("pass.RevokeViewer(old);",
    "pass.RevokeViewer(old); restore.CompleteRestoreOnlyWithClientProof({}, {}, 1, true);");
check(completionMutant !== harness && !debtCannotBeCompleted(completionMutant),
      "client-proof completion mutation escaped");
function viewerLocalQuarantine(source) {
    const lookup = source.split("bool IsQuarantinedLocked(")[1]?.split("void QuarantineLocked(")[0];
    const apply = source.split("void QuarantineLocked(")[1]?.split("void RememberClosedLocked(")[0];
    return lookup?.includes("slot.used && slot.quarantined && slot.identity == identity") &&
           lookup.includes("_closed[i].quarantined && _closed[i].identity == identity") &&
           apply?.includes("slot.used && slot.identity == identity") &&
           apply.includes("_closed[i].identity == identity");
}
check(viewerLocalQuarantine(pass), "pass quarantine must remain exact-viewer local");
const spreadMutant = pass.replace(/(void QuarantineLocked\([\s\S]*?)slot\.used && slot\.identity == identity/,
                                  "$1slot.used");
check(spreadMutant !== pass && !viewerLocalQuarantine(spreadMutant),
      "unrelated-viewer quarantine spread mutation escaped");
check(witness.includes("PendingRestores() == 3") &&
      witness.includes("protocol.GlobalRestoreRequired()") &&
      witness.includes("Pass::Result::Mismatch") &&
      witness.includes("Corr::Result::Capacity") &&
      witness.includes("Corr::Result::RecycledIncarnation"),
      "missing negative debt/ambiguity/incarnation witness");
function hasAdditionalNegativeRoutes(source) {
    const handoff = source.split("ObserveProposedBatchFlush(nextKey, otherCreature")[1]?.split(
        "CHECK(!witness.TrySend(old")[0];
    return source.includes("ObserveLinkTransition(nextKey, otherCreature") &&
           source.includes("ObserveProposedBatchFlush(nextKey, otherCreature") &&
           handoff?.includes("CHECK(protocol.NeedsRestore(nextKey, otherCreature));") &&
           source.includes("unknownEnd.ObservePassEnded(oldKey, 41)") &&
           source.includes("Corr::Result::PassClosed") &&
           source.includes("correlations.ObserveViewerRightsOrEraChanged(other.guid)") &&
           source.includes("protocol.ObserveCreatureRetired(creature)") &&
           source.includes("protocol.ObserveMapUnloaded(1, 2)") &&
           source.includes("recycled.GlobalRestoreRequired()");
}
check(hasAdditionalNegativeRoutes(witness), "missing send/receipt, unknown-end or creature-retirement route");
const receiptMutant = witness.replace(
    "CHECK(protocol.NeedsRestore(nextKey, otherCreature));\n    CHECK(protocol.LedgerView().PendingRestores() == 2);",
    "CHECK(protocol.CompleteRestoreOnlyWithClientProof(nextKey, otherCreature, 9002, true));\n    CHECK(protocol.LedgerView().PendingRestores() == 2);");
check(receiptMutant !== witness && !hasAdditionalNegativeRoutes(receiptMutant),
      "send-attempt-to-client-receipt mutation escaped");
console.log("PASS F14 negative integration source: exact ephemeral evidence, raw-GUID debt, no completion");
