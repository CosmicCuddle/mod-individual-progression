# IP: Unknown/offline group leader must not become inferred stage 0

**Owner:** CosmicCuddle
**Review:** [CS02/Grok independent C3 finding](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11), independently inspected by Maintenance in `CosmicCuddle/mod-individual-progression` current `develop/cosmic-ip` and the experimental expansion-grouping branch.
**Status:** Isolated source **DRAFT** only. No merge, production deployment, config/SQL/DBC changes or owner-host work.

## Reproduced source failure

The IP `OnPlayerCanGroupAccept(Player*, Group*)` callback obtains the group leader via `ObjectAccessor::FindPlayerByLowGUID` and immediately calls `GetPlayerProgressionFromQuests(groupLeader)`. The latter returns **0 when the argument is null**. For non-normal accounts (RandomBots, Altbots or excluded accounts) with `EnforceGroupRules=0`, the next branch checks `currentState != groupLeaderState` and calls `ForceUpdateProgressionState` on the joiner, which **removes all rewarded progression quest markers before re-awarding**. Thus an unknown/offline leader may silently appear to be a real Vanilla stage-0 leader, causing a destructive reset. With enforcement ON the original code can also make eligibility decisions using the fabricated stage0.

These are confirmed code paths in the baseline, but **whether owner production experienced them is unknown**; the running source, group topology and database state were not inspected.

## Minimal guard and preserved behaviour

- Move the pre-existing `!IndividualProgression.Enable → return true` ahead of leader/stage lookups. The disabled module still permits grouping.
- Retrieve the leader `Player*` and require both non-null and `IsInWorld()` **before reading any stage or calling any stage writer**.
- When the leader is unknown/offline, **OFF = allow membership unchanged, with no guessed stage and no progression quest write**; **ON = reject acceptance because expansion eligibility cannot be verified**.
- For a positively present online leader, execute the **unchanged original grouping and tier-synchronisation code**, including the legacy OFF-stage precise synchronization and ON-level rules. This patch intentionally does not choose a different grouping policy.
- Do not change `EnforceGroupRules`, group-invite hooks, Playerbots, `Group::AddMember` bypass, F14 race constraints, source-of-truth or admin commands; these remain independent issues.

**Caution:** Existing [group expansion DRAFT #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3) and combined [IP DRAFT #10](https://github.com/CosmicCuddle/mod-individual-progression/pull/10) refactor this exact callback. If the owner later approves both fixes, **port the semantic null-leader guard onto their current branch** and re-run the combined compile and real group tests rather than blindly merging conflicting source. This standalone branch does not override #3 or #10.

## Verification

- `tests/test_ip_offline_group_leader_no_progression_wipe.py` checks original guard conditions, no leader/stage lookup before disabled-mode escape, non-null/in-world leader required before stage reads, missing-leader OFF/ON matrix for stages0–18, and unmodified existing present-leader synchronization.
- Strict path checks and isolated CI native Worldserver compilation should verify actual C++ syntax against historical Core `8daffdabe9202c228e45d833c9a1d399597a15cd`, Playerbots `e88cc4099e69f35126b28942a412ca702d20453b` (two known **runner-only** compatibility substitutions) and Naxx `80e9de1c39a6dc995188a80c31a5e785d0f2a6f9`. Do not infer owner-installed compatibility from historical pins.
- A real staging test requires **verified restorable character and group DB snapshots**, exact source/config matching, actual online/offline group leadership and no host production mutation. Check offline vs online leader with grouping OFF/ON, normal players vs selected RandomBots/Altbots, BG/raid/group special contexts, the post-add direct Playerbots bypass, and saved `660xx` quest markers before and after. User has not approved an active deployment.

## Rollback and risk

Revert isolated branch to restore source before merge. Any future approved staging binary can be rolled back to a known-good version **only after character DB backed up**, because a previous destructive stage write cannot be undone by reverting code. OFF with missing leader now **skips historical synchronization** rather than force-resets to 0; that is a deliberate, narrowly justified data-safety exception and should be checked against desired grouping semantics.

**This prevents one exact missing-leader write only.** It does not resolve Playerbots `ResetQuests`, known online-leader downgrades, `.ip setbot`, F14 positive RandomBot identity, stage8 floor or visibility. These remain blockers for a complete F14 release.
