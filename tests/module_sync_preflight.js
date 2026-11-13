#!/usr/bin/env node
// Offline, read-only preflight for explicitly supplied local repositories.
const fs = require("node:fs");
const path = require("node:path");
const cp = require("node:child_process");

const UNKNOWN = "UNKNOWN";
const defaultNames = ["AzerothCore", "Individual Progression", "Playerbots",
                      "Mod-Naxxramas-Core", "Character Services"];
const allowedGit = new Set(["rev-parse", "symbolic-ref", "status", "rev-list", "diff"]);
function git(repo, ...args) {
    if (!allowedGit.has(args[0])) throw new Error("inspection commands only");
    try {
        return cp.execFileSync("git", ["-C", repo, ...args],
                               {encoding: "utf8", stdio: ["ignore", "pipe", "ignore"], timeout: 30000}).trimEnd();
    } catch { return null; }
}
function validRef(ref) {
    return typeof ref === "string" && /^[A-Za-z0-9_./-]+$/.test(ref) &&
           !ref.startsWith("-") && !ref.includes("..") && !ref.endsWith("/");
}
function validateManifest(manifest, dir) {
    if (!manifest || typeof manifest !== "object" || Array.isArray(manifest) ||
        !Array.isArray(manifest.modules))
        throw new TypeError("manifest must contain a modules array");
    const names = new Set(), paths = new Set();
    for (const [index, item] of manifest.modules.entries()) {
        if (!item || typeof item !== "object" || Array.isArray(item) ||
            typeof item.name !== "string" || !item.name.trim())
            throw new TypeError(`modules[${index}] needs a nonempty name`);
        if (names.has(item.name)) throw new TypeError(`duplicate module name: ${item.name}`);
        names.add(item.name);
        if (item.path != null) {
            if (typeof item.path !== "string" || !item.path.trim() || item.path.includes("\0"))
                throw new TypeError(`modules[${index}].path must be a nonempty local path`);
            const resolved = path.resolve(dir, item.path);
            if (paths.has(resolved)) throw new TypeError(`duplicate repository path in modules[${index}]`);
            paths.add(resolved);
        }
        for (const key of ["baseRef", "upstreamRef", "expectedBranch"])
            if (item[key] != null && !validRef(item[key]))
                throw new TypeError(`modules[${index}].${key} is not a valid local ref`);
        if (item.expectedHead != null &&
            (typeof item.expectedHead !== "string" || !/^[0-9a-f]{40}$/i.test(item.expectedHead)))
            throw new TypeError(`modules[${index}].expectedHead must be a 40-digit SHA`);
        for (const key of ["customPaths", "registerPaths", "pinFiles"])
            if (item[key] != null && (!Array.isArray(item[key]) ||
                item[key].some(x => typeof x !== "string" || !x || x.includes("\0"))))
                throw new TypeError(`modules[${index}].${key} must be an array of nonempty paths`);
    }
}
function localPath(base, relative) {
    return typeof relative === "string" && relative ? path.resolve(base, relative) : null;
}
function within(repo, relative) {
    if (typeof relative !== "string" || !relative) return null;
    const target = path.resolve(repo, relative);
    if (!(target === repo || target.startsWith(repo + path.sep))) return null;
    if (!fs.existsSync(target)) return target;
    const actual = fs.realpathSync(target);
    return actual === repo || actual.startsWith(repo + path.sep) ? actual : null;
}
function globMatch(value, pattern) {
    const escaped = pattern.replace(/[.+^${}()|[\]\\]/g, "\\$&").replace(/\*/g, ".*").replace(/\?/g, ".");
    return new RegExp("^" + escaped + "$").test(value);
}
function dependencies(paths) {
    if (paths === UNKNOWN) return UNKNOWN;
    const result = {SQL: [], config: [], DBC: [], client: []};
    for (const item of paths) {
        const p = item.toLowerCase();
        if (p.endsWith(".sql")) result.SQL.push(item);
        if (/\.(conf(?:\.dist)?|ini|toml)$/.test(p) || p.includes("/conf/")) result.config.push(item);
        if (/\.(dbc|db2)$/.test(p) || p.includes("/dbc/")) result.DBC.push(item);
        if (/\.(mpq|patch)$/.test(p) || p.includes("/client/")) result.client.push(item);
    }
    return result;
}
function publicChanges(repo, base, paths, target = "HEAD") {
    if (paths === UNKNOWN) return UNKNOWN;
    const headers = paths.filter(p => /\.(h|hpp|hh)$/i.test(p));
    if (!headers.length) return [];
    const diff = git(repo, "diff", "--unified=0", `${base}...${target}`, "--", ...headers);
    if (diff === null) return UNKNOWN;
    const changes = [];
    let current = "";
    for (const line of diff.split("\n")) {
        if (line.startsWith("+++ b/")) current = line.slice(6);
        if (!/^[+-]/.test(line) || /^(\+\+\+|---)/.test(line)) continue;
        let symbol = line.slice(1).trim();
        if (/^(\/\/|\/\*|\*)/.test(symbol)) continue;
        let kind = null;
        const macro = symbol.match(/^#\s*(?:define|undef)\s+([A-Za-z_]\w*)/);
        if (macro) { kind = "macro"; symbol = macro[1]; }
        else if (symbol.startsWith("#include")) kind = "include";
        else if (/^(class|struct|enum(?: class)?|using|typedef)\b/.test(symbol) ||
                 (symbol.includes("(") && symbol.includes(")") && symbol.endsWith(";"))) kind = "API";
        if (kind) changes.push({path: current, kind, side: line[0] === "+" ? "added" : "removed",
                                symbol: symbol.slice(0, 160)});
    }
    return changes.slice(0, 100);
}
function registerHits(repo, paths, files) {
    if (paths === UNKNOWN || !files.length) return UNKNOWN;
    const hits = [];
    for (const file of files) {
        const target = within(repo, file);
        if (!target || !fs.existsSync(target) || !fs.statSync(target).isFile()) return UNKNOWN;
        const text = fs.readFileSync(target, "utf8");
        for (const changed of paths) if (text.includes(changed)) hits.push({register: file, path: changed});
    }
    return hits;
}
function pins(repo, supplied) {
    const files = new Set(supplied);
    const workflows = within(repo, ".github/workflows");
    if (workflows && fs.existsSync(workflows) && fs.statSync(workflows).isDirectory())
        for (const name of fs.readdirSync(workflows))
            if (/\.ya?ml$/i.test(name)) files.add(`.github/workflows/${name}`);
    if (!files.size) return UNKNOWN;
    const result = [];
    for (const file of [...files].sort()) {
        const target = within(repo, file);
        if (!target || !fs.existsSync(target) || !fs.statSync(target).isFile()) return UNKNOWN;
        for (const sha of new Set(fs.readFileSync(target, "utf8").match(/\b[0-9a-f]{40}\b/gi) || []))
            result.push({file, sha: sha.toLowerCase()});
    }
    return result;
}
function risksAndGates(r) {
    const risks = [], gates = [];
    if (r.dirty === true) {
        risks.push("Local changes or untracked files may be absent from ref comparison.");
        gates.push("Inventory and preserve local modifications before any update.");
    }
    if (r.aheadBehind !== UNKNOWN && r.aheadBehind.ahead && r.aheadBehind.behind) {
        risks.push("Local and supplied upstream refs diverge; conflict review is needed.");
        gates.push("Compare each custom path against both sides before a proposed merge.");
    }
    if (r.customOverlap !== UNKNOWN && r.customOverlap.length) {
        risks.push("Changed paths overlap supplied custom modifications.");
        gates.push("Review intended custom behavior and regression tests for each overlap.");
    }
    if (r.likelyConflictPaths !== UNKNOWN && r.likelyConflictPaths.length) {
        risks.push("Local and supplied upstream changes touch the same paths.");
        gates.push("Review both diffs and resolve likely conflicts on a development branch.");
    }
    if ((r.publicChanges !== UNKNOWN && r.publicChanges.length) ||
        (r.upstreamPublicChanges !== UNKNOWN && r.upstreamPublicChanges.length)) {
        risks.push("Changed public headers, APIs or macros may affect the installed module tuple.");
        gates.push("Compile/link the pinned Core and dependent modules together.");
    }
    for (const [side, group] of [["Local", r.dependencies], ["Upstream", r.upstreamDependencies]])
        if (group !== UNKNOWN)
            for (const [kind, paths] of Object.entries(group)) if (paths.length) {
            risks.push(`${side} changed ${kind} dependency paths need separate review.`);
            gates.push(kind === "SQL" ? "Review migration impact and tested backup/rollback before live data work."
                                       : `Verify installed ${kind} version and compatibility offline.`);
        }
    if (r.path === UNKNOWN) risks.push("Repository path was not supplied or is unavailable.");
    if (r.contradictions.length)
        risks.push("Supplied repository identity metadata contradicts the local checkout.");
    if (r.base === UNKNOWN || r.upstream === UNKNOWN)
        risks.push("Missing local refs prevent a complete divergence/change comparison.");
    gates.push("Run relevant native tests, full Worldserver link and actual gameplay before release.");
    return {risks, testGates: [...new Set(gates)]};
}
function inspectModule(spec, manifestDir) {
    const r = {name: spec.name || UNKNOWN, path: UNKNOWN, branch: UNKNOWN, HEAD: UNKNOWN,
               base: UNKNOWN, upstream: UNKNOWN, dirty: UNKNOWN, dirtyPaths: UNKNOWN,
               untrackedPaths: UNKNOWN, aheadBehind: UNKNOWN, changedPaths: UNKNOWN,
               upstreamChangedPaths: UNKNOWN, likelyConflictPaths: UNKNOWN,
               publicChanges: UNKNOWN, upstreamPublicChanges: UNKNOWN,
               customOverlap: UNKNOWN, dependencies: UNKNOWN, upstreamDependencies: UNKNOWN,
               registerHits: UNKNOWN, pins: UNKNOWN,
               expectedHead: spec.expectedHead || UNKNOWN,
               expectedBranch: spec.expectedBranch || UNKNOWN, contradictions: []};
    const repo = localPath(manifestDir, spec.path);
    if (!repo || !fs.existsSync(repo) || git(repo, "rev-parse", "--is-inside-work-tree") !== "true")
        return {...r, ...risksAndGates(r)};
    r.path = repo;
    const top = git(repo, "rev-parse", "--show-toplevel");
    if (!top || path.resolve(top) !== repo) {
        r.contradictions.push("Supplied path is not the repository root.");
        return {...r, ...risksAndGates(r)};
    }
    r.branch = git(repo, "symbolic-ref", "--short", "-q", "HEAD") || "DETACHED";
    r.HEAD = git(repo, "rev-parse", "HEAD") || UNKNOWN;
    if (r.expectedHead !== UNKNOWN && r.HEAD !== UNKNOWN &&
        r.expectedHead.toLowerCase() !== r.HEAD.toLowerCase())
        r.contradictions.push("Expected HEAD differs from local HEAD.");
    if (r.expectedBranch !== UNKNOWN && r.branch !== UNKNOWN && r.expectedBranch !== r.branch)
        r.contradictions.push("Expected branch differs from local branch.");
    const status = git(repo, "status", "--porcelain=v1", "--untracked-files=all");
    if (status !== null) {
        const lines = status ? status.split("\n") : [];
        r.dirty = lines.length > 0;
        r.dirtyPaths = lines.filter(x => !x.startsWith("?? ")).map(x => x.slice(3));
        r.untrackedPaths = lines.filter(x => x.startsWith("?? ")).map(x => x.slice(3));
    }
    for (const [input, output] of [["baseRef", "base"], ["upstreamRef", "upstream"]])
        if (validRef(spec[input])) r[output] = git(repo, "rev-parse", "--verify", `${spec[input]}^{commit}`) || UNKNOWN;
    if (r.upstream !== UNKNOWN) {
        const counts = git(repo, "rev-list", "--left-right", "--count", `${r.upstream}...HEAD`);
        if (counts && /^\d+\s+\d+$/.test(counts)) {
            const [behind, ahead] = counts.split(/\s+/).map(Number);
            r.aheadBehind = {ahead, behind};
        }
    }
    const changed = r.base === UNKNOWN ? null : git(repo, "diff", "--name-only", `${r.base}...HEAD`);
    r.changedPaths = changed === null ? UNKNOWN : (changed ? changed.split("\n").sort() : []);
    const upstreamChanged = r.base === UNKNOWN || r.upstream === UNKNOWN ? null :
        git(repo, "diff", "--name-only", `${r.base}...${r.upstream}`);
    r.upstreamChangedPaths = upstreamChanged === null ? UNKNOWN :
        (upstreamChanged ? upstreamChanged.split("\n").sort() : []);
    r.likelyConflictPaths = r.changedPaths === UNKNOWN || r.upstreamChangedPaths === UNKNOWN ?
        UNKNOWN : r.changedPaths.filter(p => r.upstreamChangedPaths.includes(p));
    r.publicChanges = publicChanges(repo, r.base, r.changedPaths);
    r.upstreamPublicChanges = publicChanges(repo, r.base, r.upstreamChangedPaths, r.upstream);
    r.customOverlap = r.changedPaths === UNKNOWN ? UNKNOWN :
        [...new Set([...r.changedPaths, ...(r.upstreamChangedPaths === UNKNOWN ? [] : r.upstreamChangedPaths)]
            .filter(p => (spec.customPaths || []).some(pattern => globMatch(p, pattern))))].sort();
    r.dependencies = dependencies(r.changedPaths);
    r.upstreamDependencies = dependencies(r.upstreamChangedPaths);
    r.registerHits = registerHits(repo,
        r.changedPaths === UNKNOWN ? UNKNOWN :
            [...new Set([...r.changedPaths, ...(r.upstreamChangedPaths === UNKNOWN ? [] : r.upstreamChangedPaths)])],
        spec.registerPaths || []);
    r.pins = pins(repo, spec.pinFiles || []);
    return {...r, ...risksAndGates(r)};
}
function preflight(manifestFile) {
    const manifest = JSON.parse(fs.readFileSync(manifestFile, "utf8"));
    const dir = path.dirname(path.resolve(manifestFile));
    validateManifest(manifest, dir);
    const supplied = manifest.modules;
    const names = new Set(supplied.map(x => x.name));
    return {mode: "read-only local", modules: [
        ...supplied.map(x => inspectModule(x, dir)),
        ...defaultNames.filter(x => !names.has(x)).map(name => inspectModule({name}, dir))]};
}
function markdown(report) {
    const lines = ["# Module synchronization preflight", "",
                   "Read-only local evidence; UNKNOWN is not a clean bill of health.", ""];
    for (const m of report.modules) {
        lines.push(`## ${m.name}`, "", `Path: ${m.path}  Branch: ${m.branch}  HEAD: ${m.HEAD}`,
                   `Base: ${m.base}  Upstream: ${m.upstream}`,
                   `Dirty: ${m.dirty}  Ahead/behind: ${JSON.stringify(m.aheadBehind)}`,
                   `Expected HEAD: ${m.expectedHead}  Expected branch: ${m.expectedBranch}`,
                   `Contradictions: ${JSON.stringify(m.contradictions)}`);
        for (const key of ["dirtyPaths", "untrackedPaths", "changedPaths", "upstreamChangedPaths",
                           "likelyConflictPaths", "publicChanges", "upstreamPublicChanges",
                           "customOverlap", "dependencies", "upstreamDependencies",
                           "registerHits", "pins", "risks", "testGates"])
            lines.push(`- ${key}: ${JSON.stringify(m[key])}`);
        lines.push("");
    }
    return lines.join("\n");
}
if (require.main === module) {
    const args = process.argv.slice(2);
    const flag = name => args[args.indexOf(name) + 1];
    if (!args.includes("--manifest") || !flag("--manifest")) {
        process.stderr.write("Usage: node module_sync_preflight.js --manifest path [--format json|markdown]\n");
        process.exitCode = 2;
    } else if (args.includes("--format") && !["json", "markdown"].includes(flag("--format"))) {
        process.stderr.write("ERROR preflight input: --format must be json or markdown\n");
        process.exitCode = 2;
    } else {
        try {
            const report = preflight(path.resolve(flag("--manifest")));
            process.stdout.write((flag("--format") === "json" ? JSON.stringify(report, null, 2)
                                                             : markdown(report)) + "\n");
        } catch (error) {
            process.stderr.write(`ERROR preflight input: ${error.message}\n`);
            process.exitCode = 2;
        }
    }
}
module.exports = {UNKNOWN, git, inspectModule, preflight, validateManifest};
