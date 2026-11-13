#!/usr/bin/env node
// Offline path/pin preflight. Never applies a patch or mutates Git state.
const fs = require("node:fs");
const path = require("node:path");
const cp = require("node:child_process");
const register = require("./f14_register_reconcile.js");

const registerPath = "docs/CUSTOM_MODIFICATIONS_REGISTER.md";
function git(repo, ...args) {
    if (!["diff", "show", "rev-parse"].includes(args[0]))
        throw new Error("read-only Git inspection only");
    try {
        return cp.execFileSync("git", ["-c", "core.fsmonitor=false", "-C", repo, ...args],
            {encoding: "utf8", stdio: ["ignore", "pipe", "ignore"], timeout: 30000,
             env: {...process.env, GIT_OPTIONAL_LOCKS: "0", GIT_NO_LAZY_FETCH: "1",
                   GIT_TERMINAL_PROMPT: "0"}}).trimEnd();
    } catch { return null; }
}
function configDefaults(diff) {
    if (diff === null) return "UNKNOWN";
    const entries = [];
    for (const line of diff.split("\n")) {
        if (!/^[+-][^+-]/.test(line)) continue;
        const match = line.slice(1).match(/^\s*([A-Za-z][A-Za-z0-9_.-]*)\s*=\s*(.+?)\s*$/);
        if (match) entries.push({side: line[0] === "+" ? "added" : "removed",
                                  key: match[1], value: match[2]});
    }
    return entries;
}
function overlaps(candidates) {
    const pairs = [];
    for (let i = 0; i < candidates.length; ++i)
        for (let j = i + 1; j < candidates.length; ++j) {
            const a = candidates[i], b = candidates[j];
            if (!Array.isArray(a.paths) || !Array.isArray(b.paths)) continue;
            const common = a.paths.filter(p => b.paths.includes(p));
            pairs.push({a: a.name, b: b.name, common,
                        registerOverlap: common.includes(registerPath),
                        runtimeOverlap: common.filter(p => p.startsWith("src/") || p.startsWith("conf/"))});
        }
    return pairs;
}
function objectOverlaps(candidates, objectId) {
    const claims = new Map();
    for (const c of candidates) {
        if (!Array.isArray(c.paths) || !/^[0-9a-f]{40}$/i.test(c.HEAD) ||
            (c.verification && c.verification !== "LOCAL EXACT GIT OBJECTS; moving branch not checked"))
            continue;
        for (const p of c.paths) {
            if (!claims.has(p)) claims.set(p, []);
            claims.get(p).push({candidate: c.name, object: objectId(c.HEAD, p) || "UNKNOWN"});
        }
    }
    return [...claims].filter(([, rows]) => rows.length > 1).map(([path, rows]) => {
        const ids = rows.map(x => x.object);
        return {path, claims: rows, verdict: ids.includes("UNKNOWN") ? "UNKNOWN" :
            ids.every(x => x === ids[0]) ? "IDENTICAL_BLOB" : "DIVERGENT_BLOBS"};
    });
}
function dependencyEdges(candidates, base) {
    const byHead = new Map(candidates.map(c => [c.HEAD, c.name]));
    return candidates.filter(c => c.parent !== "UNKNOWN").map(c => ({
        candidate: c.name, parent: c.parent,
        parentCandidate: c.parent === base ? "COMMON_BASE" : byHead.get(c.parent) || "UNKNOWN"
    }));
}
function inspect(manifest, manifestDir) {
    const registerReport = register.inspect(manifest, manifestDir);
    const repo = path.resolve(manifestDir, manifest.repository);
    const candidates = [];
    for (const spec of manifest.candidates) {
        const report = registerReport.candidates.find(x => x.name === spec.name);
        const pathsText = report?.problems.length ? null :
            git(repo, "diff", "--name-only", spec.parent, spec.expectedHead, "--");
        const paths = pathsText === null ? "UNKNOWN" : pathsText ? pathsText.split("\n").sort() : [];
        const config = paths === "UNKNOWN" ? "UNKNOWN" :
            configDefaults(git(repo, "diff", "--unified=0", spec.parent, spec.expectedHead,
                               "--", "conf/individualProgression.conf.dist"));
        const contractPins = [];
        if (Array.isArray(paths)) for (const p of paths)
            if (/^tests\/.*source\.(?:js|py)$/.test(p)) {
                const source = git(repo, "show", `${spec.expectedHead}:${p}`);
                if (source === null) { contractPins.push({path: p, pins: "UNKNOWN"}); continue; }
                contractPins.push({path: p, pins: [...new Set(source.match(/\b[0-9a-f]{40}\b/gi) || [])]});
            }
        candidates.push({name: spec.name, parent: spec.parent, HEAD: report?.HEAD || "UNKNOWN",
                         paths, configDefaults: config, sourceContractPins: contractPins,
                         requiresParentTree: spec.parent !== manifest.candidates[0].parent,
                         registerProblems: report?.problems || ["UNKNOWN"]});
    }
    for (const e of manifest.externalRows) {
        if (!e.parent) {
            candidates.push({name: e.name, parent: "UNKNOWN", HEAD: e.head || "UNKNOWN",
                             paths: e.paths || "UNKNOWN", configDefaults: e.configDefaults || "UNKNOWN",
                             sourceContractPins: "UNKNOWN", requiresParentTree: "UNKNOWN",
                             verification: "EXTERNAL SUPPLIED METADATA ONLY; recheck branch before use"});
            continue;
        }
        const row = registerReport.externalRows.find(x => x.candidate === e.name);
        const verified = row?.verification === "local exact external Register blob; moving branch not checked";
        const names = verified ? git(repo, "diff", "--name-only", e.parent, e.head, "--") : null;
        const paths = names === null ? "UNKNOWN" : names ? names.split("\n").sort() : [];
        const config = paths === "UNKNOWN" ? "UNKNOWN" :
            configDefaults(git(repo, "diff", "--unified=0", e.parent, e.head,
                               "--", "conf/individualProgression.conf.dist"));
        const defaults = Array.isArray(config) ? Object.fromEntries(config.filter(x => x.side === "added")
            .map(x => [x.key, x.value])) : "UNKNOWN";
        const problems = [...(row?.problems || [])];
        if (Array.isArray(paths) && Array.isArray(e.paths) &&
            JSON.stringify(paths) !== JSON.stringify([...e.paths].sort()))
            problems.push("supplied external path list differs from exact local Git diff");
        if (defaults !== "UNKNOWN" && e.configDefaults &&
            JSON.stringify(Object.entries(defaults).sort()) !==
            JSON.stringify(Object.entries(e.configDefaults).sort()))
            problems.push("supplied external config defaults differ from exact local Git diff");
        candidates.push({name: e.name, parent: e.parent, HEAD: verified ? e.head : "UNKNOWN",
                         paths, configDefaults: defaults, sourceContractPins: "UNKNOWN",
                         requiresParentTree: e.parent !== manifest.candidates[0].parent,
                         verification: verified ? "LOCAL EXACT GIT OBJECTS; moving branch not checked" : "UNKNOWN",
                         problems});
    }
    return {mode: "offline read-only composition preflight", candidates,
            overlaps: overlaps(candidates), duplicateFeatureIds: registerReport.conflicts,
            objectOverlaps: objectOverlaps(candidates, (head, p) =>
                git(repo, "rev-parse", "--verify", `${head}:${p}`)),
            dependencyEdges: dependencyEdges(candidates, manifest.candidates[0].parent),
            externalRegisterRows: registerReport.externalRows};
}
if (require.main === module) {
    if (process.argv.length !== 4 || process.argv[2] !== "--manifest") {
        process.stderr.write("Usage: node f14_composition_preflight.js --manifest path\n");
        process.exitCode = 2;
    } else try {
        const file = path.resolve(process.argv[3]);
        const manifest = JSON.parse(fs.readFileSync(file, "utf8"));
        process.stdout.write(JSON.stringify(inspect(manifest, path.dirname(file)), null, 2) + "\n");
    } catch (error) {
        process.stderr.write(`ERROR composition preflight: ${error.message}\n`);
        process.exitCode = 2;
    }
}
module.exports = {configDefaults, overlaps, objectOverlaps, dependencyEdges, inspect};
