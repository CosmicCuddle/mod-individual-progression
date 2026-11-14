# Historical / current IP regression separation — CosmicCuddle

Status: DRAFT CI-only child of IP155. No gameplay, settings, SQL, database, DBC, client or deployment change. No historical workflow or test is disabled, edited or waived.

## Confirmed history

- C3/partial F14: [IP19](https://github.com/CosmicCuddle/mod-individual-progression/pull/19), exact `d53eb71c39c35f2ed488f2315565f89d8be71aa7`, [original green run 41248206246](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41248206246).
- Paid entitlement: [IP23](https://github.com/CosmicCuddle/mod-individual-progression/pull/23), exact `8dc16ffb3ca7f70a9c7ddc7cfbe6c5bd40088e37`, [original green run 41326659049](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41326659049).
- Combined F19/paid/GM: [IP32](https://github.com/CosmicCuddle/mod-individual-progression/pull/32), exact `6c180c0a78cdadcf6c79e8e627f2cc2e91e0fdae`, [original green run 41361694398](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41361694398).

The inherited workflows continue invoking exact historical source checks against newer PR heads, including when only the append-only custom register changes. Their old pinned Worldserver builds also expect earlier Core, Playerbots, Naxx and IP source blobs. Recent failures are genuine contract mismatches; they are not automatically harmless. Original green runs establish only those historical snapshots.

## New CI separation

1. Replay original source and pure C++ policy tests independently on each of the three immutable historical IP refs. The existing original workflows and tests are untouched. No historical native Worldserver rebuild is claimed by this lightweight replay.
2. On the exact current PR head, require byte-identical protected gameplay source, configuration and SQL paths relative to IP142, the last verified fully linked five-owner tuple in [Services55 run 43191601513](https://github.com/CosmicCuddle/mod-character-services/actions/runs/43191601513). Run existing current tests for missing leader, group floor, disabled TBC sale, invalid cap, cached viewer/actor policy, raw/effective stage equivalence, Hunter/group source and receipt full-slot/group floor models.
3. Do not treat the new workflow as resolving all old failures. The remaining work is to audit deliberate source differences between the historical refs and current IP142, verify intended paid/F19/C3 semantics and actual gameplay, and only then explicitly review an appropriate trigger split/current full regression gate. Preserve the exact old tests and results. Do not remove or weaken checks for green status.

Current IP155 is a separate F14 departure-storage child, not yet fully linked. Services56 compiles the frozen Core16/IP153 tuple, not IP155. All F14/Paid flags remain OFF. Final synchronized release, R2, actual runtime positive admission, restoration, S5/S6 outstanding. Rollback: abandon this CI-only child; no production state to revert.
