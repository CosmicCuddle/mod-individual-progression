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
                     ["missing stable IP feature ID", "missing owner", "missing owning component",
                      "missing intended behavior", "missing affected paths",
                      "missing parent/source reference", "missing test evidence/plan",
                      "missing dependency statement", "missing rollback statement",
                      "missing explicit non-live status"]);
    assert.ok(reconcile.rowGaps({heading: "IP-F14-NEW — DRAFT/OFF",
                                 body: "Tests, dependencies, rollback. NOW DEPLOYED ON server."})
                       .includes("possible positive live/deployed claim needs evidence"));
});

test("a stale claimed exact HEAD is distinct from a historical source reference", () => {
    const expected = "a".repeat(40), stale = "b".repeat(40);
    const row = {id: "IP-F14-NEW", heading: "IP-F14-NEW — DRAFT/OFF",
                 body: `Owner: CosmicCuddle. Owning component: IP. Intended behavior: preserve rights.\nAffected paths: src/example.h. Child of ${stale}. Tests: pending. Dependencies: Core. Rollback: abandon branch. NOT DEPLOYED.\nCurrent HEAD: ${stale}.`};
    assert.deepEqual(reconcile.rowGaps(row, expected), ["stale claimed current/exact HEAD"]);
    assert.deepEqual(reconcile.rowGaps({...row, body: row.body.replace(`Current HEAD: ${stale}.`, "")}, expected), []);
});

test("pinned local candidates add one row each without altering earlier sections", () => {
    const manifestFile = path.resolve(__dirname, "../docs/F14_REGISTER_RECONCILIATION_MANIFEST_2026-11-13.json");
    const manifest = JSON.parse(fs.readFileSync(manifestFile, "utf8"));
    const report = reconcile.inspect(manifest, path.dirname(manifestFile));
    assert.equal(report.candidates.length, 5);
    for (const c of report.candidates) {
        assert.deepEqual(c.problems, [], c.name);
        assert.equal(c.rows.length, 1, c.name);
        const expectedGaps = {
            "IP #174 pass lifecycle": ["missing owning component"],
            "IP #175 group floor": ["missing parent/source reference"]
        };
        assert.deepEqual(c.rows[0].gaps, expectedGaps[c.name] || [], c.name);
    }
    assert.deepEqual(report.conflicts, []);
    assert.equal(report.externalRows[0].id, "UNKNOWN");
});

test("exact external social Register blob is inspected when its parent and head are local", () => {
    const file = path.resolve(__dirname, "../docs/F14_REGISTER_RECONCILIATION_MANIFEST_V5_2026-11-13.json");
    const manifest = JSON.parse(fs.readFileSync(file, "utf8"));
    const report = reconcile.inspect(manifest, path.dirname(file));
    const social = report.externalRows[0];
    if (social.verification === "UNKNOWN; exact external Git objects unavailable") {
        assert.deepEqual(social.problems, ["external head, parent ancestry or Register blob unavailable"]);
        return; // A fresh partial clone need not contain Claude's pinned blob.
    }
    assert.equal(social.verification, "local exact external Register blob; moving branch not checked");
    assert.deepEqual(social.removed, []);
    assert.deepEqual(social.modified, []);
    assert.deepEqual(social.problems, []);
    assert.deepEqual(social.gaps, ["missing stable IP feature ID", "missing affected paths"]);
    const unavailable = structuredClone(manifest);
    unavailable.externalRows[0].head = "f".repeat(40);
    assert.equal(reconcile.inspect(unavailable, path.dirname(file)).externalRows[0].verification,
                 "UNKNOWN; exact external Git objects unavailable");
});
