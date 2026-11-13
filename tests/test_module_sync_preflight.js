const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const cp = require("node:child_process");
const preflight = require("./module_sync_preflight.js");

function command(repo, ...args) {
    return cp.execFileSync("git", ["-C", repo, ...args],
                           {encoding: "utf8", stdio: ["ignore", "pipe", "ignore"]}).trim();
}
function put(repo, name, content) {
    const target = path.join(repo, name);
    fs.mkdirSync(path.dirname(target), {recursive: true});
    fs.writeFileSync(target, content);
}
function fixture(fn) {
    const root = fs.mkdtempSync(path.join(os.tmpdir(), "f14-preflight-"));
    try { fn(root); }
    finally {
        const target = path.resolve(root);
        const parent = path.resolve(os.tmpdir());
        assert.ok(target.startsWith(parent + path.sep), "fixture cleanup escaped temp directory");
        fs.rmSync(target, {recursive: true, force: true});
    }
}
function init(repo) {
    fs.mkdirSync(repo);
    command(repo, "init", "-b", "main");
    command(repo, "config", "user.name", "Fixture");
    command(repo, "config", "user.email", "fixture@example.invalid");
}
function commit(repo, message) {
    command(repo, "add", ".");
    command(repo, "commit", "-m", message);
}

test("dirty divergent refs, API/macro, SQL/config/DBC/client, custom overlap and pins", () => fixture(root => {
    const repo = path.join(root, "ip");
    init(repo);
    put(repo, "src/PublicAPI.h", "#define F14_API_VERSION 1\nint OldApi();\n");
    put(repo, "docs/CUSTOM_MODIFICATIONS_REGISTER.md", "src/PublicAPI.h custom owner\n");
    const pinned = "a".repeat(40);
    put(repo, ".github/workflows/pinned.yml", `# pinned Core ${pinned}\n`);
    commit(repo, "baseline");
    command(repo, "tag", "base");
    command(repo, "switch", "-c", "upstream");
    put(repo, "src/Upstream.h", "#define UPSTREAM_API 2\n");
    put(repo, "src/PublicAPI.h", "#define F14_API_VERSION 9\nint UpstreamApi();\n");
    put(repo, "data/sql/world/upstream.sql", "-- upstream fixture only\n");
    commit(repo, "upstream change");
    command(repo, "switch", "main");
    put(repo, "src/PublicAPI.h", "#define F14_API_VERSION 2\nint NewApi();\n");
    put(repo, "data/sql/world/change.sql", "-- offline fixture only\n");
    put(repo, "conf/module.conf.dist", "Enabled = 0\n");
    put(repo, "data/dbc/Test.dbc", "fixture\n");
    put(repo, "client/patch.mpq", "fixture\n");
    commit(repo, "local API and dependencies");
    put(repo, "src/PublicAPI.h", "#define F14_API_VERSION 3\nint NewApi();\n");
    put(repo, "untracked.txt", "untracked\n");
    const spec = {name: "Individual Progression", path: "ip", baseRef: "base",
                  upstreamRef: "upstream", customPaths: ["src/PublicAPI.h"],
                  registerPaths: ["docs/CUSTOM_MODIFICATIONS_REGISTER.md"],
                  pinFiles: [".github/workflows/pinned.yml"]};
    const manifest = path.join(root, "manifest.json");
    fs.writeFileSync(manifest, JSON.stringify({modules: [spec]}));
    const cli = cp.execFileSync(process.execPath,
        [path.join(__dirname, "module_sync_preflight.js"), "--manifest", manifest, "--format", "json"],
        {encoding: "utf8"});
    const report = JSON.parse(cli);
    const item = report.modules[0];
    assert.equal(item.branch, "main");
    assert.equal(item.dirty, true);
    assert.ok(item.dirtyPaths.includes("src/PublicAPI.h"));
    assert.ok(item.untrackedPaths.includes("untracked.txt"));
    assert.deepEqual(item.aheadBehind, {ahead: 1, behind: 1});
    assert.ok(item.customOverlap.includes("src/PublicAPI.h"));
    assert.ok(item.upstreamChangedPaths.includes("src/PublicAPI.h"));
    assert.ok(item.likelyConflictPaths.includes("src/PublicAPI.h"));
    assert.ok(item.changedPaths.includes("src/PublicAPI.h"));
    assert.ok(item.publicChanges.some(x => x.kind === "macro"));
    assert.ok(item.publicChanges.some(x => x.kind === "API"));
    assert.ok(item.upstreamPublicChanges.some(x => x.kind === "macro"));
    assert.ok(item.upstreamPublicChanges.some(x => x.path === "src/Upstream.h"));
    assert.ok(item.dependencies.SQL.includes("data/sql/world/change.sql"));
    assert.ok(item.upstreamDependencies.SQL.includes("data/sql/world/upstream.sql"));
    assert.ok(item.dependencies.config.includes("conf/module.conf.dist"));
    assert.ok(item.dependencies.DBC.includes("data/dbc/Test.dbc"));
    assert.ok(item.dependencies.client.includes("client/patch.mpq"));
    assert.ok(item.registerHits.some(x => x.path === "src/PublicAPI.h"));
    assert.ok(item.pins.some(x => x.sha === pinned));
    assert.ok(item.testGates.some(x => x.includes("Compile/link")));
    assert.equal(report.modules[1].path, preflight.UNKNOWN);
    assert.equal(report.modules[1].changedPaths, preflight.UNKNOWN);
}));

test("unresolved refs are UNKNOWN and mutating Git verbs are rejected", () => fixture(root => {
    const repo = path.join(root, "core");
    init(repo);
    put(repo, "README.md", "baseline\n");
    commit(repo, "baseline");
    const report = preflight.inspectModule({name: "AzerothCore", path: "core",
                                            baseRef: "missing", upstreamRef: "missing",
                                            registerPaths: ["docs/missing.md"],
                                            pinFiles: [".github/workflows/missing.yml"]}, root);
    assert.equal(report.base, preflight.UNKNOWN);
    assert.equal(report.upstream, preflight.UNKNOWN);
    assert.equal(report.changedPaths, preflight.UNKNOWN);
    assert.equal(report.aheadBehind, preflight.UNKNOWN);
    assert.equal(report.pins, preflight.UNKNOWN);
    const missingFiles = preflight.inspectModule({name: "AzerothCore", path: "core",
        baseRef: "HEAD", registerPaths: ["docs/missing.md"],
        pinFiles: [".github/workflows/missing.yml"]}, root);
    assert.deepEqual(missingFiles.changedPaths, []);
    assert.equal(missingFiles.registerHits, preflight.UNKNOWN);
    assert.equal(missingFiles.pins, preflight.UNKNOWN);
    put(repo, "docs/present.md", "src/Present.h\n");
    put(repo, ".github/workflows/present.yml", `# ${"b".repeat(40)}\n`);
    const partial = preflight.inspectModule({name: "AzerothCore", path: "core",
        baseRef: "HEAD", registerPaths: ["docs/present.md", "docs/missing.md"],
        pinFiles: [".github/workflows/present.yml", ".github/workflows/missing.yml"]}, root);
    assert.equal(partial.registerHits, preflight.UNKNOWN);
    assert.equal(partial.pins, preflight.UNKNOWN);
    assert.throws(() => preflight.git(repo, "fetch", "origin"), /inspection commands only/);
}));

test("malformed manifest fails visibly before repository inspection", () => fixture(root => {
    const manifest = path.join(root, "bad.json");
    const cli = () => cp.spawnSync(process.execPath,
        [path.join(__dirname, "module_sync_preflight.js"), "--manifest", manifest, "--format", "json"],
        {encoding: "utf8"});
    for (const data of [
        {}, {modules: {}}, {modules: [null]},
        {modules: [{name: "AzerothCore", baseRef: "--upload-pack=bad"}]},
        {modules: [{name: "AzerothCore", customPaths: "src/*"}]},
        {modules: [{name: "AzerothCore"}, {name: "AzerothCore"}]},
        {modules: [{name: "AzerothCore", path: "same"},
                   {name: "Playerbots", path: "same"}]},
        {modules: [{name: "AzerothCore", expectedHead: "short"}]}
    ]) {
        fs.writeFileSync(manifest, JSON.stringify(data));
        const result = cli();
        assert.equal(result.status, 2);
        assert.match(result.stderr, /ERROR preflight input:/);
        assert.equal(result.stdout, "");
    }
}));

test("contradictory identity and subdirectory paths never imply a verified checkout", () => fixture(root => {
    const repo = path.join(root, "core");
    init(repo);
    put(repo, "src/API.h", "int Api();\n");
    commit(repo, "baseline");
    const wrong = preflight.inspectModule({name: "AzerothCore", path: "core",
        expectedHead: "a".repeat(40), expectedBranch: "wrong"}, root);
    assert.equal(wrong.contradictions.length, 2);
    assert.ok(wrong.risks.some(x => x.includes("contradicts")));
    const nested = preflight.inspectModule({name: "AzerothCore", path: "core/src",
        baseRef: "HEAD"}, root);
    assert.deepEqual(nested.contradictions, ["Supplied path is not the repository root."]);
    assert.equal(nested.changedPaths, preflight.UNKNOWN);
    assert.equal(nested.HEAD, preflight.UNKNOWN);
}));
