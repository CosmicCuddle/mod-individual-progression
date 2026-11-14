"""Owner-approved Core guild-invite hook patch: deterministic, additive,
all-or-nothing and refusing repeat application. Uses minimal fixtures that
carry the exact Core25 anchors, so it runs without a Core checkout; the
workflow additionally applies it to the pinned Core25 tree."""
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("guild_patch", ROOT / "tests/core_patches/apply_f14_guild_invite_hook.py")
patch = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(patch)

PLAYER_H = ("#ifndef SCRIPT_OBJECT_PLAYER_SCRIPT_H_\n\nenum PlayerHook\n{\n    PLAYERHOOK_ON_AFTER_TRAIN_SPELL,\n"
            "    PLAYERHOOK_END\n};\n\nclass PlayerScript\n{\n"
            "    [[nodiscard]] virtual bool OnPlayerCanGroupInvite(Player* /*player*/, std::string& /*membername*/) { return true; }\n};\n")
MGR_H = "class ScriptMgr\n{\n    bool OnPlayerCanGroupInvite(Player* player, std::string& membername);\n};\n"
PLAYER_CPP = ("bool ScriptMgr::OnPlayerCanGroupInvite(Player* player, std::string& membername)\n{\n"
              "    CALL_ENABLED_BOOLEAN_HOOKS(PlayerScript, PLAYERHOOK_CAN_GROUP_INVITE, !script->OnPlayerCanGroupInvite(player, membername));\n}\n")
GUILD_CPP = ("void Guild::HandleInviteMember(WorldSession* session, std::string const& name)\n{\n"
             "    // Inviting player must have rights to invite\n    if (!_HasRankRight(player, GR_RIGHT_INVITE))\n    {\n"
             "        SendCommandResult(session, GUILD_COMMAND_INVITE, ERR_GUILD_PERMISSIONS);\n        return;\n    }\n\n"
             "    SendCommandResult(session, GUILD_COMMAND_INVITE, ERR_GUILD_COMMAND_SUCCESS, name);\n}\n\n"
             "void Guild::HandleAcceptMember(WorldSession* session)\n{\n}\n")
FIXTURES = {patch.PLAYER_SCRIPT_H: PLAYER_H, patch.SCRIPT_MGR_H: MGR_H,
            patch.PLAYER_SCRIPT_CPP: PLAYER_CPP, patch.GUILD_CPP: GUILD_CPP}


class GuildInviteHookPatch(unittest.TestCase):
    def tree(self, files=FIXTURES):
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        for name, text in files.items():
            p = Path(d.name) / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8")
        return Path(d.name)

    def run_patch(self, root):
        import sys
        argv = sys.argv
        sys.argv = ["apply", str(root)]
        try:
            return patch.main()
        finally:
            sys.argv = argv

    def test_applies_additively(self):
        root = self.tree()
        self.assertEqual(self.run_patch(root), 0)
        for name, old in FIXTURES.items():
            new = (root / name).read_text(encoding="utf-8")
            # Purely additive: every original line survives, in order.
            it = iter(new.splitlines())
            self.assertTrue(all(line in it for line in old.splitlines()), name)
        h = (root / patch.PLAYER_SCRIPT_H).read_text()
        self.assertIn("#define COSMIC_F14_GUILD_INVITE_HOOK_VERSION 1", h)
        self.assertIn("    PLAYERHOOK_ON_F14_CAN_GUILD_INVITE,\n    PLAYERHOOK_END", h)  # appended: no renumbering
        self.assertIn("virtual bool OnPlayerCanGuildInvite(Player* /*inviter*/, Player* /*invitee*/) { return true; }", h)
        g = (root / patch.GUILD_CPP).read_text()
        self.assertLess(g.index("ERR_GUILD_PERMISSIONS"), g.index("OnPlayerCanGuildInvite"))
        self.assertLess(g.index("OnPlayerCanGuildInvite"), g.index("ERR_GUILD_COMMAND_SUCCESS"))
        self.assertRegex(g, r"if \(!sScriptMgr->OnPlayerCanGuildInvite\(player, pInvitee\)\)\n        return;")
        cpp = (root / patch.PLAYER_SCRIPT_CPP).read_text()
        self.assertIn("CALL_ENABLED_BOOLEAN_HOOKS(PlayerScript, PLAYERHOOK_ON_F14_CAN_GUILD_INVITE, "
                      "!script->OnPlayerCanGuildInvite(inviter, invitee));", cpp)

    def test_refuses_repeat(self):
        root = self.tree()
        self.run_patch(root)
        snapshot = {n: (root / n).read_text() for n in FIXTURES}
        with self.assertRaises(ValueError):
            self.run_patch(root)
        self.assertEqual(snapshot, {n: (root / n).read_text() for n in FIXTURES})

    def test_all_or_nothing_on_a_missing_anchor(self):
        broken = dict(FIXTURES)
        broken[patch.GUILD_CPP] = GUILD_CPP.replace("GR_RIGHT_INVITE", "GR_RIGHT_SOMETHING_ELSE")
        root = self.tree(broken)
        with self.assertRaises(ValueError):
            self.run_patch(root)
        for name, text in broken.items():
            self.assertEqual((root / name).read_text(), text, name)  # nothing written


if __name__ == "__main__":
    unittest.main()
