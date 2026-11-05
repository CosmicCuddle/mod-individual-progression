# F14 per-GUID batched CREATE send-attempt evidence — CS01 development prototype

**Owner:** CosmicCuddle. **Date:** 5 November 2026. **Status:** DRAFT, default FALSE/OFF, CI-only.
**Parent:** [F14 #93](https://github.com/CosmicCuddle/mod-individual-progression/pull/93) exact `98ddffb98a91012d2d96b532c3ec82275d5eed53`. Prior [#86](https://github.com/CosmicCuddle/mod-individual-progression/pull/86) per-viewer source-attributed suppression remains default FALSE.

## What this actually changes

The **seven Core paths** are patched only inside a disposable GitHub Actions checkout, layered **after** the exact #93 Core prototype. This is NOT a change to CosmicCuddle's protected Grim Core fork, IP runtime source, Playerbots, Naxx Core, Character Services, SQL, DBC, clients or host.

- Adds a new `WorldObjectScript::OnF14ShouldAttributeCreatureCreateAttempt(viewer, creature)` **default-FALSE** query, with **NO registered positive provider**. It is an attribution check, not a visibility decision.
- The ordinary stock-visible/nonpet creature CREATE path in `Player::UpdateVisibilityOf(T*, UpdateData&, ...)` first builds the real CREATE block and updates the server visibility link. Only if the NEW opt-in query separately establishes a previously F14-hidden exact pair does `UpdateData` remember that creature GUID. Ordinary distance, phase, zone or GM CREATEs are never automatically treated as F14 restores.
- Refines #93's metadata to one shared **512 GUID combined cap** across out-of-range and CREATE directions. Either-direction overflow poisons and clears **BOTH** metadata vectors. Packet construction remains byte-path unchanged.
- In `VisibleNotifier::SendToSelf`, after the existing `SendDirectMessage` call, the new default-empty `OnF14VisibilityBatchCreateAttempt(viewer, guid)` event emits the tagged GUIDs ONLY if no metadata overflow occurred. It is a **server function call attempt**, not a TCP/socket success, packet delivery, client rendering, restore, ledger completion, or permission to conceal.

The existing #93 out-of-range and previous source-attributed Core proof remain separate; the new query does not grant rights or change the Core's baseline visibility or client packets. No default-positive F14 policy is introduced.

## Why this is not a working hide/restore release

[#94](https://github.com/CosmicCuddle/mod-individual-progression/pull/94) models viewer session + creature-incarnation evidence without client ACK. This Core prototype does **not** wire #94 or #85's prior-hidden ledger, does not prove session/incarnation on its own, and **never** calls `CompleteRestoreOnlyWithClientProof` or `ClearFrozenAfterGlobalProof`. An eventual positive attribution listener would require complete owner-approved rights, exact previously F14-suppressed pair history, owning-map execution and session/creature incarnation validation.

A 3.3.5a client does not provide an object create/destroy acknowledgement. Real hide/reappear/targetability must be exercised in a controlled client test, and no previously hidden client state may be declared restored based only on this callback.

## Tests and rollback

- Source checks: exact five-path child diff, previous #93 source gates, default FALSE, no IP runtime or client/DB/host changes.
- Disposable native workflow: pins Grim Core `0d754d19`, historical Playerbots `e88cc409`, Naxx #292, Services #16, existing F14 Core afterpass and #86/#93 patches, then adds this narrow seven-path layer and compiles/links the Worldserver **without launching it**.
- Exact-head CI status is pending until the GitHub Actions job finishes; a compile success would not be gameplay proof.
- Roll back by closing this DRAFT and deleting its isolated branch; no live rollback is required because nothing was deployed.

**Still blocked:** positive verified selected BE/Draenei RandomBot-only policy; full real-human threat/assist/target/DoT/pet/tap/loot/quest/group/kill rights; WHO; per-viewer exact hide/restore wiring; 3.3.5a client visual/targeting tests; installed source/config/SQL/DBC/client inventory, backups/rollback and CosmicCuddle's release approval. All F14 and paid experiment flags OFF.
