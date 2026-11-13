# F14 pass identity lifecycle hardening — draft, source only

Owner: CosmicCuddle. Coding support under CS01-IP Maintenance.

## Source and scope

- Repository: `CosmicCuddle/mod-individual-progression`
- Branch: `feature/coding-support-f14-pass-identity-lifecycle-20261113`
- Parent: `aed05a92f262362608f5d643c8570b504189aae0` (IP172). The pass tracker and its standalone test are byte-identical between this parent and IP168 `455597329f2578c634091b8c3a79d83b31181d40`.
- Changed paths: `src/IndividualProgressionF14CurrentPassIdentity.h`, `tests/f14_current_pass_identity_test.cpp`, `tests/test_f14_pass_identity_lifecycle_source.js`, this note, and an additive Custom Modifications Register entry.
- Claude CS03-IP's pair-rights adapter branch and its files are untouched. This branch does not merge or synchronize modules.

## Behavior

The tracker still admits at most 64 simultaneous active passes. One viewer's nested Begin, reordered/unknown callback, duplicate End, or identity mismatch quarantines that viewer's exact identity (GUID, login epoch, map generation, map and instance). An unrelated viewer's active pass remains valid. Nested passes are **not** supported: Begin A, Begin B, End B, resume A, End A leaves A/B refused and the viewer quarantined. A new login epoch or map generation is a different identity, but a retained active pass with the same GUID conservatively refuses an overlapping new pass until the old pass is revoked or ended.

Closed tokens are retained in a fixed 256-record array by default. A definitively closed token cannot be accepted by Begin again, regardless of token value or callback order. A rejected capacity overflow token is also retained. The tracker never evicts a live slot or a closed token. On closed-history exhaustion, new admissions close for the lifetime of the tracker. Existing active passes can still be inspected and ended. This is a **safety/liveness tradeoff**: without a Core-provided quiescent token watermark or a bounded token-generation domain, exact forever replay rejection and unlimited new admissions cannot both be provided with finite memory. The default 256 history entries are a regression-test sizing choice, **not a production-ready operating limit**. CS01 must review a safe retirement/rotation protocol before enabling this observer for sustained use.

Global poison is reserved for a token simultaneously claimed by two different viewer GUIDs, or an explicit independently proven whole-table corruption call. It disables all activity. Ordinary capacity exhaustion and viewer-local ambiguity do not set global poison.

The new `RevokeViewer(ViewerIdentity)` API requires the full viewer identity. A delayed revoke carrying an old identity cannot erase a newer session or map-generation pass. The existing runtime `CurrentCoreLifecycle.cpp` call passes only a GUID. Because that file is in Claude CS03-IP's active pair-rights adapter PR, this branch leaves it byte-identical to the parent; the GUID overload now **refuses without deleting any slot**. This preserves safety but can leave stale active evidence, consume capacity and conservatively refuse a new pass for the same GUID. CS01 must integrate an event-time identity into the runtime call after adapter review. Simply reading `CurrentPassViewer` at callback time would be insufficient if an old callback could observe a newly rebound marker; callback ordering or a captured old identity must be proved first.

The tracker does not own restoration obligations. No operation authorizes concealment, clears #94 restore debt/prior-hidden records, resets the F14 recovery fence, changes stage/progression, or infers delivery/client ACK. F14 and Paid stay OFF. R2 / Coordination #42 remains blocked.

## Reproduction

Run locally when GCC or Clang is available:

```sh
for compiler in g++ clang++; do
  for standard in c++17 c++20; do
    for define in "" -DNDEBUG; do
      "$compiler" -std="$standard" -Wall -Wextra -Werror -pthread $define -Isrc tests/f14_current_pass_identity_test.cpp -o /tmp/f14-pass-identity
      /tmp/f14-pass-identity
    done
  done
done
node tests/test_f14_pass_identity_lifecycle_source.js
```

The standalone test covers normal Begin/Batch/End; nested A/B; unrelated viewer isolation; closed replay; duplicate and unknown End; stale login epoch and map generation; delayed old-identity revoke and safe legacy GUID refusal; exact capacity and capacity+1; slot reuse; local quarantine; closed-history exhaustion; and genuine global corruption. The source contract checks bounded arrays, refusal of GUID-only deletion, unchanged runtime callsite, and absence of concealment/restoration-debt authority.

## Evidence and release gate

Local validation on 13 Nov 2026: `node tests/test_f14_pass_identity_lifecycle_source.js` passed; `git diff --check` passed; the Claude adapter source paths remained byte-identical to the parent. GCC, Clang, MSVC, Python and WSL were not installed in the local environment, so the C++17/C++20 normal/NDEBUG matrix was **not run**. No hosted CI was used.

This is source-only code until the native tests and a macro-ON full Core25 Worldserver link are run on the exact head. A standalone model pass would not establish callback ordering, actual gameplay, restoration acceptance, or deployment. No hosted CI run is authorized for this draft. The parent documentation describes the previous global poison behavior; this entry supersedes that behavior only on this child branch.

Rollback: abandon/revert this child branch before integration. No database, SQL, DBC, client, config, host, merge, or production action is part of this draft.
