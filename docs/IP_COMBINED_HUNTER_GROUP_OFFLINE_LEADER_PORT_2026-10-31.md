# Combined Hunter + expansion grouping: preserve offline-leader no-stage0 safeguard

**Owner:** CosmicCuddle
**Parent review source:** [Combined Hunter + grouping DRAFT #10](https://github.com/CosmicCuddle/mod-individual-progression/pull/10) exact parent SHA `d3688e243a24441438eaff7b3488b05f41614deb`, stacked on expansion grouping [DRAFT #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3).
**Source finding:** Grok/CS02 C3 [Coordination #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11), also fixed independently against original develop by [DRAFT #15](https://github.com/CosmicCuddle/mod-individual-progression/pull/15).
**Status:** **DRAFT only, unmerged, not installed, not game-tested.**

## Motivation

The combined #10 source already prevents unknown/offline group leader eligibility when `EnforceGroupRules=1`, **but it performs `GetPlayerProgressionFromQuests(leader)` before checking whether the leader exists**. The helper returns **stage 0 on nullptr**. With enforcement **OFF**, accepting an invite can therefore call the destructive `ForceUpdateProgressionState(joiner, stage0)` when the group leader is missing. Even though the new grouping rules are OFF, **the joiner's saved progression can be removed**.

The isolated [DRAFT #15](https://github.com/CosmicCuddle/mod-individual-progression/pull/15) resolves this on original `develop/cosmic-ip`, but its source cannot safely overwrite the different callback already present on #10/#3.

## Exact integration port

In **only** `src/IndividualProgressionPlayer.cpp::OnPlayerCanGroupAccept`:

1. Keep pre-existing invalid-player/group rejection and `!IP.enabled -> true`.
2. Resolve leader `Player*`, then **require non-null and `IsInWorld()` BEFORE both player/leader quest-stage reads**.
3. Unknown leader: `EnforceGroupRules=0` -> **allow invitation, perform no progress mutation**; `=1` -> **deny invitation with no progress mutation**.
4. Real online leader: retain #10's original full expansion-band member checks, offline-member fail-closed semantics, ON no-mutation acceptance, OFF legacy exact-tier bot synchronisation.
5. No other executable IP unit, Hunter rank logic, F14 policy, GroupScript, configs, SQL, Playerbots, Naxx or Core changes.

This is **only the missing-leader safeguard**, NOT a newly implemented pre-add veto for Playerbots direct `Group::AddMember`, a change to BG/LFG rules, nor F14's RandomBot stage floor. Online leader and actual group membership/rejoin behaviour require real in-game testing.

## Tests and dependencies

- Standalone `tests/test_ip_combined_offline_leader_guard.py` ensures missing leader is checked before quest reads/writes and that ON/OFF and Hunter/grouping source remain present.
- CI compares source changes to the **exact frozen parent #10 commit**, requires unchanged Hunter `IndividualProgressionSpells.cpp` Git blob `6ee93e1174b5aef6c5d61c9ea31526506c9d08da`, unchanged IP progression engine blob `625817c67f70be65eb3a9462199c0228e378c605`, and unchanged pure group policy blob `3b7cf2c520137aeeb49dd1e6857614d9bdf63fa0`. It runs existing Hunter/group source/pure C++ regressions **without promising a native Worldserver link** for this new integration.
- Parent #10's earlier historical native full Worldserver build was successful at [run 41145398854](https://github.com/CosmicCuddle/mod-individual-progression/actions/runs/41145398854), but **the new change to `IndividualProgressionPlayer.cpp` needs a fresh native compile/link before deployment**. All existing historical pins differ from unverified owner-installed Core/Playerbots/IP/Naxx source.
- Staged acceptance: using restorable test-character and group DB backups, verify unknown/temporarily offline leader before accepting an invitation with group rules OFF and ON, stage8+ joiner retained, no `660xx` quest removal; leader back online; human vs RandomBot/Altbot/excluded; stage0 leader; full same-expansion 0–7/8–12/13–18 and cross-expansion deny tests; Core group direct-add still blocked as separately documented. Include Hunter autocast ON/OFF/two pets/relog because this source is stacked on their integration.

## Release/rollback

**Do not merge this branch or #10/#3/#15 automatically.** It exists as a reviewable semantic port; reconcile the source/CI/Custom Modifications Register in a separately reviewed eventual release branch after owner approval. Reverting a binary cannot restore rewarded quest records already erased; require source/binary/config and restorable character/group DB backups before staged tests. No current owner machine, DB, SQL, config, clients, binary or Worldserver processes were touched.
