# PR19 + reviewed GM null-safety integration — DRAFT (1 November 2026)

**Owner:** CosmicCuddle. **Implementer:** Server-Side-Maintenance CS01-IP. **Review:** CS03-IP code coordination in central Issue #13; CS02-IP Grok remains on user-reported break. This is a new isolated child, not an amendment to the successfully built or frozen PR #19.

## Exactly what was integrated

- **Parent [Individual Progression DRAFT #19](https://github.com/CosmicCuddle/mod-individual-progression/pull/19):** `d53eb71c39c35f2ed488f2315565f89d8be71aa7`, which combines Hunter autocast + `EnforceGroupRules` 0/1 expansion grouping + offline/null leader guard + optional **default-OFF** partial F14 saved-stage RandomBot login guard.
- **Source of independent GM fix [DRAFT #14](https://github.com/CosmicCuddle/mod-individual-progression/pull/14):** `d05f51a1f68fdf25e8f705dd1deddbcb397f22fc`. The GM source blob at #19 was independently checked to be **byte-identical to #14's original parent** (`dd5754716e5b5546fa05324dc9e2250334cd5313`); therefore this is a direct copy of already-reviewed GM source, not a manual conflict resolution or new rewrite. The resulting `src/cs_individualProgression.cpp` SHA must equal **`654119d29e0519600d50b179ccd5a9c5df8695b0`**.

## Preservation and scope

Exactly six paths changed **relative to frozen #19**, including:
1. `src/cs_individualProgression.cpp` — exact reviewed #14 GM safety code.
2. `tests/test_ip_gm_command_null_safety.py` — exact original 15 Python source regressions from #14.
3. `tests/test_ip_combined_c3_f14_partial_login.py` — adapt only one negative control that previously required GM source to match the *old* #16 blob; now instead assert the exact reviewed #14 GM blob. The other Hunter, C3, group, F14 source tests remain.
4. `.github/workflows/ip19-gm-null-safety-integration-20261101.yml` — independent six-file scope, append-only register, exact source blobs and 15 GM source regressions; no new duplicate full native job.
5. This review memo.
6. `docs/CUSTOM_MODIFICATIONS_REGISTER.md` — append-only preservation entry.

All `src/IndividualProgression.cpp`, `src/IndividualProgressionPlayer.cpp`, `src/IndividualProgressionSpells.cpp`, `src/IndividualProgressionGroupPolicy.h`, `src/IndividualProgression.h`, and IP `.conf.dist` remain **byte-identical** to the frozen #19 parent. No Core, Playerbots, Naxx, SQL, DBC, migrations, client patch or live configuration modification. The **inherited #19 workflow** still runs source regression plus a full pinned Core/PB/Naxx Worldserver compilation/link on this child head; only its own #19-only nine-file freeze scope is skipped by the pre-existing narrow branch condition. The inherited #16 source workflow has its own exact-branch scope guard, not a blanket source-test skip.

## GM command intended behavior

- `.ip set` and `.ip tele` respect a supplied player identifier before falling back to selected target/self; unavailable or offline/unloaded targets fail before a stage/achievement/teleport write.
- `.ip setbot` checks for a console-null `WorldSession` and absent/unloaded player before group access.
- `.ip setrep` and `.ip attune` check for missing console session/player before dereferencing and modifying state.
- Original command RBAC, valid-path progression writes, quest/achievement adjustment, command syntax and output behavior are not redesigned. **Numeric first arguments** and explicitly named targets require controlled manual parser tests. This integration does **not** make a valid but unsafe manual progression downgrade safe.

## Required evidence and release gates

- [SRC] Exact six paths, #14 GM source blob, all six protected PR19 source/config blobs unchanged, append-only register and 15 GM source checks. [SRC/SYNTH] Inherited PR19 Hunter/C3/group/F14 tests. None is a real console/client test.
- [NATIVE] **New exact-head** full historical-pinned Worldserver configure/compile/link on disposable CI, including `cs_individualProgression.cpp`, not merely inherited #19 or isolated #14 successes. Original pinned Core `8daffdabe9202c228e45d833c9a1d399597a15cd`, PB `e88cc4099e69f35126b28942a412ca702d20453b` with two explicitly disclosed runner-only compatibility edits, Naxx `80e9de1c39a6dc995188a80c31a5e785d0f2a6f9`. Do not mark success until the exact-head original job log has the final `worldserver` link.
- [GAME] Still **NOT TESTED**: `.ip set` with an explicit name/GUID versus an actual selected target, missing/offline targets, numeric-first parser input, all five commands via in-game/console, bot group sync, preserved saved-stage quests, Hunter pet spell rank autocast, mixed-tier group accept, current-boot RandomBot login and default-OFF baseline.
- [PROD] **NO-GO** until owner privately confirms installed source/dirty changes, effective config (including PB account prefix vs IP bot regex), DB/schema/SQL mode, restorable backups and controlled gameplay, and explicitly approves any merge/deployment. This child remains DRAFT.
- **Unfixed first-milestone blockers:** full BE/Draenei RandomBot stage8 initialization/population/visibility; PB bulk reset or direct quest-drop marker protection 66001–66018 and owner's target/PvP choices; group OFF pre-accept stage-write hazard and PB direct add bypass. Standalone #14 GM command security=0 policy is not broadened here.

## Rollback

Discard the new child branch/PR or revert its proposed diff before any merge. The untouched #19/#14 parent branches remain available. In a separately authorised future staging environment, restore approved character/world DB backups if tests change saved rewarded quests; binary rollback alone does not repair deleted marker rows. Never use Playerbots `DeleteRandomBotAccounts` to repair progression.

**Evidence boundary:** A draft branch, docs and automated compile do not establish gameplay, live configuration or owner deployment.
