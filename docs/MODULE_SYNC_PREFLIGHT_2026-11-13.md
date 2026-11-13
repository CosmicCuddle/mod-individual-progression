# Read-only module synchronization preflight

Owner: CosmicCuddle. Codex coding support under the [Coordination #51 continuous queue](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6851977983). This tool is a source review aid, not an updater or installed-server inventory.

`tests/module_sync_preflight.js` reads only repositories and local refs **explicitly supplied** in a JSON manifest. The example names AzerothCore, Individual Progression, Playerbots, Mod-Naxxramas-Core and Character Services. Other supporting modules appear only if added to `modules`. Relative paths are resolved from the manifest's directory. Absent checkout/ref/metadata is printed as `UNKNOWN`, never inferred clean or current.

```sh
node tests/module_sync_preflight.js --manifest docs/MODULE_SYNC_PREFLIGHT_EXAMPLE.json --format markdown
node tests/module_sync_preflight.js --manifest docs/MODULE_SYNC_PREFLIGHT_EXAMPLE.json --format json
node --test tests/test_module_sync_preflight.js
```

For each supplied checkout, the report gives branch, HEAD, resolved base/upstream local refs, tracked dirty and untracked paths, local ahead/behind counts where the upstream ref is available, changed paths from base, upstream-changed paths, likely same-path conflicts, local and upstream public header API/macro/include lines, supplied custom-path overlap, SQL/config/DBC/client-patch paths, Custom Modifications Register path hits, and 40-character source/workflow pins from supplied pin files and local workflows. Missing supplied Register/pin files remain `UNKNOWN`. Risks and test gates derive from those observations. Path overlap is a **review signal**, not a claim of an actual merge conflict or runtime incompatibility. Header-diff recognition is intentionally conservative and can miss semantic changes or a macro defined elsewhere. Register hits are literal path matches. Values in dirty working files are not diffed; they are only listed as dirty.

Optional `expectedHead` (full 40-digit SHA) and `expectedBranch` fields compare supplied identity claims with the local checkout. Mismatches appear in `contradictions` and risk text; the tool never substitutes the expected value for observed HEAD/branch. A supplied path inside a repository but not at its root is also contradictory and leaves change/diff fields `UNKNOWN`. Malformed JSON/schema, duplicate component names or paths, invalid refs, wrong-shaped path lists, and invalid expected SHAs fail visibly with exit code 2 before inspection. Valid but unavailable local refs stay `UNKNOWN`. This guards against treating bad inputs as an apparently clean report.

The tool allows only Git `rev-parse`, `symbolic-ref`, `status`, `rev-list` and `diff` inspection commands. It never fetches, pulls, merges, rebases, checks out, resets, updates submodules, installs packages or opens a database. The fixture tests create disposable local Git repositories offline and cover dirty/untracked trees, divergent refs, changed macro/API, SQL/config/DBC/client dependency paths, custom/register overlaps, likely conflict paths, pins, missing refs, malformed and contradictory metadata, and rejected mutation verbs. No production environment, host config or live data is touched.

The example manifest intentionally leaves unsupplied component paths and upstream refs null. Before any proposed update, supply verified local paths/refs and check the report against installed versions, actual custom behavior, pinned source-contract SHA tuples, SQL/config/DBC/client dependencies, backup/rollback plans, native compilation/link and gameplay. A read-only report never authorizes merge or deployment. Rollback: remove this isolated tool and its documentation; no server state changes.
