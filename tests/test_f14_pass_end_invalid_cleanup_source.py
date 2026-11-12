"""IP166 fixes invalid End identity slot retention without granting F14 authority."""
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARENT = "246b638161d10be74e2a9b9046c04c68850b4bdf"

def require(ok, message):
    if not ok:
        raise SystemExit(message)

expected = sorted([
    ".github/workflows/f14-current-pass-identity.yml",
    ".github/workflows/f14-pass-end-invalid-cleanup.yml",
    "docs/CUSTOM_MODIFICATIONS_REGISTER.md",
    "src/IndividualProgressionF14CurrentPassIdentity.h",
    "tests/f14_current_pass_identity_test.cpp",
    "tests/test_f14_pass_end_invalid_cleanup_source.py",
])
actual = subprocess.check_output(["git","diff","--name-only",PARENT,"HEAD"], cwd=ROOT, text=True).splitlines()
require(sorted(actual) == expected, "unexpected IP166 changed-path scope")

header = (ROOT / "src/IndividualProgressionF14CurrentPassIdentity.h").read_text()
end = header.split("Result End(std::uint64_t token, ViewerIdentity identity)",1)[1].split("std::size_t RevokeViewer",1)[0]
require("if (!token)" in end, "End must still refuse a zero token")
require("Slot* slot = FindToken(token);" in end, "End must resolve the exact token slot before identity mismatch handling")
require("if (!identity || !(slot->identity == identity))" in end, "invalid or changed identity must share mismatch handling")
require("*slot = {};" in end and "return MismatchLocked();" in end, "mismatch must clear the slot before sticky fail-visible poison")
require("if (!token || !identity)" not in end, "invalid identity must not return before slot cleanup")

for forbidden in (
    ".Open(", ".Reserve(", ".Commit(",
    "ShouldF14Suppress", "OnF14ShouldSuppress",
    "SendDirectMessage(", "UpdateObjectVisibility(", "DestroyForPlayer(",
    "RecordConfirmedF14Suppression",
):
    require(forbidden not in header, "pass tracker gained positive authority: " + forbidden)

test = (ROOT / "tests/f14_current_pass_identity_test.cpp").read_text()
for needle in (
    "CHECK(t.Begin(750,a) == P::Result::Recorded);",
    "CHECK(t.End(750,{}) == P::Result::Mismatch);",
    "CHECK(s.poisoned && s.active == 0 && s.mismatches == 1);",
    "CHECK(t.Begin(751,a) == P::Result::Poisoned);",
    "CHECK(t.Inspect().active == 0);",
):
    require(needle in test, "missing invalid-End regression: " + needle)

historical = (ROOT / ".github/workflows/f14-current-pass-identity.yml").read_text()
require("013a2e0e63e41bbd7b21ddf31c6fa4f6fc8ea554" in historical, "historical IP159 guard must pin immutable reviewed head")
require("git worktree add --detach" in historical, "historical guard must run in detached immutable worktree")

old_register = subprocess.check_output(["git","show",PARENT + ":docs/CUSTOM_MODIFICATIONS_REGISTER.md"], cwd=ROOT)
new_register = (ROOT / "docs/CUSTOM_MODIFICATIONS_REGISTER.md").read_bytes()
require(new_register.startswith(old_register), "custom modifications register must preserve IP165 bytes as exact prefix")

print("PASS IP166 invalid End identity clears the exact slot before sticky mismatch poison and grants no positive authority")
