#!/usr/bin/env python3
"""CI-only exact-seam Core C++ transformation AFTER F14 #56/#67/#76/#77.

This is not a deployment patch. It introduces a script callback defaulting FALSE
and a separate source-tagged decision observation in Player::UpdateVisibilityOf.
NO existing script returns TRUE; NO runtime is permitted to conceal anything.
A positive future callback would require a rights-complete owner-approved policy.
"""
from pathlib import Path
import sys

FILES = {
    "src/server/game/Scripting/ScriptMgr.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h",
    "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp",
    "src/server/game/Entities/Player/PlayerUpdates.cpp",
}

def one(s: str, old: str, new: str, where: str) -> str:
    count = s.count(old)
    if count != 1:
        raise ValueError(f"{where}: expected ONE exact anchor; found {count}: {old[:95]!r}")
    return s.replace(old, new, 1)

def function(s: str, start: str, end: str, apply, where: str) -> str:
    before, token, tail = s.partition(start)
    if not token:
        raise ValueError(f"{where}: missing function start")
    body, token_end, after = tail.partition(end)
    if not token_end:
        raise ValueError(f"{where}: missing function end")
    return before + token + apply(body) + token_end + after

def mgr(s: str) -> str:
    old = "    void OnF14ViewerTargetLinkChanged(Player const* observer, WorldObject const* target, bool nowLinked, bool batched);"
    return one(s, old, old + """
    // ONLY Player visible-object update calls this; positive callers MUST
    // independently prove all human combat/tap/quest/loot/assist rights.
    // No registered IP script returns true. An ordinary Core deny is NOT F14.
    [[nodiscard]] bool ShouldF14SuppressCreatureForViewer(Player const* viewer, WorldObject const* creature);
    void OnF14CreatureSuppressionDecision(Player const* viewer, WorldObject const* creature,
                                         bool wasLinked, bool batched);""", "ScriptMgr.h")

def world_h(s: str) -> str:
    s = one(s, "    WORLDOBJECTHOOK_ON_F14_VIEWER_TARGET_LINK_CHANGED,",
            "    WORLDOBJECTHOOK_ON_F14_VIEWER_TARGET_LINK_CHANGED,\n"
            "    WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_QUERY,\n"
            "    WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED,", "WorldObjectScript.h")
    old = """    virtual void OnF14ViewerTargetLinkChanged(Player const* /*observer*/, WorldObject const* /*target*/,
                                             bool /*nowLinked*/, bool /*batched*/) { }"""
    return one(s, old, old + """

    // Default FALSE, no positive F14 hide provider installed. The eventual
    // implementation cannot reuse ordinary Core link-loss as F14 proof.
    [[nodiscard]] virtual bool OnF14ShouldSuppressCreatureForViewer(
        Player const* /*viewer*/, WorldObject const* /*creature*/) { return false; }
    // Source-attributed DECISION only: not packet send/receive or client ACK.
    virtual void OnF14CreatureSuppressionDecision(Player const* /*viewer*/,
        WorldObject const* /*creature*/, bool /*wasLinked*/, bool /*batched*/) { }""",
        "WorldObjectScript.h")

def world_cpp(s: str) -> str:
    anchor = "WorldObjectScript::WorldObjectScript(char const* name, std::vector<uint16> enabledHooks)"
    addition = """bool ScriptMgr::ShouldF14SuppressCreatureForViewer(Player const* viewer, WorldObject const* creature)
{
    ASSERT(viewer && creature);
    // No positive listener by default: every other Core visibility decision
    // remains exactly as before, with human and Altbot creatures unaffected.
    CALL_ENABLED_BOOLEAN_HOOKS_WITH_DEFAULT_FALSE(WorldObjectScript,
        WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_QUERY,
        script->OnF14ShouldSuppressCreatureForViewer(viewer, creature));
}

void ScriptMgr::OnF14CreatureSuppressionDecision(Player const* viewer,
                                                WorldObject const* creature,
                                                bool wasLinked, bool batched)
{
    ASSERT(viewer && creature);
    // A positive F14-specific decision only. Server link / batched queue
    // cannot establish a 3.3.5a client packet acknowledgement.
    CALL_ENABLED_HOOKS(WorldObjectScript,
        WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED,
        script->OnF14CreatureSuppressionDecision(viewer, creature, wasLinked, batched));
}

"""
    return one(s, anchor, addition + anchor, "WorldObjectScript.cpp")

def visibility_block(s: str, batched: bool) -> str:
    lookup_anchor = ("    GetMap()->AddObjectToPendingUpdateList(target);" if batched else
                     "    bool const f14WasLinked = f14Reportable && HaveAtClient(target);")
    query = """
    // Only a baseline-visible ordinary NPC may be denied by an EXPLICIT
    // F14-only policy callback. It defaults FALSE; never changes Core
    // CanSeeOrDetect, combat, GM/self, pet or ordinary phasing decisions.
    bool const f14BaselineVisible = CanSeeOrDetect(target, false, true);
    bool const f14PositiveSuppression = f14BaselineVisible && target->IsCreature() &&
        !target->ToCreature()->IsPet() &&
        sScriptMgr->ShouldF14SuppressCreatureForViewer(this, target);"""
    s = one(s, lookup_anchor, lookup_anchor + query, "PlayerUpdates.cpp")
    s = one(s, "if (!CanSeeOrDetect(target, false, true))",
            "if (!f14BaselineVisible || f14PositiveSuppression)", "PlayerUpdates.cpp")
    s = one(s, "if (CanSeeOrDetect(target, false, true))",
            "if (f14BaselineVisible && !f14PositiveSuppression)", "PlayerUpdates.cpp")
    hook = ("    if (f14Reportable && f14WasLinked != HaveAtClient(target))\n"
            "        sScriptMgr->OnF14ViewerTargetLinkChanged(this, target, HaveAtClient(target), " +
            ("true" if batched else "false") + ");")
    added = """
    // Exact F14-specific suppression attribution, including an initially
    // unlinked NPC that baseline Core WOULD have shown. May run repeatedly;
    // a future ledger must de-duplicate by viewer epoch and incarnation.
    // This callback cannot assert packet delivery or client restoration.
    if (f14PositiveSuppression)
        sScriptMgr->OnF14CreatureSuppressionDecision(this, target, f14WasLinked, """ + (
            "true" if batched else "false") + ");"
    return one(s, hook, hook + added, "PlayerUpdates.cpp")

def player_updates(s: str) -> str:
    template_start = "void Player::UpdateVisibilityOf(T* target, UpdateData& data,"
    template_end = "\nvoid Player::GetInitialVisiblePackets(Unit* target)"
    s = function(s, template_start, template_end, lambda b: visibility_block(b, True), "batched visibility")
    direct_start = "void Player::UpdateVisibilityOf(WorldObject* target)"
    direct_end = "\nvoid Player::UpdateTriggerVisibility()"
    return function(s, direct_start, direct_end, lambda b: visibility_block(b, False),
                    "direct visibility")

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_viewer_suppression_attribution.py <exact Core checkout>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("missing disposable Core checkout")
    sources = {name: (root / name).read_text(encoding="utf-8") for name in sorted(FILES)}
    if "OnF14ViewerTargetLinkChanged" not in sources["src/server/game/Scripting/ScriptMgr.h"]:
        raise SystemExit("missing #76 parent observer; refusing unexpected source")
    if "ShouldF14RefreshCreatureVisibility" not in sources["src/server/game/Scripting/ScriptMgr.h"]:
        raise SystemExit("missing #77 parent default-false refresh; refusing unexpected source")
    edits = {
        "src/server/game/Scripting/ScriptMgr.h": mgr,
        "src/server/game/Scripting/ScriptDefines/WorldObjectScript.h": world_h,
        "src/server/game/Scripting/ScriptDefines/WorldObjectScript.cpp": world_cpp,
        "src/server/game/Entities/Player/PlayerUpdates.cpp": player_updates,
    }
    # All-or-nothing validation BEFORE any file writes.
    outputs = {name: edits[name](src) for name, src in sources.items()}
    for name, output in outputs.items():
        if output == sources[name]:
            raise ValueError(f"{name} unchanged, refusing ambiguous patch")
        if output.count("OnF14CreatureSuppressionDecision") < 0:
            raise ValueError("source validation impossible")
    for name, output in outputs.items():
        (root / name).write_text(output, encoding="utf-8")
    print("PASS: four narrowly source-matched Core files altered in disposable checkout ONLY")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
