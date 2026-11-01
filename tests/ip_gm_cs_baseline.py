"""Reviewed-baseline check for src/cs_individualProgression.cpp.

The GM command file was reviewed at blob 654119d (#14 / #20). The GM-command
DRAFT adds only the lines listed in DOCUMENTED_EDITS. matches_reviewed_blob()
reverts exactly those lines and checks the result is byte-identical to the
reviewed blob, so any other change still fails.

Usage as a script: python3 tests/ip_gm_cs_baseline.py [path]  (exit 0 = match)
"""
import hashlib
import sys
from pathlib import Path

REVIEWED_BLOB = "654119d29e0519600d50b179ccd5a9c5df8695b0"

# (text in the new file, text in the reviewed file)
DOCUMENTED_EDITS = (
    ('#include "IndividualProgression.h"\n#include "IndividualProgressionGmTools.h"\n',
     '#include "IndividualProgression.h"\n'),
    ('{ "get",    HandleGetIndividualProgressionCommand,    SEC_PLAYER,        Console::Yes },',
     '{ "get",    HandleGetIndividualProgressionCommand,    SEC_GAMEMASTER,    Console::Yes },'),
    ("        // Ordinary players may read only their own progression.\n"
     "        if (!IndividualProgressionGmTools::ResolveGetTarget(handler, player))\n"
     "            return false;\n\n", ""),
    ("        IndividualProgressionGmTools::SendAccessStage(handler, target);\n", ""),
    ("        IndividualProgressionGmTools::SetAudit const paidAudit = IndividualProgressionGmTools::BeginSetAudit(target);\n", ""),
    ("        IndividualProgressionGmTools::FinishSetAudit(handler, target, paidAudit, currentState, progressionLevel);\n", ""),
)


def revert_documented_edits(text: str) -> str:
    for new_text, old_text in DOCUMENTED_EDITS:
        text = text.replace(new_text, old_text)
    return text


def git_blob_sha(data: bytes) -> str:
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def matches_reviewed_blob(text: str) -> bool:
    return git_blob_sha(revert_documented_edits(text).encode()) == REVIEWED_BLOB


if __name__ == "__main__":
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "src/cs_individualProgression.cpp"
    ok = matches_reviewed_blob(path.read_text())
    print("PASS: cs_individualProgression.cpp = reviewed blob + documented GM edits only" if ok
          else "FAIL: cs_individualProgression.cpp differs from the reviewed blob beyond the documented GM edits")
    sys.exit(0 if ok else 1)
