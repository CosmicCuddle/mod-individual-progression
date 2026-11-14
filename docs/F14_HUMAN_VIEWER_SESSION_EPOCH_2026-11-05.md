# F14 independently issued human viewer session epoch (DRAFT/OFF)

**Owner:** CosmicCuddle
**Developer:** CS01 Server-Side-Maintenance
**Date:** 5 November 2026
**State:** isolated development source, not merged, installed, enabled or gameplay-tested

## Why this source is needed

Earlier [DRAFT #103](https://github.com/CosmicCuddle/mod-individual-progression/pull/103) defined a safe pure evidence adapter for #93/#95 source-tagged Core per-GUID server send-call attempts, using Claude's **unchanged** #94 prior-hidden ledger. [DRAFT #104](https://github.com/CosmicCuddle/mod-individual-progression/pull/104) compiled a real, **OFF-by-default** IP `WorldObjectScript` observer that receives the hooks and retains only aggregate counts. It refused to claim an authenticated viewer login epoch or a creature incarnation, because the Core send-attempt callback does not supply either.

This child adds an **independent human viewer login epoch**, not a creature incarnation and not a full #103 bridge:

1. An actual opt-in `PlayerScript` on the existing IP lifecycle C++ compilation unit issues a new strictly increasing nonzero 64-bit server-process epoch on login for an actual Player with a non-headless session **and current normal-account classification** (neither bot nor excluded account). It **does not** use verified headless RandomBot actor epochs as human viewer identity, nor does it infer a session from a GUID alone.
2. The stamp is stored in the logged-in `Player` object's existing `CustomData` and binds the **full player ObjectGuid** to the unique epoch. No account ID, IP address, password, session address or arbitrary player pointer is persisted in the issuer.
3. `OnPlayerLogout` clears the stamp. A second login for the same GUID receives a **different** epoch. Bad input and unsigned counter exhaustion **fail closed without wrapping**.
4. The previously added #104 Core callback observer now requires `CurrentHumanViewerEpoch(viewer) != 0` for valid anonymous counters. Missing/in-world=false, null session, headless session, changed bot/excluded account classification or mismatched stored full GUID is rejected. No callback stores an epoch, modifies Claude's ledger, or grants concealment.
5. Both scripts and their includes/registration remain under the single `COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT=1` **development-build-only** guard and the existing F14 Core hook version guard. Normal builds do not register these scripts. F14 concealment and paid defaults remain OFF.

## Critical limits

An epoch stamped at login is a piece of **identity provenance**, not proof that a later event is executing on the correct owning map thread. In particular, it does not independently recover the **previous creature incarnation** once Core has unlinked a creature, does not prove per-viewer combat/quest/loot rights, does not establish a prior F14-hidden pair, and is **not** a 3.3.5a client CREATE acknowledgement. The #103 adapter stays **UNWIRED** and its static client/authorization queries still return FALSE.

An observer enabled only on a disposable native CI runner may build and link correctly, but it has not been launched; login/logout hooks and order must still be checked in an actual owner-approved development environment before relying on the provenance. There is no mechanism for this change to make a creature disappear or reappear.

## Files, tests and rollback

The child changes **seven reviewable paths**: adds `src/IndividualProgressionF14HumanViewerEpoch.h`, adds a small macro-gated `PlayerScript` and validation to `src/IndividualProgressionF14LifecycleScript.cpp`, adds `tests/f14_human_viewer_session_epoch.cpp`, `tests/test_ip_f14_human_viewer_session_epoch_source.py`, a CI workflow, this note and an append to `docs/CUSTOM_MODIFICATIONS_REGISTER.md`. Existing IP progression logic, Claude #94 source, paid files, Core fork, Playerbots/Naxx/Services, SQL, config, DBC, client and host remain unchanged.

CI source tests must reconstruct **byte-identical #104 lifecycle source** after removing just the new epoch code, verify immutable #92 registry and Claude #94 blobs, ensure no new hidden-client, packet or ledger operation, and check F14/paid flags OFF. The pure C++ test exercises non-reused monotonic issuance, UINT64_MAX refusal, login/logout/relogin and GUID mismatch. A **new actual combined native Worldserver build** is required before claiming Core C++ compatibility. Even native PASS is not a login/target/client acceptance test.

Only CosmicCuddle may approve a later coherent combined server test after full human rights, valid selected headless RandomBot-only hide policy, restored client-visible targeting/loot, installed source/schema/config/DBC inventory, proven backups and rollback. Rollback now: reject this DRAFT branch; the live server requires no changes.
