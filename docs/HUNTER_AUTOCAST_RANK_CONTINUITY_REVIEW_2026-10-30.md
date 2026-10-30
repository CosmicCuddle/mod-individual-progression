# Hunter pet autocast rank continuity — development review and acceptance plan

**Owner:** CosmicCuddle  
**Date:** 2026-10-30  
**State:** EXPERIMENTAL development-only candidate. **Not compiled, not gameplay-tested, not installed, not merged or released.**  
**Base:** `develop/cosmic-ip`, matching protected source baseline `dd96f2b77e3ebfa2a4726bf785525ba133f1ab61` when this work began.  
**Authoritative policy:** [Coordination Issue #3 owner decision](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/3#issuecomment-6524119930).

## Goal

When IP replaces an allowed spell rank with another rank in the **same ability chain** on the **same loaded Hunter pet**, continue its existing native ON/OFF autocast state and valid action-bar slot. This is in-place rank continuity, **not** the A02/N-Core saved-settings restoration feature. Maintain P1 postcombat manual apply, K1 distinct-pet privacy/lineage, D18 combat prohibition and existing bot exclusions. Do not learn prohibited spells or change progression rules.

## Changed code scope

Only `src/IndividualProgressionSpells.cpp` is modified.

- In `checkHunterPetSpells`, abort when the current pet is still loading or absent from the world.
- Prior to a legal replacement, inspect the existing pet spell-map active state, autocast list, rank chain and action bar. Eligible transfer requires exactly one old known rank, a compatible target, matching ON/OFF across all representations, and exactly one valid bar slot. An additional active same-chain spell outside IP's intended list, or an orphaned same-chain ID in the autocast list, **blocks state transfer**.
- For a validated old rank, disable the *old* autocast ID if ON, unlearn with the validated slot retained, learn the new rank, then synchronize native autocast and the bar. Native action bar replacement must be verified; if it fails, leave the new spell OFF and remove an old ghost slot where detectable.
- For all other cases, use existing IP's remove/learn pattern. No transfer across separate pets, families/abilities, missing rank, multiple old ranks, `keepId=0`, passive spells or contradictory state.
- The queued delayed pet event captures the originating pet GUID; Hunter checks verify the current pet matches before reconciliation. The existing retry cap remains in place. If a Hunter pet is still loading when retries expire, that pet's own CustomData records `afterLoad`; an additional existing-core `OnPlayerAfterUpdate` callback checks it once loading finishes and performs the delayed same-pet reconciliation **one time**. The callback never operates on another pet and makes no additional database queries. Warlock reconciliation remains on the original event path.
- Warlock progression mapping and the original family/dummy spell lists are not intentionally altered. No changes to SQL/config, Playerbots, core, DBC, patches or live state.

**Implementation evidence boundary:** This code was written against historic public core APIs, especially [Pet.cpp](https://github.com/Grimfeather/azerothcore-wotlk/blob/b7f06a16ac7120b5abc2e8d3f96a94265a363224/src/server/game/Entities/Pet/Pet.cpp) and [CharmInfo.cpp](https://github.com/Grimfeather/azerothcore-wotlk/blob/b7f06a16ac7120b5abc2e8d3f96a94265a363224/src/server/game/Entities/Unit/CharmInfo.cpp), and the protected IP baseline. **These are not proof of the installed owner's core API or runtime behaviour.**

## Risks requiring source review and real tests

1. **Action bar and packet timing:** `Pet::learnSpell` may initialize the client spell bar before autocast state is synchronized. Check final client UI after the follow-up initialization; verify no spurious duplicate learn/remove messages or slot shifting.
2. **Failed learn / unexpected native bar replacement:** Fail-closed handling must be verified on a controlled test pet. The code is **not transactional** and cannot promise an in-memory rollback after partially completed unlearn/learn operations.
3. **Old autocast list hygiene:** The historical native `removeSpell` does not explicitly remove the old ID from `m_autospells`. The candidate clears it for the validated transfer path. Other legacy removal-only lines are unchanged; verify whether their existing behaviour already leaves stale IDs.
4. **Rank-chain validation:** IP dummy spell translation may not align with the installed core's spell rank chains for every pet family and custom spell. Unsupported or ambiguous chains intentionally skip transfer.
5. **Loading/identity:** Pinned historical [Pet::LoadPetFromDB](https://github.com/Grimfeather/azerothcore-wotlk/blob/b7f06a16ac7120b5abc2e8d3f96a94265a363224/src/server/game/Entities/Pet/Pet.cpp#L204-L500) calls `Create(...)` **before** `InitStatsForLevel`, so the runtime pet GUID exists before that hook in this source; the same routine does not clear `m_loading` until the async database callback completes. This does **not** prove equivalent ordering in CosmicCuddle's installed core. If loading exceeds the original 10-retry cap, the new Hunter-specific `afterLoad` marker survives on the same Pet and `OnPlayerAfterUpdate` reconciles once fully loaded. Historical [PlayerUpdates.cpp](https://github.com/Grimfeather/azerothcore-wotlk/blob/b7f06a16ac7120b5abc2e8d3f96a94265a363224/src/server/game/Entities/Player/PlayerUpdates.cpp#L428-L435) actually dispatches this hook. **Not compiled or tested**: live hook ordering, delayed event cancellation, loading never completing, pet swaps and unloaded owners remain functional acceptance cases. A runtime pet GUID identifies the originating loaded object, **not** permanent pet-birth lineage.
6. **P1/K1/D18 boundaries:** No persisted-setting replay, pet-to-pet copy, human click receipt, or protected in-combat action is implemented. The existing A02/NoWrite gates remain OFF.
7. **Normal pet persistence:** The core already saves pet spells/active states in `pet_spell` and pet action-bar data in its native pet save. No secondary table or SQL is added. Still validate successful save/reload and ensure the new behaviour does not write settings to other pets.

## Additional historical core compatibility cross-check

At the **same historical commit** `b7f06a16ac7120b5abc2e8d3f96a94265a363224`, the following five files have **identical Git blob SHAs** in both `mod-playerbots/azerothcore-wotlk` and `Grimfeather/azerothcore-wotlk`:

- `src/server/game/Entities/Pet/Pet.h`
- `src/server/game/Entities/Pet/Pet.cpp`
- `src/server/game/Entities/Unit/CharmInfo.h`
- `src/server/game/Entities/Unit/CharmInfo.cpp`
- `src/server/game/Scripting/ScriptDefines/PetScript.h`

This reduces ambiguity **between those two references at the pinned revision**, but **does not establish that CosmicCuddle's running core matches that revision** or that the draft compiles. It also does not verify changed SQL/DBC/client data.

The pinned `PetScript.h` has `OnInitStatsForLevel` and `OnPetAddToWorld` but **no dedicated post-async-spell-load callback**. The draft therefore adds a narrow Hunter-only `PlayerScript::OnPlayerAfterUpdate` recovery for the originally scheduled Pet after the existing timed retry cap, using pet-local `CustomData` flags. Source inspection confirms [`PlayerUpdates.cpp` invokes the hook](https://github.com/Grimfeather/azerothcore-wotlk/blob/b7f06a16ac7120b5abc2e8d3f96a94265a363224/src/server/game/Entities/Player/PlayerUpdates.cpp#L428-L435) and [`DataMap::Get` is a non-allocating lookup](https://github.com/Grimfeather/azerothcore-wotlk/blob/b7f06a16ac7120b5abc2e8d3f96a94265a363224/src/common/Utilities/DataMap.h). **Unverified on the owner server**: that this solves delayed loads under the actual core and does not create duplicates or a per-tick performance issue. It does not poll SQL or schedule infinite pet events.

## Source-only regression checks performed (not a compile)

The current revision also statically verifies that `afterLoad` is set only at the bounded Hunter timeout, cleared before the recovery callback invokes reconciliation, and registered as a separate Hunter-only `PlayerScript`. The fallback uses `DataMap::Get` (not `GetDefault`) on ordinary player updates to avoid creating pet-local records each tick and reuses a static key. These are code-review checks, not observed runtime timing or performance tests.


Compared the proposed branch against `develop/cosmic-ip` using GitHub file contents on 30 October 2026:

- The entire Hunter **family-rule section** (from the Wolf condition through the next IP function) matched byte-for-byte; **111** existing `reconcile(...)` calls were preserved. Existing commented Cower mappings remained commented.
- The candidate still has the same two delayed-event allocation sites; its constructor now also carries the originating pet's runtime GUID.
- Reviewed the fail-closed checks for ambiguous ranks/autocast IDs and confirmed no new SQL/config mutation tokens appeared in the changed Hunter source.
- The branch modifies one C++ file plus this review document.

These are **static source comparisons only**. They neither compile C++ nor reproduce client/server gameplay. Actual installed-core API differences, spell database ranks, packet order and successful save/load still need approved build and in-game tests.

## Minimum acceptance matrix — test only after separate owner approval

| Scenario | Required outcome |
| --- | --- |
| Hunter Wolf Bite known ON, IP selects legal higher same-chain rank | New rank ON; old rank removed; no stale autocast ID; same action slot/type |
| Same case with old OFF | New rank OFF; preserve observed native OFF only, not a durable human-choice receipt |
| Legal lower rank replacing higher in same chain | ON and OFF cases work; no unexpected rejection or extra spell |
| Already-correct legal rank | No unnecessary toggles, extra learns or bar changes |
| Fresh ability with no valid predecessor | Default native OFF; no invented ON or cross-chain transfer |
| Removal-only `keepId=0`, forbidden or Cower-related paths | Existing removal/gating unchanged; no unauthorized learn or transfer |
| Multiple old ranks, invalid chain, no bar entry, mismatch, duplicate slots | No state transfer; validate safe legacy handling and absence of ghost bar/stale autocast references |
| Event queued for Pet A, Pet B becomes current | Pet B receives no write from Pet A's event |
| Still loading after bounded retries, then completes | No mutation during loading; Hunter-only `OnPlayerAfterUpdate` completes same-current-pet reconciliation exactly once |
| Pet never completes loading / is dismissed before loading finishes | No rank transfer or cross-pet work; no unbounded event scheduling, pet-local marker discarded with pet |
| Human vs bots | Existing bot exclusions and spell rules unchanged |
| Save pet, stable, summon, relog, new same-named pet | Native same-pet state persists where valid; never leak to distinct pets |
| Client packets after successful replacement | Final PetSpellInitialize consistent with server spell map/autocast list/bar, no bad messages |
| Other families and Warlock pet | No unintended regressions |
| Combat and P1/K1/D18 scenarios | No automatic replay of saved settings, no new combat-protected action |

## Owner-run, read-only Git source fingerprint (prepared, not executed)

This is a **first-pass version check only**. When CosmicCuddle separately authorises checking the host, open a terminal **in the existing server's AzerothCore source checkout**, not the production executable directory, and paste the block below as one unit. It reads Git source information. It does **not** pull, fetch, reset, compile, restart, edit files, query databases or inspect secrets.

```bash
for item in "core:." "Playerbots:modules/mod-playerbots" "IP:modules/mod-individual-progression" "NaxxramasModule:modules/Mod-Naxxramas-Core"; do
  label=${item%%:*}; dir=${item#*:}
  if [ ! -e "$dir/.git" ]; then
    printf '%s: no separate Git checkout at expected path\n' "$label"
    continue
  fi
  sha=$(git -C "$dir" rev-parse --verify HEAD 2>/dev/null) || continue
  branch_name=$(git -C "$dir" symbolic-ref --short -q HEAD 2>/dev/null || printf 'DETACHED')
  if git -C "$dir" diff --quiet --no-ext-diff && git -C "$dir" diff --cached --quiet --no-ext-diff; then
    tracked="tracked-clean"
  else
    tracked="tracked-modifications-present"
  fi
  if [ -n "$(git -C "$dir" ls-files --others --exclude-standard | head -n 1)" ]; then
    untracked="untracked-files-present"
  else
    untracked="no-untracked-files-seen"
  fi
  printf '%s | %s | %s | %s | %s\n' "$label" "$sha" "$branch_name" "$tracked" "$untracked"
done
```

**Safety and interpretation:**

- The four directories above are **examples**, not verified owner-host paths. "No separate Git checkout" means **this location did not match**; it does not prove the component is uninstalled. If modules are elsewhere, adjust the local paths with the owner's consent.
- Run from the source checkout and only when the host inspection is authorised; these commands **do not install or deploy anything**. They do not output remote URLs, configuration contents, SQL credentials, account IDs or file diffs.
- A `tracked-clean` state does **not** prove every installed custom modification is in Git: untracked files may exist; a built binary can differ from the checkout; local configs, DBC/client files and database migrations are not covered.
- **Do not paste any private remote URL or unredacted `.conf`/database content into GitHub**. Share only the five fields above, plus an explanation if any directory differs.
- This is **not** a backup, backup-restore exercise, build or gameplay test. The actual binary and active settings still require separate evidence under [coordination #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).

## Test-readiness decision at this revision — NOT READY

This draft has reached a **source-review candidate** state, not a compiled or accepted test release.

| Gate | Current evidence | Status |
| --- | --- | --- |
| Owner gameplay rule | CosmicCuddle approved same-pet same-chain ON/OFF continuation in coordination #3 | Confirmed |
| Protected source and IP family mappings | Draft based on `develop/cosmic-ip`; all 111 Hunter family `reconcile` entries compared unchanged | Source-checked |
| Bounded delayed-load recovery | Hunter-only `OnPlayerAfterUpdate` continuation added; pinned core declares and dispatches hook; pet-local marker | Source-checked, **not runtime-tested** |
| Actual installed core/IP/Playerbots and customisations | Host binary/source/config fingerprint not yet supplied | **BLOCKED** |
| Isolated compile/link and automated test | No reported successful run for this exact PR head | **BLOCKED** |
| Save/stable/relog + client action-bar/packet tests | No controlled WoW client/server test | **BLOCKED** |
| Backup/restore and test rollback | No verified restore exercise or approved test environment | **BLOCKED** |
| Merge and production deployment | Separate owner approval mandatory | **NOT AUTHORISED** |

**First possible test:** an **isolated compile/link smoke test**, after installed-source fingerprint and compatibility planning. A live-world pet test is **later**, after backups, isolated gameplay setup and separate approval. Owner can use the read-only instructions above to supply the source fingerprints with minimal typing; no secrets or database dumps are needed.

## Engineering and release gates

1. **Read-only host inventory (consent required):** capture exact core/IP/Playerbots/custom-module HEADs, local modifications, effective config, actual DBC/SQL/client dependencies and running-binary provenance without exposing credentials. Compare against the candidate's APIs, particularly pet loading, spell rank reconciliation, CharmInfo, and event scheduling. [Coordination #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4).
2. **Code review:** check that every modified call exists and has identical semantics in the actual target core; adjust in this draft PR only if supported by evidence. Review custom changes for collisions. Do not merge blindly from upstream.
3. **Independent isolated compile/link:** compile with the verified core and modules in a nonproduction environment. A green compile is **not** gameplay acceptance.
4. **Backups and recovery:** before owner-approved in-game tests, verify restorable database/config/working build backups and a tested rollback mechanism. Merely reverting a code commit does **not necessarily undo already-saved pet spell state** after gameplay tests.
5. **Controlled gameplay regression:** apply the acceptance matrix to disposable test pets / test copies, compare server + client state, and obtain CosmicCuddle's acceptance.
6. **Separate approval:** only after all gates, ask permission to merge and to deploy. Do not push to live server, modify the live DB, change config gates or restart based on this review PR.

## Rollback sketch (not executed)

- **Source:** abandon/close the experimental branch or revert its change before any production deployment.
- **Test environment:** restore the known-good binary/source and any changed test-state from the **verified** backup as appropriate, then re-run native pet save/load checks.
- **Production:** not applicable; no deployment authorised or performed. If ever deployed, use a separately approved versioned release and restore plan.

## References

- [Coordination #3 — Hunter feasibility, decision and engineering design](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/3)
- [Coordination #4 — installed dependency and restore-readiness investigation](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4)
- [Protected historical IP source](https://github.com/CosmicCuddle/mod-individual-progression/blob/dd96f2b77e3ebfa2a4726bf785525ba133f1ab61/src/IndividualProgressionSpells.cpp)
- [Historical core Pet.h and Pet.cpp](https://github.com/Grimfeather/azerothcore-wotlk/tree/b7f06a16ac7120b5abc2e8d3f96a94265a363224/src/server/game/Entities/Pet)
- [Historical core CharmInfo.cpp](https://github.com/Grimfeather/azerothcore-wotlk/blob/b7f06a16ac7120b5abc2e8d3f96a94265a363224/src/server/game/Entities/Unit/CharmInfo.cpp)

**Status at document creation:** source candidate and this test plan only. **No build, test, backup, migration or live change has been verified.**
