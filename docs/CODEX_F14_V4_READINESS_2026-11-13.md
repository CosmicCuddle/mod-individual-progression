# F14 v4 source readiness — pinned evidence

Owner CosmicCuddle; [coordination #51 v4 queue](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6858091821). This note is a moving source audit. It does not assert an installed binary, merge, runtime activation or gameplay result. F14/Paid stay OFF. The Altbot/excluded-account temporary group-floor population is unresolved in [Information #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10#issuecomment-6858551237); no runtime population is selected here.

## 1. Current candidate graph (GitHub heads checked 13 November)

| Candidate | Parent → current HEAD | Changed-path class |
| --- | --- | --- |
| IP [#174](https://github.com/CosmicCuddle/mod-individual-progression/pull/174) | `aed05a92f262362608f5d643c8570b504189aae0` → `3bf97b4d6edc746cb0ff7e7e02a55d6ab9fc5b32` | Register, pure pass header, note, C++/Node tests |
| IP [#175](https://github.com/CosmicCuddle/mod-individual-progression/pull/175) | `aed05a92` → `005d8949334b11c7c985225d6aa9a1cc120157a1` | Register, pure floor header, note, C++/Node tests |
| IP [#176](https://github.com/CosmicCuddle/mod-individual-progression/pull/176) | #175 `005d8949` → `971ad2ce160e29a8c426d2650e1d665c56e2ee01` | Register, pure consumer header, note, C++/Node tests |
| IP [#177](https://github.com/CosmicCuddle/mod-individual-progression/pull/177) | #174 `3bf97b4d` → `49af2465898e318481e9ad9de17699aea6f9338d` | Register, test-only witness/note/Node contract |
| IP [#178](https://github.com/CosmicCuddle/mod-individual-progression/pull/178) | `aed05a92` → `e75e2fe7033ccee35fd476fb008f6d231d4f6d5e` | Register, offline preflight/test/docs/example |
| Claude social S1 | `aed05a92` → `021be3afb4699b5058be77fba60e0e42d2818e1d` | Config, IP runtime, new social policy/script, Register, docs/tests/workflow; branch is moving |
| F15 [#310](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/310) | `827f5618c991611c483941cabb0ba910fee09180` → `044fde67fe5b56acfae050b9188fc50e5cac14b1` | Separate Mod-Naxxramas-Core offline staging preflight/docs/example/tests |
| F15 [#311](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/311) | #310 `044fde67` → `9495c5ec41e481b4088d0b687e593eb492a6426f` | Separate F15 Register, staging backup/rollback template/test |

The five Codex IP candidate **deltas** and Claude social all touch `docs/CUSTOM_MODIFICATIONS_REGISTER.md`, and no other exact path overlaps. #176 inherits #175's floor header; #177 inherits #174's pass header. Claude social changes `src/IndividualProgression.cpp`, `.h`, `src/IndividualProgressionPlayer.cpp` and `conf/individualProgression.conf.dist`; a future runtime group adapter could overlap those paths, but the current pure #175/#176 do not. F15 is a separate repository and shares no IP path. This is a path matrix, not semantic compatibility proof.

| Pair class | Exact changed-path intersection | Semantic relationship |
| --- | --- | --- |
| Any two of #174–#178 | Register only | #176 is a child of #175; #177 is a child of #174; sibling Register rows need reconciliation |
| Claude social vs each #174–#178 | Register only | Future runtime IP/group source overlap is a design dependency; Claude branch must not be edited |
| F15 #310/#311 vs IP candidates | None (different repositories) | #311 is a child of #310; evidence manifests and source pins must distinguish repos |

Stale v3 references corrected: Claude advanced from `d8a43a77` through `7b343a22` and S1 `cb23a099` to current `021be3af`; #176/#177/#178 advanced to the heads above. Re-read [Claude #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13) and the branch ref immediately before any actual rehearsal, because S1/T1 is active. The older `d8a43a77` remains a historical ref, not the present social input.

## 2. #177 exact-head standalone native recipe

Run only from an exact checkout of `49af2465898e318481e9ad9de17699aea6f9338d` (or a verified file-identical source snapshot). This is a **recipe, not a claimed run in this workspace**; no GCC/Clang C++ compiler is available locally. It needs no server, database, network package install or hosted Actions.

```sh
test "$(git rev-parse HEAD)" = 49af2465898e318481e9ad9de17699aea6f9338d
node tests/test_f14_negative_integration_source.js
for compiler in g++ clang++; do
  for standard in c++17 c++20; do
    for mode in normal ndebug; do
      flags=""
      if test "$mode" = ndebug; then flags=-DNDEBUG; fi
      "$compiler" -std="$standard" -Wall -Wextra -Werror -pedantic $flags -Isrc \
        tests/f14_negative_integration_witness.cpp -o /tmp/f14-negative-witness
      /tmp/f14-negative-witness
    done
  done
done
```

Expected successful output: Node prints `PASS F14 negative integration source: exact ephemeral evidence, raw-GUID debt, no completion`; each of eight binaries prints `PASS F14 negative composition: pass/correlation loss retains restore debt`. Any compiler/test nonzero exit is a failure. [Claude's independent prior matrix](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6857731743) passed 8/8 at `b75cabf7`, not at this head. Between `b75cabf7` and `49af2465`, only the witness C++ test, Node source contract and its documentation changed (72 insertions, one deletion); the inherited pass header blob remains `e95477c4378e3364d580b53108abfc3beb9a8e2d` at both refs. The new tests cover send/handoff/debt routes and G2's nominated send before `ObserveBatch`. Structural similarity does **not** establish exact-head native PASS. Full Worldserver link, Core callback production and gameplay remain separate.
