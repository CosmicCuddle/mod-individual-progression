# F14 — IP-owned experimental positive Core6 actor /who viewer policy (9 November 2026)

**Owner:** CosmicCuddle. **Maintainer:** Server-Side-Maintenance CS01. **Status:** isolated review-only DRAFT, **OFF**. **Frozen parent:** [IP #137](https://github.com/CosmicCuddle/mod-individual-progression/pull/137) `7ca897e2397216405f910f46e6a12e5b4e0b987b`; no edits to IP137 parent, Character Services, Naxx, Playerbots, Core, SQL, client/DBC, DB, configs or host.

## What this changes

The original IP Core6 adapter in `src/IndividualProgressionPlayer.cpp` deliberately returned FALSE from `OnF14ShouldSuppressPlayerForViewer` and `OnF14ShouldExcludeWhoEntryForViewer` (TRUE **means HIDE** for Core6, unlike the old IP TRUE-means-ALLOW API). This child keeps that original negative adapter **byte-identical**, retains its exact FALSE behavior in **every ordinary build**, and leaves the Core6/PB15/IP137/Naxx292/Services S4 build unchanged. An isolated NEW class implements the positive-only option.

In a **separately deliberate, opt-in development-only build**, new macro `COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_VIEWER_POLICY_EXPERIMENT>=1` requires:
- the existing verified login publication compile macro `COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT>=1`;
- the existing Core6 actor/WHO ABI adapter compile macro `COSMIC_IP_F14_CORE6_ACTOR_WHO_NEGATIVE_ADAPTER_EXPERIMENT>=1`.

Without either prerequisite, compilation deliberately fails rather than guessing identity. The distributed `IndividualProgression.F14.ExperimentalRndbotPlayerVisibility = 0` remains OFF, and the original `IndividualProgression.cpp` config gate **forces this runtime option OFF** without a compiled, genuine `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION>=1` Core implementation. Core6 does not provide the accepted safe immediate-refresh bridge; this branch does not define or forge that version.

Only in that multi-opted future prototype, the same IP-owned adapter *could* return TRUE if all are true:
1. The viewer is an ordinary IP normal human account, has a real non-headless active session, is in world, is not GM and is not the target.
2. Effective per-character progression (which includes existing paid entitlement) is a **known Vanilla stage 0–7**, not TBC+ or an unknown 255. Both ordinary real humans and lower-era characters keep their usual Core visibility outside the exact positive case.
3. `TryReadF14VerifiedActorPublication` **returns current-generation positive RandomBot proof** issued on the login thread, with full GUID, nonzero verified source account and login epoch, matching race10/11. No headless-only, race-only, account-name or cached-visibility guess qualifies.
4. **Actual actor** callback additionally checks actor in world, currently headless non-GM session, source account **matches current actor session account**, current race and full GUID. **Cached WHO** cannot access live target Player or cached account; it checks the **fresh revocable publication** and cached race/full GUID only, never inventing an account comparison. Unknown/stale/reloaded identity fails visible.

This is not an implicit assignment or port of Naxx/Claude's [test-only positive selected RNDbot pure policy #295](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/295). It reuses its owner's logical acceptance rule **within IP's runtime authority** while leaving Claude's Naxx implementation and Claude CS03-IP's **separate E1 pass-ended replay fix** [Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13) untouched. Do NOT combine old true-means-ALLOW IP observer with Core6's new true-means-HIDE dispatcher or use IP GUID-only diagnostic caches without current login epoch.

## Exact eight paths and evidence

- New pure standard-C++ header `src/IndividualProgressionF14Core6PositiveRndbotViewerPolicy.h`.
- New `src/IndividualProgressionF14Core6PositiveRndbotAdapter.h`: distinct optional script class, compiled only under the new macro. It validates the real IP-published full GUID, cached/live race, current actor session account when available, nonzero login epoch and human paid-aware observer.
- `src/IndividualProgressionPlayer.cpp`: **original false-only adapter class remains exactly byte-identical**. One new guarded include before it, and one guarded registration choice: original class in ordinary builds; separate positive class only in explicitly triple-opted builds. Removing these two inserts reconstructs original IP137 Player.cpp exactly. All earlier login, reward, group, Hunter, paid and stage logic remains original.
- New `tests/f14_core6_positive_rndbot_viewer_policy.cpp` exercises exhaustive positive/negative, source/fullGUID/account/race/epoch, human, GM, self, headless, Vanilla vs TBC and absent proof cases under GCC/Clang C++17/C++20 normal/NDEBUG.
- New `tests/test_ip_f14_core6_positive_rndbot_adapter_source.py` proves exact original restoration, runtime OFF, bridge prerequisites, actual IP positive-source function, WHO no-target-dereference, producer never calls Playerbots/DB from viewer worker.
- One new scoped GitHub Actions workflow, this documentation and append-only Custom Modifications Register.

**No claim of CI PASS before the original hosted job completes.** This is *not* actual five-owner native Worldserver compilation with positive experiment defined, Core6 packet send, 3.3.5a client, correct GM/Altbot tap/loot/DoT/pet/assist, actual restored NPC after an attack, stage8 birth, paid activation or an accepted release candidate.

## Safety blockers and rollback

The existing `f14ExperimentalRndbotPlayerVisibility` and legacy IP `enabled` are non-atomic and `GetEffectiveProgression` can traverse per-character entitlement state; actual concurrent reload/map/WHO read guarantees need separate proven owner-Core/IP contracts. Do **not** add/claim thread safety by merely putting a mutex inside a pure policy. WHO target session ownership can only be inferred through **IP publisher freshness**, not a cached WHO row; source snapshot is not per-client ACK and must never clear a creature-restoration ledger.

The current Core6 actual actor/WHO callbacks are **default FALSE** and intentionally fail visible. There is NO accepted Core visibility-pass runtime producer, immediate viewer refresh/hide restoration protocol, client receipt, NPC/combat rights handling, genuine race-bot stage8 authority, or Claude E1 pass-ended replay fix. Any actual use is unsafe until all are independently reviewed and tested. No server configuration should enable this or force the Core refresh macro before it exists.

**Integration plan:** (1) review the isolated IP adapter and original C++ policy/source tests, (2) obtain Claude E1 correction and trusted Core pass/refresh producer, (3) address combat/quest/loot/pet/DoT/corpse restoration and true stage8 birth, (4) run exact Core/PB/IP/Naxx/Services full native Worldserver with only deliberate experimental compile checks and flags OFF, (5) owner S5 installed extras, dirty tracked/untracked/**ignored** SQL, DB/DBC/client/rollback/restore evidence, (6) CosmicCuddle-approved disposable offline two-human + selected RNDbot 3.3.5a gameplay. Separate owner approval before merge/deploy/activation.

**Rollback:** abandon this isolated draft and retain frozen IP137. All upstream and custom source, local SQL files, characters and live settings remain unchanged.

## Strict negative adapter regression found and corrected (9 November 2026)

The original first hosted [IP #138 CI #42622923884](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/42622923884) completed **FAILURE**: exact child diff and new positive pure C++ tests **PASSED**, but inherited `test_ip_f14_optin_publication_login_predicate` correctly rejected altering the original negative-only adapter and mentioning its publisher compile macro in `IndividualProgressionPlayer.cpp`. **This was a regression contract violation in the first draft, not a real game error.** The candidate has been redesigned rather than weakening or deleting the inherited safety tests: original Core6 false-only class remains byte-identical; optional new positive script class and publisher compiler prerequisites move into an independent header; the original compiled reader and publisher remain untouched. Updated source-contract asserts this exact preservation and the inherited IP136 test is rerun unchanged. New CI result must be checked before PASS claim.
