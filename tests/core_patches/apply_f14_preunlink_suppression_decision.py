#!/usr/bin/env python3
"""Disposable pinned Core: source-tagged F14 BEFORE visibility mutation hook.

Apply AFTER #86/#93/#95/#99/#100/#101/#102 in a disposable pinned Grim Core.
Does not add a positive hide provider, change CanSeeOrDetect, link behavior,
packets, SQL or production source. The only caller is an already-POSITIVE
(default FALSE) F14-specific decision, before the viewer's link is updated.
Four exact Core files; all-or-nothing verification before writing.
"""
from pathlib import Path
import re
import sys

FILES = (
    "src/server/game/Scripting/ScriptMgr.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
    "src/server/game/Entities/Player/PlayerUpdates.cpp",
)

def one(text, old, new, label):
    n = text.count(old)
    if n != 1:
        raise ValueError(f"{label}: expected one anchor, found {n}")
    return text.replace(old, new, 1)

def edit_manager(text):
    old = """    void OnF14CreatureSuppressionDecision(Player const* viewer, WorldObject const* creature,
                                         bool wasLinked, bool batched);"""
    new = """    // Source-specific F14 positive intent, BEFORE any target visibility link
    // or out-of-range/create update is changed. Not a client delivery ACK.
    void OnF14BeforeCreatureSuppression(Player const* viewer, WorldObject const* creature,
                                       bool wasLinked, bool batched);
""" + old
    return one(text, old, new, "ScriptMgr pre-mutation API")

def edit_world_header(text):
    old = "    WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED,"
    text = one(text, old,
               "    WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_BEFORE_SUPPRESSION,\n" + old,
               "WorldObjectScript hook enum")
    old = """    virtual void OnF14CreatureSuppressionDecision(Player const* /*viewer*/,
        WorldObject const* /*creature*/, bool /*wasLinked*/, bool /*batched*/) { }"""
    new = """    // F14-source-attributed positive intent only, before link/packet mutation.
    virtual void OnF14BeforeCreatureSuppression(Player const* /*viewer*/,
        WorldObject const* /*creature*/, bool /*wasLinked*/, bool /*batched*/) { }
""" + old
    return one(text, old, new, "WorldObjectScript callback")

def edit_world_cpp(text):
    old = "void ScriptMgr::OnF14CreatureSuppressionDecision(Player const* viewer,"
    new = """void ScriptMgr::OnF14BeforeCreatureSuppression(Player const* viewer,
                                               WorldObject const* creature,
                                               bool wasLinked, bool batched)
{
    ASSERT(viewer && creature);
    CALL_ENABLED_HOOKS(WorldObjectScript,
        WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_BEFORE_SUPPRESSION,
        script->OnF14BeforeCreatureSuppression(viewer, creature, wasLinked, batched));
}

""" + old
    return one(text, old, new, "ScriptMgr pre-mutation dispatcher")

def edit_visibility_function(text, start, end, batched):
    prefix, found, remainder = text.partition(start)
    if not found:
        raise ValueError("missing Player::UpdateVisibilityOf start")
    body, found_end, suffix = remainder.partition(end)
    if not found_end:
        raise ValueError("missing Player::UpdateVisibilityOf end")
    # Only ever match the one F14-specific decision expression, not a generic
    # baseline/CanSeeOrDetect branch or packet batching implementation.
    matches = list(re.finditer(
        r"(?m)^    bool const f14PositiveSuppression = [^\n]*\n"
        r"(?:        [^\n]*\n)*", body))
    if len(matches) != 1:
        raise ValueError(f"prelink F14 decision expression expected once, got {len(matches)}")
    m = matches[0]
    expr = m.group()
    if expr.count(";") != 1 or not expr.rstrip().endswith(";"):
        raise ValueError("uncertain F14 decision termination; refuse to patch")
    if body.count("if (HaveAtClient(target))") != 1:
        raise ValueError("viewer link state anchor not unique")
    if m.end() > body.index("if (HaveAtClient(target))"):
        raise ValueError("pre-mutation hook cannot be placed before link state")
    hook = (
        "\n    // F14-only positive intent, synchronous BEFORE viewer link update;\n"
        "    // no F14 positive script exists by default. Never client ACK.\n"
        "    if (f14PositiveSuppression)\n"
        "        sScriptMgr->OnF14BeforeCreatureSuppression(this, target, f14WasLinked, "
        + ("true" if batched else "false") + ");\n"
    )
    body = body[:m.end()] + hook + body[m.end():]
    if body.index("OnF14BeforeCreatureSuppression(this, target") >= body.index(
            "if (HaveAtClient(target))"):
        raise ValueError("pre-unlink ordering invariant violated")
    if body.count("OnF14CreatureSuppressionDecision(this, target") != 1:
        raise ValueError("original F14 post-decision attribution missing")
    return prefix + found + body + found_end + suffix

def edit_player_updates(text):
    text = edit_visibility_function(
        text,
        "void Player::UpdateVisibilityOf(T* target, UpdateData& data,",
        "\nvoid Player::GetInitialVisiblePackets(Unit* target)", True)
    return edit_visibility_function(
        text,
        "void Player::UpdateVisibilityOf(WorldObject* target)",
        "\nvoid Player::UpdateTriggerVisibility()", False)

def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_preunlink_suppression_decision.py <disposable Core root>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("Core root missing")
    previous = {p: (root / p).read_text(encoding="utf-8") for p in FILES}
    if "OnF14CreatureSuppressionDecision" not in previous[FILES[0]]:
        raise ValueError("missing exact prior #86 source-attributed F14 decision")
    if "F14ViewerHasDirectInteractionVeto" not in previous[FILES[3]] or (
            "HaveQuestLootForPlayer" not in previous[FILES[3]]):
        raise ValueError("missing #99/#102 negative human rights overlays; refuse")
    functions = {
        FILES[0]: edit_manager,
        FILES[1]: edit_world_header,
        FILES[2]: edit_world_cpp,
        FILES[3]: edit_player_updates,
    }
    result = {p: functions[p](previous[p]) for p in FILES}  # validate all
    for p in FILES:
        if result[p] == previous[p]:
            raise ValueError(f"no change for {p}; refuse")
    for p in FILES:
        (root / p).write_text(result[p], encoding="utf-8")
    print("PASS: four disposable Core files gained source-tagged pre-link F14 intent callback; no hide enabled")

if __name__ == "__main__":
    main()
