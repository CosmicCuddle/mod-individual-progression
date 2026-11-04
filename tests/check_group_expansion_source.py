#!/usr/bin/env python3
"""Source-wiring guards for the expansion-only grouping candidate.

This is not a Worldserver build or a substitute for gameplay testing.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
player = (root / "src/IndividualProgressionPlayer.cpp").read_text()
core = (root / "src/IndividualProgression.cpp").read_text()
policy = (root / "src/IndividualProgressionGroupPolicy.h").read_text()

accept = player.split("    bool OnPlayerCanGroupAccept(", 1)[1].split(
    "    void OnPlayerCreatureKill(", 1)[0]
enforced_accept = accept.split("if (sIndividualProgression->enforceGroupRules)", 1)[1].split(
    "// Preserve the existing OFF pathway", 1)[0]

invite = player.split("    bool OnPlayerCanGroupInvite(", 1)[1].split(
    "    bool OnPlayerCanGroupAccept(", 1)[0]
off_accept = accept.split("// Preserve the existing OFF pathway", 1)[1]

checks = {
    "player script uses shared policy": '#include "IndividualProgressionGroupPolicy.h"' in player,
    "IP grouping helper uses shared policy": '#include "IndividualProgressionGroupPolicy.h"' in core,
    "policy rejects above-cap recorded tiers": "firstStage > progressionLimit || secondStage > progressionLimit" in policy,
    "policy rejects invalid raw stages": "firstStage > LastSupportedStage || secondStage > LastSupportedStage" in policy,
    "Vanilla TBC race restriction": "first == Expansion::Vanilla && (IsExpansionRace(firstRace) || IsExpansionRace(secondRace))" in policy,
    "on invite enforces same expansion": 'bool OnPlayerCanGroupInvite(' in player and player.count('IndividualProgressionGroupPolicy::MayGroup(') >= 3,
    "on accept defers bot stage mutation until true membership": (
        "ForceUpdateProgressionState(" not in enforced_accept and
        "void OnAddMember(Group* group, ObjectGuid guid) override" in player and
        "sIndividualProgression->SyncBotsProgressionToLeader(group);" in player
    ),
    "disabled IP prevents group-time quest mutations": "if (!enabled || !group)" in core,
    "both invite/accept inspect persisted member slots": player.count('group->GetMemberSlots()') == 2 and player.count('ObjectAccessor::FindConnectedPlayer(slot.guid)') == 2,
    "offline member tiers fail closed under enforcement": player.count('if (!member || !member->IsInWorld())\n                    return false;') == 2,
    "leader sync guards expansion": 'if (!IndividualProgressionGroupPolicy::MayGroup(' in core and 'if (enforceGroupRules)' in core,
    "tier-0 bot sync only when enforcement ON": 'if (!refProgress && !enforceGroupRules)' in core,
    "skip redundant bot quest resets when enforced": 'if (memberStage == permittedState)' in core,
    "stage/race drift guards": 'static_assert(RACE_BLOODELF ==' in player and 'static_assert(PROGRESSION_PRE_TBC ==' in player,
    "no old exact tier comparison": 'currentState == otherPlayerState' not in player and 'return (currentState == groupLeaderState)' not in player,
    "OFF invite remains normal stock/group semantics": (
        'if (!sIndividualProgression->enabled || !sIndividualProgression->enforceGroupRules)' in invite and
        'return true;' in invite
    ),
    "OFF accept still synchronizes excluded and bot accounts": (
        'if (!sIndividualProgression->isNormalAccount(player) && joiningState != leaderState)' in off_accept and
        'sIndividualProgression->ForceUpdateProgressionState(' in off_accept
    ),
    "OFF group-add sync still skips tier zero": 'if (!refProgress && !enforceGroupRules)' in core,
    "ON invited bot does not bypass group policy": (
        'isBotAccount(player)' not in invite and 'IndividualProgressionGroupPolicy::MayGroup(' in invite
    ),
    "rndbot floor is NOT falsely wired before classifier review":
        'IsAllowedRandomBotStage(' not in player and 'IsAllowedRandomBotStage(' not in core,
}
for label, passed in checks.items():
    print(f"{'PASS' if passed else 'FAIL'}: {label}")
if not all(checks.values()):
    raise SystemExit(1)
print(f"PASS: {len(checks)} source-wiring checks; only standalone policy compiled")
