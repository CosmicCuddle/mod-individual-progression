#!/usr/bin/env python3
"""Source-wiring guards for the expansion-only grouping candidate.

This is not a Worldserver build or a substitute for gameplay testing.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
player = (root / "src/IndividualProgressionPlayer.cpp").read_text()
core = (root / "src/IndividualProgression.cpp").read_text()
policy = (root / "src/IndividualProgressionGroupPolicy.h").read_text()

checks = {
    "player script uses shared policy": '#include "IndividualProgressionGroupPolicy.h"' in player,
    "IP grouping helper uses shared policy": '#include "IndividualProgressionGroupPolicy.h"' in core,
    "policy uses effective cap": "EffectiveStage(firstStage, progressionLimit)" in policy,
    "policy rejects invalid raw stages": "firstStage > LastSupportedStage || secondStage > LastSupportedStage" in policy,
    "Vanilla TBC race restriction": "first == Expansion::Vanilla && (IsExpansionRace(firstRace) || IsExpansionRace(secondRace))" in policy,
    "on invite enforces same expansion": 'bool OnPlayerCanGroupInvite(' in player and player.count('IndividualProgressionGroupPolicy::MayGroup(') >= 3,
    "on accept protects bot stage syncing": 'uint8 const permittedStage = IndividualProgressionGroupPolicy::EffectiveStage(leaderState, limit);' in player,
    "leader sync guards expansion": 'if (enforceGroupRules && !IndividualProgressionGroupPolicy::MayGroup(' in core,
    "stage/race drift guards": 'static_assert(RACE_BLOODELF ==' in player and 'static_assert(PROGRESSION_PRE_TBC ==' in player,
    "no old exact tier comparison": 'currentState == otherPlayerState' not in player and 'return (currentState == groupLeaderState)' not in player,
    "rndbot floor is NOT falsely wired before classifier review":
        'IsAllowedRandomBotStage(' not in player and 'IsAllowedRandomBotStage(' not in core,
}
for label, passed in checks.items():
    print(f"{'PASS' if passed else 'FAIL'}: {label}")
if not all(checks.values()):
    raise SystemExit(1)
print(f"PASS: {len(checks)} source-wiring checks; only standalone policy compiled")
