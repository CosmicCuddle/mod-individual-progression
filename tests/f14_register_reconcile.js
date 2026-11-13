#!/usr/bin/env node
// Read-only, local Git comparison of candidate Register rows. No merge output.
const fs = require("node:fs");
const path = require("node:path");
const cp = require("node:child_process");

const UNKNOWN = "UNKNOWN";
function git(repo, ...args) {
    if (!["show", "rev-parse", "merge-base"].includes(args[0]))
        throw new Error("read-only Git inspection only");
    try {
        return cp.execFileSync("git", ["-c", "core.fsmonitor=false", "-C", repo, ...args],
            {encoding: "utf8", stdio: ["ignore", "pipe", "ignore"], timeout: 30000,
             env: {...process.env, GIT_OPTIONAL_LOCKS: "0", GIT_NO_LAZY_FETCH: "1",
                   GIT_TERMINAL_PROMPT: "0"}}).trimEnd();
    } catch { return null; }
}
function sections(source) {
    const result = [], lines = source.split(/\r?\n/);
    let current = null;
    for (const line of lines) {
        const heading = line.match(/^## (.+)$/);
        if (heading) {
            if (current) result.push(current);
            const id = heading[1].match(/^(IP-[A-Z0-9-]+)\b/);
            current = {heading: heading[1], id: id ? id[1] : null, body: ""};
        } else if (current) current.body += line + "\n";
    }
    if (current) result.push(current);
    return result;
}
function sectionKey(s) { return s.id || `heading:${s.heading}`; }
function sectionDiff(parentText, childText) {
    const parent = sections(parentText), child = sections(childText);
    const before = new Map(parent.map(s => [sectionKey(s), s]));
    const after = new Map(child.map(s => [sectionKey(s), s]));
    return {
        added: child.filter(s => !before.has(sectionKey(s))),
        removed: parent.filter(s => !after.has(sectionKey(s))),
        modified: child.filter(s => before.has(sectionKey(s)) &&
            (before.get(sectionKey(s)).heading !== s.heading ||
             before.get(sectionKey(s)).body.trim() !== s.body.trim())),
        duplicateIds: duplicateIds(child)
    };
}
function duplicateIds(items) {
    const seen = new Set(), duplicates = [];
    for (const s of items) if (s.id) {
        if (seen.has(s.id)) duplicates.push(s.id);
        seen.add(s.id);
    }
    return duplicates;
}
function rowGaps(row) {
    const text = `${row.heading}\n${row.body}`;
    const gaps = [];
    if (!row.id) gaps.push("missing stable IP feature ID");
    if (!/\btest(?:s|ed|ing)?\b/i.test(text)) gaps.push("missing test evidence/plan");
    if (!/\b(?:dependenc\w*|config|SQL|DBC|client)\b/i.test(text))
        gaps.push("missing dependency statement");
    if (!/\brollback\b/i.test(text)) gaps.push("missing rollback statement");
    if (!/\b(?:NOT DEPLOYED|NOT MERGED|DRAFT|TEST ONLY|OFF)\b/i.test(text))
        gaps.push("missing explicit non-live status");
    if (/\b(?:IS LIVE|NOW DEPLOYED|DEPLOYED ON)\b/i.test(text))
        gaps.push("possible positive live/deployed claim needs evidence");
    return gaps;
}
function validRef(ref) {
    return typeof ref === "string" && /^[A-Za-z0-9_./-]+$/.test(ref) &&
           !ref.startsWith("-") && !ref.includes("..") && !ref.endsWith("/");
}
function validateManifest(manifest) {
    if (!manifest || typeof manifest !== "object" || Array.isArray(manifest) ||
        typeof manifest.repository !== "string" || typeof manifest.registerPath !== "string" ||
        !Array.isArray(manifest.candidates) || !Array.isArray(manifest.externalRows))
        throw new TypeError("manifest needs repository, registerPath, candidates and externalRows");
    for (const c of manifest.candidates)
        if (!c || typeof c.name !== "string" || !validRef(c.ref) ||
            typeof c.expectedHead !== "string" || !/^[0-9a-f]{40}$/i.test(c.expectedHead) ||
            typeof c.parent !== "string" || !/^[0-9a-f]{40}$/i.test(c.parent))
            throw new TypeError("candidate needs name, local ref, exact expectedHead and parent SHA");
    for (const e of manifest.externalRows)
        if (!e || typeof e.name !== "string" || typeof e.heading !== "string" ||
            typeof e.source !== "string")
            throw new TypeError("external row needs name, heading and source link");
}
function inspect(manifest, manifestDir) {
    validateManifest(manifest);
    const repo = path.resolve(manifestDir, manifest.repository);
    const top = git(repo, "rev-parse", "--show-toplevel");
    if (!top || path.resolve(top).toLowerCase() !== repo.toLowerCase())
        throw new Error("supplied path is not a local repository root");
    if (path.isAbsolute(manifest.registerPath) || manifest.registerPath.split(/[\\/]/).includes(".."))
        throw new Error("registerPath must stay inside repository");
    const reports = [], allAdded = [], conflicts = [];
    for (const c of manifest.candidates) {
        const head = git(repo, "rev-parse", "--verify", `${c.ref}^{commit}`);
        const parent = git(repo, "rev-parse", "--verify", `${c.parent}^{commit}`);
        const mergeBase = head && parent ? git(repo, "merge-base", parent, head) : null;
        const problems = [];
        if (!head || head.toLowerCase() !== c.expectedHead.toLowerCase())
            problems.push("local ref missing or moved from expected HEAD");
        if (!parent || mergeBase !== parent)
            problems.push("expected parent is absent or not the merge base");
        const before = parent ? git(repo, "show", `${parent}:${manifest.registerPath}`) : null;
        const after = head ? git(repo, "show", `${head}:${manifest.registerPath}`) : null;
        if (before === null || after === null) {
            reports.push({name: c.name, parent: parent || UNKNOWN, HEAD: head || UNKNOWN,
                          rows: UNKNOWN, problems: [...problems, "Register blob unavailable"]});
            continue;
        }
        const delta = sectionDiff(before, after);
        if (delta.removed.length) problems.push("candidate removes Register sections");
        if (delta.modified.length) problems.push("candidate modifies existing Register sections");
        if (delta.duplicateIds.length) problems.push("duplicate feature IDs inside candidate Register");
        const rows = delta.added.map(row => ({id: row.id || UNKNOWN, heading: row.heading,
                                               gaps: rowGaps(row)}));
        for (const row of rows) allAdded.push({candidate: c.name, ...row});
        reports.push({name: c.name, parent, HEAD: head, rows,
                      removed: delta.removed.map(sectionKey),
                      modified: delta.modified.map(sectionKey), problems});
    }
    for (const e of manifest.externalRows) {
        const id = e.heading.match(/^(IP-[A-Z0-9-]+)\b/)?.[1] || UNKNOWN;
        allAdded.push({candidate: e.name, id, heading: e.heading,
                       gaps: id === UNKNOWN ? ["missing stable IP feature ID"] : [],
                       source: e.source, verification: "external heading supplied; blob not locally inspected"});
    }
    const owners = new Map();
    for (const row of allAdded) if (row.id !== UNKNOWN) {
        if (owners.has(row.id)) conflicts.push({id: row.id, candidates: [owners.get(row.id), row.candidate]});
        else owners.set(row.id, row.candidate);
    }
    return {mode: "read-only local Register delta", candidates: reports,
            externalRows: allAdded.filter(x => x.verification), conflicts};
}
if (require.main === module) {
    if (process.argv.length !== 4 || process.argv[2] !== "--manifest") {
        process.stderr.write("Usage: node f14_register_reconcile.js --manifest path\n");
        process.exitCode = 2;
    } else try {
        const file = path.resolve(process.argv[3]);
        process.stdout.write(JSON.stringify(inspect(JSON.parse(fs.readFileSync(file, "utf8")),
                                              path.dirname(file)), null, 2) + "\n");
    } catch (error) {
        process.stderr.write(`ERROR Register reconciliation: ${error.message}\n`);
        process.exitCode = 2;
    }
}
module.exports = {sections, sectionDiff, duplicateIds, rowGaps, inspect};
