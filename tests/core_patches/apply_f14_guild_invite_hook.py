#!/usr/bin/env python3
"""Owner-approved Core25 change: a boolean "can guild invite" player hook.

Core25 has no script check before a guild invitation is sent (only
notifications after a member has joined). This adds one, following
Core25's own PlayerScript conventions:

  * PlayerScript.h: PLAYERHOOK_ON_F14_CAN_GUILD_INVITE appended just
    before PLAYERHOOK_END (no existing hook number changes), the virtual
    OnPlayerCanGuildInvite(inviter, invitee) defaulting to TRUE, and
    COSMIC_F14_GUILD_INVITE_HOOK_VERSION so modules can compile against
    both patched and unpatched Core.
  * ScriptMgr.h / PlayerScript.cpp: the dispatcher (any script answering
    FALSE refuses), exactly like OnPlayerCanGroupInvite.
  * Guild.cpp: Guild::HandleInviteMember consults it after all of Core's
    own checks and before the invitation is sent. A refusal returns
    silently, like Core's existing "ignored player" case.

No behaviour changes unless a script answers FALSE. Every anchor is
validated before anything is written; a repeat application is refused.
Usage: apply_f14_guild_invite_hook.py <Core25 checkout>
"""
from pathlib import Path
import sys

MACRO = "COSMIC_F14_GUILD_INVITE_HOOK_VERSION"

PLAYER_SCRIPT_H = "src/server/game/Scripting/ScriptDefines/PlayerScript.h"
SCRIPT_MGR_H = "src/server/game/Scripting/ScriptMgr.h"
PLAYER_SCRIPT_CPP = "src/server/game/Scripting/ScriptDefines/PlayerScript.cpp"
GUILD_CPP = "src/server/game/Guilds/Guild.cpp"

ENUM_OLD = "    PLAYERHOOK_ON_AFTER_TRAIN_SPELL,\n    PLAYERHOOK_END\n};"
ENUM_NEW = ("    PLAYERHOOK_ON_AFTER_TRAIN_SPELL,\n"
            "    PLAYERHOOK_ON_F14_CAN_GUILD_INVITE,\n"
            "    PLAYERHOOK_END\n};")

VIRTUAL_ANCHOR = ("    [[nodiscard]] virtual bool OnPlayerCanGroupInvite(Player* /*player*/, "
                  "std::string& /*membername*/) { return true; }\n")
VIRTUAL_NEW = VIRTUAL_ANCHOR + (
    "\n"
    "    // Cosmic F14: may `inviter` invite `invitee` to a guild? Called after\n"
    "    // Core's own checks and before the invitation is sent. Defaults TRUE.\n"
    "    [[nodiscard]] virtual bool OnPlayerCanGuildInvite(Player* /*inviter*/, "
    "Player* /*invitee*/) { return true; }\n")

MACRO_ANCHOR = "enum PlayerHook\n"
MACRO_NEW = ("// Cosmic F14 owner-approved guild-invite hook (version 1).\n"
             f"#define {MACRO} 1\n\n" + MACRO_ANCHOR)

MGR_ANCHOR = "    bool OnPlayerCanGroupInvite(Player* player, std::string& membername);\n"
MGR_NEW = MGR_ANCHOR + "    bool OnPlayerCanGuildInvite(Player* inviter, Player* invitee);\n"

IMPL_ANCHOR = ("bool ScriptMgr::OnPlayerCanGroupInvite(Player* player, std::string& membername)\n"
               "{\n"
               "    CALL_ENABLED_BOOLEAN_HOOKS(PlayerScript, PLAYERHOOK_CAN_GROUP_INVITE, "
               "!script->OnPlayerCanGroupInvite(player, membername));\n"
               "}\n")
IMPL_NEW = IMPL_ANCHOR + (
    "\n"
    "bool ScriptMgr::OnPlayerCanGuildInvite(Player* inviter, Player* invitee)\n"
    "{\n"
    "    CALL_ENABLED_BOOLEAN_HOOKS(PlayerScript, PLAYERHOOK_ON_F14_CAN_GUILD_INVITE, "
    "!script->OnPlayerCanGuildInvite(inviter, invitee));\n"
    "}\n")

CALL_ANCHOR = ("    // Inviting player must have rights to invite\n"
               "    if (!_HasRankRight(player, GR_RIGHT_INVITE))\n"
               "    {\n"
               "        SendCommandResult(session, GUILD_COMMAND_INVITE, ERR_GUILD_PERMISSIONS);\n"
               "        return;\n"
               "    }\n")
CALL_NEW = CALL_ANCHOR + (
    "\n"
    "    // Cosmic F14: a script may refuse this invitation (silently, like an\n"
    "    // ignored player); it is consulted only after every Core check passed.\n"
    "    if (!sScriptMgr->OnPlayerCanGuildInvite(player, pInvitee))\n"
    "        return;\n")
CALL_FOLLOW = "    SendCommandResult(session, GUILD_COMMAND_INVITE, ERR_GUILD_COMMAND_SUCCESS, name);"


def _replace_once(text: str, old: str, new: str, what: str) -> str:
    if text.count(old) != 1:
        raise ValueError(f"{what}: expected exactly one anchor, found {text.count(old)}")
    return text.replace(old, new, 1)


def transform_player_script_h(s: str) -> str:
    if MACRO in s or "OnPlayerCanGuildInvite" in s:
        raise ValueError("Refusing repeat application (PlayerScript.h)")
    s = _replace_once(s, ENUM_OLD, ENUM_NEW, "PlayerHook enum tail")
    s = _replace_once(s, VIRTUAL_ANCHOR, VIRTUAL_NEW, "OnPlayerCanGroupInvite virtual")
    return _replace_once(s, MACRO_ANCHOR, MACRO_NEW, "enum PlayerHook")


def transform_script_mgr_h(s: str) -> str:
    if "OnPlayerCanGuildInvite" in s:
        raise ValueError("Refusing repeat application (ScriptMgr.h)")
    return _replace_once(s, MGR_ANCHOR, MGR_NEW, "ScriptMgr declaration")


def transform_player_script_cpp(s: str) -> str:
    if "OnPlayerCanGuildInvite" in s:
        raise ValueError("Refusing repeat application (PlayerScript.cpp)")
    return _replace_once(s, IMPL_ANCHOR, IMPL_NEW, "ScriptMgr dispatcher")


def transform_guild_cpp(s: str) -> str:
    if "OnPlayerCanGuildInvite" in s:
        raise ValueError("Refusing repeat application (Guild.cpp)")
    body = s.split("void Guild::HandleInviteMember(WorldSession* session, std::string const& name)", 1)
    if len(body) != 2:
        raise ValueError("Guild::HandleInviteMember not found")
    head, rest = body
    function, tail = rest.split("\nvoid Guild::HandleAcceptMember(", 1)
    if function.count(CALL_FOLLOW) != 1:
        raise ValueError("Unexpected invite success path")
    function = _replace_once(function, CALL_ANCHOR, CALL_NEW, "rank-right check")
    if function.index("OnPlayerCanGuildInvite") > function.index(CALL_FOLLOW):
        raise ValueError("Hook must run before the invitation is sent")
    return head + "void Guild::HandleInviteMember(WorldSession* session, std::string const& name)" + \
        function + "\nvoid Guild::HandleAcceptMember(" + tail


TRANSFORMS = (
    (PLAYER_SCRIPT_H, transform_player_script_h),
    (SCRIPT_MGR_H, transform_script_mgr_h),
    (PLAYER_SCRIPT_CPP, transform_player_script_cpp),
    (GUILD_CPP, transform_guild_cpp),
)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: apply_f14_guild_invite_hook.py <Core25 checkout>")
    root = Path(sys.argv[1]).resolve()
    if not root.is_dir():
        raise SystemExit("missing Core checkout")
    # Validate and build every change first; write only if all succeed.
    changed = {}
    for name, transform in TRANSFORMS:
        source = (root / name).read_text(encoding="utf-8")
        result = transform(source)
        if result == source:
            raise ValueError(f"{name}: no change made")
        changed[name] = result
    for name, text in changed.items():
        (root / name).write_text(text, encoding="utf-8")
    print(f"Applied {MACRO} 1 to {len(changed)} Core files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
