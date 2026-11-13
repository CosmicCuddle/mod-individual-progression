const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const preflight = require("./f14_composition_preflight.js");

test("config additions and removals are values; comments cannot assert a default", () => {
    assert.deepEqual(preflight.configDefaults(
        "+# IndividualProgression.F14.Enable = 1\n+IndividualProgression.F14.Enable = 0\n" +
        "-IndividualProgression.F14.Enable = 1"),
        [{side: "added", key: "IndividualProgression.F14.Enable", value: "0"},
         {side: "removed", key: "IndividualProgression.F14.Enable", value: "1"}]);
    assert.equal(preflight.configDefaults(null), "UNKNOWN");
});

test("overlap classifies Register separately from executable runtime paths", () => {
    const pairs = preflight.overlaps([
        {name: "a", paths: ["docs/CUSTOM_MODIFICATIONS_REGISTER.md", "src/Shared.cpp"]},
        {name: "b", paths: ["docs/CUSTOM_MODIFICATIONS_REGISTER.md", "src/Shared.cpp"]},
        {name: "c", paths: "UNKNOWN"}
    ]);
    assert.equal(pairs.length, 1);
    assert.equal(pairs[0].registerOverlap, true);
    assert.deepEqual(pairs[0].runtimeOverlap, ["src/Shared.cpp"]);
});

test("object-level fallback distinguishes content, mode and unavailable tree entries", () => {
    const a = "a".repeat(40), b = "b".repeat(40);
    const candidates = [
        {name: "a", HEAD: a, paths: ["same.h", "different.h", "mode.h", "unknown.h"]},
        {name: "b", HEAD: b, paths: ["same.h", "different.h", "mode.h", "unknown.h"]},
        {name: "supplied-only", HEAD: a, paths: ["same.h"],
         verification: "EXTERNAL SUPPLIED METADATA ONLY; recheck branch before use"}
    ];
    const entry = (mode, object) => ({mode, type: "blob", object: object.repeat(40)});
    const entries = {"same.h": [entry("100644", "1"), entry("100644", "1")],
                     "different.h": [entry("100644", "2"), entry("100644", "3")],
                     "mode.h": [entry("100644", "4"), entry("100755", "4")],
                     "unknown.h": [entry("100644", "5"), null]};
    const report = preflight.objectOverlaps(candidates, (head, p) => entries[p][head === a ? 0 : 1]);
    assert.deepEqual(report.map(x => [x.path, x.verdict]), [
        ["same.h", "IDENTICAL_TREE_ENTRY"], ["different.h", "DIVERGENT_TREE_ENTRIES"],
        ["mode.h", "DIVERGENT_TREE_ENTRIES"], ["unknown.h", "UNKNOWN"]]);
    assert.equal(report[0].claims.length, 2); // Supplied metadata is not a local object claim.
    assert.deepEqual(preflight.dependencyEdges([
        {name: "root", HEAD: a, parent: "c".repeat(40)},
        {name: "child", HEAD: b, parent: a},
        {name: "unmatched", HEAD: "d".repeat(40), parent: "e".repeat(40)}
    ], "c".repeat(40)).map(x => x.parentCandidate), ["COMMON_BASE", "root", "UNKNOWN"]);
});

test("common base comes from the graph, not the first manifest row", () => {
    const base = "a".repeat(40), parent = "b".repeat(40), child = "c".repeat(40);
    const specs = [
        {name: "child", expectedHead: child, parent},
        {name: "parent", expectedHead: parent, parent: base}
    ];
    assert.equal(preflight.commonBase(specs), base);
    assert.equal(preflight.commonBase([...specs].reverse()), base);
    assert.deepEqual(preflight.dependencyEdges(specs.map(s =>
        ({name: s.name, HEAD: s.expectedHead, parent: s.parent})), base)
        .map(x => [x.candidate, x.parentCandidate]),
        [["child", "parent"], ["parent", "COMMON_BASE"]]);
    assert.equal(preflight.commonBase([...specs,
        {name: "unrelated", expectedHead: "d".repeat(40), parent: "e".repeat(40)}]), "UNKNOWN");
});

test("pinned local candidates have only Register overlap; external social data stays labeled", () => {
    const file = path.resolve(__dirname, "../docs/F14_REGISTER_RECONCILIATION_MANIFEST_2026-11-13.json");
    const manifest = JSON.parse(fs.readFileSync(file, "utf8"));
    const report = preflight.inspect(manifest, path.dirname(file));
    assert.equal(report.candidates.length, 6);
    assert.equal(report.overlaps.length, 15);
    assert.ok(report.overlaps.every(x => x.common.length === 1 && x.registerOverlap &&
                                       x.runtimeOverlap.length === 0));
    assert.deepEqual(report.duplicateFeatureIds, []);
    assert.ok(report.candidates[2].requiresParentTree); // #176 needs #175.
    assert.ok(report.candidates[3].requiresParentTree); // #177 needs #174.
    assert.equal(report.candidates[5].verification,
                 "EXTERNAL SUPPLIED METADATA ONLY; recheck branch before use");
    assert.equal(report.candidates[5].configDefaults["IndividualProgression.F14.RandomBotSocialIsolation"], "0");
});

test("exact external Git diff verifies paths and all three config defaults", () => {
    const file = path.resolve(__dirname, "../docs/F14_REGISTER_RECONCILIATION_MANIFEST_V5_2026-11-13.json");
    const manifest = JSON.parse(fs.readFileSync(file, "utf8"));
    const report = preflight.inspect(manifest, path.dirname(file));
    const social = report.candidates[5];
    if (social.verification === "UNKNOWN") {
        assert.equal(social.paths, "UNKNOWN");
        assert.equal(social.configDefaults, "UNKNOWN");
        return; // Do not silently trust supplied metadata in an incomplete clone.
    }
    assert.equal(social.verification, "LOCAL EXACT GIT OBJECTS; moving branch not checked");
    assert.equal(social.HEAD, manifest.externalRows[0].head);
    assert.equal(report.commonBase, "aed05a92f262362608f5d643c8570b504189aae0");
    assert.deepEqual(social.problems, []);
    assert.equal(social.paths.length, 12);
    assert.equal(Object.keys(social.configDefaults).length, 3);
    assert.deepEqual(report.objectOverlaps.map(x => [x.path, x.verdict]),
                     [["docs/CUSTOM_MODIFICATIONS_REGISTER.md", "DIVERGENT_TREE_ENTRIES"]]);
    assert.deepEqual(report.dependencyEdges.filter(x => x.parentCandidate !== "COMMON_BASE")
                     .map(x => [x.candidate, x.parentCandidate]),
                     [["IP #176 group consumer", "IP #175 group floor"],
                      ["IP #177 negative witness", "IP #174 pass lifecycle"]]);
    const reordered = structuredClone(manifest);
    reordered.candidates.unshift(reordered.candidates.splice(2, 1)[0]); // Child #176 first.
    const reorderedReport = preflight.inspect(reordered, path.dirname(file));
    assert.equal(reorderedReport.commonBase, report.commonBase);
    assert.equal(reorderedReport.candidates[0].requiresParentTree, true);
    assert.deepEqual(reorderedReport.dependencyEdges.filter(x => x.parentCandidate !== "COMMON_BASE")
                     .map(x => [x.candidate, x.parentCandidate]),
                     [["IP #176 group consumer", "IP #175 group floor"],
                      ["IP #177 negative witness", "IP #174 pass lifecycle"]]);
    const bad = structuredClone(manifest);
    bad.externalRows[0].configDefaults["IndividualProgression.F14.RandomBotSocialIsolation"] = "1";
    assert.ok(preflight.inspect(bad, path.dirname(file)).candidates[5].problems
              .includes("supplied external config defaults differ from exact local Git diff"));
});
