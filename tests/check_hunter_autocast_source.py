#!/usr/bin/env python3
"""Read-only source regression checks for the DRAFT Hunter autocast change.

Run from the IP checkout: python3 tests/check_hunter_autocast_source.py

This uses the local develop/cosmic-ip Git ref as a preservation baseline.
It does not compile C++, start a server, read a database or access the network.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

SOURCE = "src/IndividualProgressionSpells.cpp"
BASE_REF = "develop/cosmic-ip"
EXPECTED_RECONCILE_CALLS = 111


def hunter_family_region(source: str) -> str:
    marker = '    if (familyName == "Wolf")'
    start = source.find(marker)
    if start == -1:
        raise ValueError("Hunter family mapping start not found")
    end = source.find("\nvoid IndividualProgression::", start + len(marker))
    if end == -1:
        raise ValueError("Hunter family mapping end not found")
    return source[start:end]


def baseline_source(repo_root: Path) -> str:
    # Branch may be checked out locally or exist only as a remote-tracking ref.
    # No fetch is performed; absence fails explicitly.
    last_error = ""
    for baseline_ref in (BASE_REF, f"origin/{BASE_REF}"):
        process = subprocess.run(
            ["git", "show", f"{baseline_ref}:{SOURCE}"],
            cwd=repo_root,
            text=True,
            encoding="utf-8",
            capture_output=True,
            check=False,
        )
        if process.returncode == 0:
            return process.stdout
        last_error = process.stderr.strip()[:250]
    raise RuntimeError(
        "Neither local develop/cosmic-ip nor origin/develop/cosmic-ip "
        "is available. Use an existing complete Git checkout. "
        "This checker will NOT fetch from GitHub. "
        f"Git error: {last_error}"
    )


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    candidate = (root / SOURCE).read_text(encoding="utf-8")
    reference = baseline_source(root)
    errors: list[str] = []

    def require(name: str, result: bool) -> None:
        if not result:
            errors.append(name)
        print(f"{'PASS' if result else 'FAIL'}  {name}")

    try:
        old_rules = hunter_family_region(reference)
        new_rules = hunter_family_region(candidate)
        require("Original Hunter family/rank mappings unchanged", old_rules == new_rules)
        calls = len(re.findall(r"\breconcile\s*\(", new_rules))
        require(f"All {EXPECTED_RECONCILE_CALLS} reconcile entries retained",
                calls == EXPECTED_RECONCILE_CALLS)
        require("Cower rules remain commented",
                "// reconcile({ 1742" in old_rules and
                "// reconcile({ 1742" in new_rules)
    except ValueError as error:
        errors.append(str(error))
        print(f"FAIL  {error}")

    require("New state transfer verifies same ability chain",
            "keepInfo->IsDifferentRankOf(oldInfo)" in candidate)
    require("Only unambiguous predecessor can transfer",
            "existingRankCount == 1" in candidate and
            "!ambiguousChain" in candidate)
    require("Native ON and OFF both considered",
            "oldItr->second.active == ACT_ENABLED" in candidate and
            "oldItr->second.active == ACT_DISABLED" in candidate)
    require("Old autocast list cleaned before removal",
            "pet->ToggleAutocast(oldInfo, false)" in candidate)
    require("New spell and action bar kept in sync",
            "pet->ToggleAutocast(keepInfo, enabled)" in candidate and
            "charm->SetSpellAutocast(keepInfo, enabled)" in candidate)
    require("Queued event checks originating Hunter pet GUID",
            "pet->GetGUID() != _petGuid" in candidate)
    require("Bounded retry maintained",
            "pet->isBeingLoaded() && _attempts < 10" in candidate)
    require("Hunter recovery callback registered",
            "new IndividualPlayerProgression_HunterPetLoadRecoveryScript();" in candidate and
            "void OnPlayerAfterUpdate(Player* player, uint32 /*diff*/) override" in candidate)
    require("Deferred work uses a per-pet flag",
            "pending->afterLoad = true;" in candidate and
            "pending->afterLoad = false;" in candidate)
    require("Per-update check does not allocate pet records",
            "pet->CustomData.Get<PendingPetSpellCheck>(PENDING_CHECK_KEY)" in candidate)
    require("Warlock's original reconciliation call retained",
            "sIndividualProgression->checkWarlockPetSpells(player);" in candidate)
    require("No added schema mutation statements versus the protected base",
            not any(token in candidate and token not in reference
                    for token in ("CREATE TABLE", "ALTER TABLE", "DROP TABLE",
                                  "DELETE FROM pet_spell", "TRUNCATE TABLE")))

    if errors:
        print("\nSTATIC SOURCE CHECK FAILED. These checks do not compile C++.")
        return 1
    print("\nSTATIC SOURCE CHECK PASSED. Compilation and gameplay are NOT tested.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(2)
