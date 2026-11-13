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
