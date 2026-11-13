# F14 v6 social source check — offline mail and guild hook

Owner CosmicCuddle. Source-only peer review under [coordination Issue #51](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51) and [Claude's social Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13). F14/Paid remain OFF. This does not claim an installed source tuple, composed checkout, native Worldserver build, gameplay, merge or deployment.

## Exact pins and overlap

Claude's delivered social commit `a6d3ef078f7d2fe9b283d8dfcd80a55a3ac0a9c8` descends from IP172 `aed05a92f262362608f5d643c8570b504189aae0`. The [v6 offline manifest](F14_REGISTER_RECONCILIATION_MANIFEST_V6_2026-11-13.json) pins that commit and its exact 14-path diff, including the new offline-mail policy and guild-invite Core patch script/test. It is separate from the historical v5 snapshot.

Against pinned #174–#178, the only common changed path is `docs/CUSTOM_MODIFICATIONS_REGISTER.md`; its complete tree entries differ. The social Register adds one stable `IP-F14-SOCIAL-ISOLATION-20261113` row, preserves earlier sections, has no detected field gaps, and introduces no duplicate feature ID among these candidates. The three social config additions retain `RandomBotSocialIsolation = 0`, the quoted reply default and cooldown 60. `node --test tests/test_f14_composition_preflight.js tests/test_f14_register_reconcile.js` passed **13/13** locally; `git diff --check` passed. These are exact local Git-object and Node checks, not an integrated executable-tree validation. Moving branch refs require a fresh pin check.

## Logout identity capture ordering

At candidate [Core #25 `240f6250`](https://github.com/CosmicCuddle/azerothcore-wotlk/commit/240f625033e6388ad73ba61282b3f62340301a02), `ScriptMgr::OnPlayerLogout` iterates `ScriptRegistry<PlayerScript>::EnabledHooks[PLAYERHOOK_ON_LOGOUT]`; the registry appends code-only scripts to its hook vector in construction order. At the pinned social commit, `AddSC_mod_individual_progression()` registers the social scripts before the later `AddSC_mod_individual_progression_player()` call in `Addmod_individual_progressionScripts()`. The earlier F14 lifecycle logout callback revokes its viewer stamp, while IP's player callback later calls `ForgetF14VerifiedRandomBotGroupIdentity`. This supports the source-order premise for capturing the positively verified bot GUID at social `OnPlayerLogout` before the IP cache is cleared. It does not establish every logout path, lifetime, or installed binary behavior.

## Remaining integration boundaries

The owner-approved offline-mail set remembers positively confirmed bot GUIDs for the server run. Its monotonic set is consistent with that stated policy; identity revocation/GUID reuse semantics still belong in adversarial review, not an inferred policy change here. The guild hook is a module-side consumer plus a Core patch script; no Core repository change or linked binary is established by this source check. The known queued `CMSG_GUILD_INVITE` route is distinct from the [bot-initiated group-invite worker-thread blocker](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6865093100), which remains CHANGES REQUIRED. Claude/CS01 own those overlapping implementation decisions.
