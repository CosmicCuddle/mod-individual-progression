# F14 — revoke stale RandomBot evidence on disabled / early login

**Review DRAFT, 8 November 2026.** Owner CosmicCuddle. Parent [IP #129](https://github.com/CosmicCuddle/mod-individual-progression/pull/129) SHA `b7d4c5602d4222b4b3d9a93b13dd79a04f408ca3`.

## Exact failure in the earlier source
`IndividualPlayerProgression::OnPlayerLogin` returned on `!enabled` before calling the existing `ForgetF14VerifiedRandomBotGroupIdentity(player)`. A new login while IP was disabled, or before Player enters world, therefore would not evict an older verified bot's low-GUID Boolean, its epoch, or IP129's optional full-GUID/account/race publication. An earlier logout normally clears this too; this fix is defence in depth for missing/reordered lifecycle messages and reloads. **There is no evidence this happened on the owner host.**

The normal F14 login verification still uses the **original positive headless `IsRandomBot` test** when IP is active and the player is in-world. No new positive identity, Stage8, paid entitlement, WORLD object visibility or SQL action is created. The existing `Forget` method atomically erases previous group identity, login epoch and optional full-identity record, then expires the old attack lifecycle evidence.

## Narrow fix and evidence
For every non-null Player arrival, call Forget **before** the `!enabled || !player->IsInWorld()` return. A null Player is still ignored safely. Separately, the optional read-only `TryReadF14VerifiedActorPublication` now clears output and refuses to return a record when `enabled` is false. The IP129 compile guard remains undefined/OFF by default, and the IP128 adapter still returns **literal FALSE** for both Core6 actor and WHO queries. No old Source custom changes or owner SQL modified.

New Python tests enforce exact source order, mutate it back to the old bad ordering, ensure no extra Stage8/hide actions, and rerun inherited IP129/IP128/IP127 F14 source guards. CI is **source only**, not native Worldserver, gameplay or a concurrency proof. A snapshot can still expire immediately after releasing its shared lock; the true/positive per-viewer phase, send, rights and restoration protocol remains unimplemented.

**Rollback:** abandon only this new child and return to immutable IP129. Before any owner-approved offline test/install, protect tracked/untracked/**ignored custom SQL cleanups and migrations**, actual source/config/other mods/binaries/DB/DBC/client, confirm tested backups and restore and effective `Playerbots.DeleteRandomBotAccounts=0` for the disposable DB. No merge, live host, client, server or DB action here.
