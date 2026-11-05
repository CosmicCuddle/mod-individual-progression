# First real-source F14 stage-floor plus rights-refresh integration (DRAFT, OFF)

**Owner:** CosmicCuddle. **Date:** 5 November 2026. **Maintenance code branch:** `research/cs01-ip-f14-rights-stagefloor-integration-20261105`.

## Why

Earlier PR #91 successfully built a full Worldserver with safe stage-floor and threat-refresh code *copied into a disposable CI checkout*; those fixes were still separate development branches. This DRAFT consolidates **actual mod-individual-progression source** so it can be reviewed and regression-tested as one coherent fork change later, instead of relying solely on test-runner overlays.

## Exact source ancestry and preservation

Base is the complete F14 #90 experimental queue-health/right-invalidation code (`4ec81739efef98adb6fcdc2fa15a731458bfba04`), including its #88 human damage/DoT/healer-assist queue extension, #83 logout stamp/owning-map refresh, and earlier preserved custom source.

Source integration copies **nine exact blobs** from Claude IP #80 rev2 (`c64c6f7ef496419d6fa82c054035892f1ab91864`): `src/IndividualProgression.cpp`, `src/IndividualProgression.h`, `src/IndividualProgressionPlayer.cpp`, `src/cs_individualProgression.cpp`, `conf/individualProgression.conf.dist`, `src/IndividualProgressionF14StageFloorPolicy.h`, and the three stage-floor source, pure model and GM successor test files. All five pre-existing source/config paths were verified **byte-identical between F14 #83 and #90 before overlay**, so this does not remove later F14 custom implementation. Only `tests/test_ip_f14_lifecycle_source.py` had a deliberate overlap; it is adjusted in place to check all **four** positive identity reasons while keeping the newer #88/90 rights and queue contracts, rather than replacing the entire test with a stale older version.

## Behaviour

- When explicitly enabled *in a future reviewed build*, the stage-floor durability guard refuses **IP-owned destructive force updates** that would lower already-earned progression of a positively verified selected Blood Elf/Draenei RNDBOT from stage 8–18 to below 8, even when an expansion cap is lower. It does not grant stage8, raise the cap, change human/Altbot state, touch the DB directly or prevent ordinary legitimate progression. `IndividualProgression.F14.ProtectVerifiedRandomBotStageFloor = 0` remains **OFF**.
- Existing #88 human damage/DoT/assist invalidation requests a value-only refresh of the affected creature on its owning map; #90 reports queue poisoning from ALL event sources, not only bot logout. This does not implement positive creature concealment or prove a 3.3.5a client restore. Viewer-specific #85/#86 protocols are not wired.
- Playerbots selected RNDBOT-only marker-preservation #49/#74 is **not** merged into Playerbots, only applied in a disposable native CI runner with empty/OFF keep-list. Unchanged Naxx Core #292 and Character Services #16 are pinned in CI. Grim Core `0d754d19` with reviewed experimental F14 patches is also ephemeral.

## Test, release and rollback

Source contracts check exact branch path scope, 9 Claude blobs, 4-reason login identity source, preserved #90 queue, cap-independent stage protection/GM paths, original F14 rights/logout and default OFF configs. GCC/Clang C++17/20 pure tests plus pinned full native Worldserver compile/link are required. **A link success is not gameplay** and the project has not checked installed host state or backups. All concealment/WHO, real player threat/DoT/assist/pet/group quest/loot/tap rights and client hide/reveal restoration remain blocking.

No protected branches, production code or DB/SQL/DBC/client/host operations. Owner approval, installed-version inventory, restorable backups, explicit regression plan and in-game gameplay acceptance must precede installation. **Rollback:** close/discard this DRAFT; original #80/#88/#90 drafts remain unchanged.
