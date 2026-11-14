# IP Core6 actor and WHO bridge — compilation-only, always visible (8 November 2026)

**Owner:** CosmicCuddle. **Base:** [IP #127](https://github.com/CosmicCuddle/mod-individual-progression/pull/127) exact `40665dffc717d32666da38c89e8d692be57e5138`. **Status:** DRAFT, default OFF, UNWIRED from any positive policy.

Newer Core5 and Core6 introduced `WorldObjectScript::OnF14ShouldExcludeWhoEntryForViewer(Player const*,ObjectGuid const&,uint32)` and `OnF14ShouldSuppressPlayerForViewer(Player const*,Player const*)`. Both use **TRUE = HIDE**, while old IP120 `OnWhoListEntryCanBeSeenByPlayer` used **TRUE = ALLOW** and a different API. They cannot be forwarded without adapting both ABI and return semantics.

A single actual IP lifecycle compilation-unit `WorldObjectScript` subclass now explicitly registers the TWO new enums under `COSMIC_IP_F14_CORE6_ACTOR_WHO_NEGATIVE_ADAPTER_EXPERIMENT >= 1` only. The macro is not defined anywhere in normal builds. Its overrides always **return FALSE** without consulting RandomPlayerbotMgr, cached WHO GUID, paid stage, session, or mutable map state. Thus even opting into compilation cannot hide a real human, Altbot or RandomBot. Core5 WHO and Core6 actor caller macros separately remain undefined/OFF. Original old IP WHO/script preserved exactly.

The purpose of a subsequent exact-commit five-owner static Worldserver CI job with only this adapter macro ON is to verify the real new Core6 WorldObjectScript hook signatures and registration/vtable against IP with PB10, Naxx292, and Services unchanged. No game, SQL, DB, client, packet, stage8, quest, group, targeting, previous-hidden restoration or runtime positive behavior is exercised or implied.

The existing [IP #127 pure published-login policy](https://github.com/CosmicCuddle/mod-individual-progression/pull/127) remains a source/model only, NOT the actual provenance producer; the adapter intentionally does not use it to create synthetic permission. Before any non-false implementation, prove actual current headless genuine RNDBOT identity and timely logout/GUID-reuse revocation, human effective era+rights, WHO/player parity, NPC combat-only hide/complete restoration, native link and controlled real 3.3.5a tests.

**Rollback:** abandon this isolated five-path child. Do not merge/enable/deploy or alter owner machine. Preserve ALL tracked, untracked AND ignored custom SQL cleanups/migrations, source/config/DB/DBC/client/other-module/binary versions and prove restoration first. Confirm effective `Playerbots.DeleteRandomBotAccounts=0` on separately approved offline DB.
