const test = require("node:test");
const assert = require("node:assert/strict");
const path = require("node:path");
const fs = require("node:fs");
const reconcile = require("./f14_register_reconcile.js");

const oldRow = `## IP-F14-OLD — old feature (DRAFT/OFF)\nTests and dependencies. Rollback: retain base. NOT DEPLOYED.\n`;
const newRow = `## IP-F14-NEW — new feature (DRAFT/OFF)\nTests and dependencies. Rollback: abandon branch. NOT DEPLOYED.\n`;

test("additive Register row preserves the prior row despite separator whitespace", () => {
    const delta = reconcile.sectionDiff(oldRow, oldRow + "\n" + newRow);
    assert.deepEqual(delta.added.map(x => x.id), ["IP-F14-NEW"]);
    assert.deepEqual(delta.modified, []);
    assert.deepEqual(delta.removed, []);
});

test("splicing a new heading before an old paragraph is reported as modification", () => {
    const before = oldRow + "Focused original tests remain required.\n";
    const after = oldRow + newRow + "Focused original tests remain required.\n";
    const delta = reconcile.sectionDiff(before, after);
    assert.deepEqual(delta.modified.map(x => x.id), ["IP-F14-OLD"]);
    assert.deepEqual(delta.added.map(x => x.id), ["IP-F14-NEW"]);
});

test("duplicate IDs and missing status, test, dependency, rollback fields are visible", () => {
    const repeated = reconcile.sections(oldRow + newRow + newRow);
    assert.deepEqual(reconcile.duplicateIds(repeated), ["IP-F14-NEW"]);
    assert.deepEqual(reconcile.rowGaps({heading: "13 November — unnamed feature", body: "Nothing here."}),
                     ["missing stable IP feature ID", "missing test evidence/plan",
                      "missing dependency statement", "missing rollback statement",
                      "missing explicit non-live status"]);
});

test("pinned local candidates add one row each without altering earlier sections", () => {
    const manifestFile = path.resolve(__dirname, "../docs/F14_REGISTER_RECONCILIATION_MANIFEST_2026-11-13.json");
    const manifest = JSON.parse(fs.readFileSync(manifestFile, "utf8"));
    const report = reconcile.inspect(manifest, path.dirname(manifestFile));
    assert.equal(report.candidates.length, 5);
    for (const c of report.candidates) {
        assert.deepEqual(c.problems, [], c.name);
        assert.equal(c.rows.length, 1, c.name);
        assert.deepEqual(c.rows[0].gaps, [], c.name);
    }
    assert.deepEqual(report.conflicts, []);
    assert.equal(report.externalRows[0].id, reconcile.UNKNOWN || "UNKNOWN");
});
