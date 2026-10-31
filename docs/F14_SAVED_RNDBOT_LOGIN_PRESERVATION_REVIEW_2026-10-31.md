# F14 partial implementation: preserve existing TBC RandomBot progress at login

**Owner:** CosmicCuddle
**Created:** 31 October 2026
**Status:** DRAFT, OFF-by-default, not approved for merge/deployment or full F14 gameplay.
**Parent source:** [F14 pure C++ policy #11](https://github.com/CosmicCuddle/mod-individual-progression/pull/11), fixed at `d4ff9374a0b89c42d93e0f57fdc3fc720cfec785`. This PR is a sibling, not a replacement for [F14 identity source tests #12](https://github.com/CosmicCuddle/mod-individual-progression/pull/12). Existing Hunter/grouping executable source is preserved.

## Precisely implemented behaviour (only if enabled)

Adds `IndividualProgression.F14.PreserveVerifiedRandomBotStageOnLogin = 0` to the existing IP distributed config (OFF by default) and reads the same key at ordinary IP config initialization. No change to owner's active or effective configuration.

In `IndividualProgressionPlayer.cpp::OnPlayerLogin`, inside the **existing non-normal-account branch**, the code checks four preconditions before the previous level-based `ForceUpdateProgressionState` branch:

1. The new explicit partial safeguard flag is **ON**.
2. The character is **Blood Elf/Draenei** AND has a **headless** `WorldSession`.
3. The character currently has **rewarded progression stage 8–18**, read from in-memory hidden quest status.
4. Source-compatible Playerbots confirms **`sRandomPlayerbotMgr.IsRandomBot(characterGUID)`**, which on pinned historical source requires the selected GUID in `currentBots` and a configured random-bot account-list member.

Only if **all four** conditions pass does this **single login callback** skip the old destructive level-based reset, protecting those *already awarded* stage8–18 quest markers. Other characters follow exactly the previous level-based path; human and Altbot players do not acquire a new stage or visibility change. Saved stage 0–7 is **not** migrated or upgraded. No SQL, world/map filtering, race phasing, WHO, command, group-admission or bot runtime modifications occur.

This is a narrow **no-loss on successful positive login** mitigation. It is **not** the full F14 stage-8 floor or any guarantee that no selected RandomBot appears to a Vanilla observer. It does not solve `.ip set` achievement deletion before stage write, `.ip setbot`, direct Playerbots group addition, post-login group-sync, unknown identity, pre-map visibility, first-time starting stage8 or TBC eligibility. **Do not turn it ON as a substitute for the approved complete F14 design.**

## Cross-module and thread constraints

Pinned historical Playerbots `mod-playerbots/mod-playerbots@e88cc4099e69f35126b28942a412ca702d20453b` records selected `currentBots` GUID before pending headless login, and its `PlayerbotsWorldScript::OnUpdate` runs `PlayerbotHolder::UpdatePendingLogins` on the world thread before its RandomBot update. Core `Grimfeather/azerothcore-wotlk@8daffdabe9202c228e45d833c9a1d399597a15cd` invokes IP `OnPlayerLogin` inside headless login. **This is source reasoning for one callback**, not a proof that a raw `IsRandomBot` read is safe in arbitrary map threads, alternate login paths or the owner-installed version.

Use `#if __has_include("../../mod-playerbots/src/Bot/RandomPlayerbotMgr.h")` to keep the original IP fork compilable without Playerbots; when absent the flag has no protective effect. Even when the header is present, Playerbots itself must be linked at runtime. This optional code is in IP login **only**. Its read-only GUID overload avoids reliance on post-attach PlayerbotAI; it does not run a new synchronous database lookup. Calling the mutable `currentBots` predicate from map/WHO hooks would be unsafe without further ownership/thread testing.

## Verification and deployment gates

- A fast isolated source/negative matrix checks the OFF-path is untouched, the four preconditions, no new stage grants, original human branch, no non-login hooks and config default OFF.
- A pinned **native full Worldserver compile and link** is required for the exact committed implementation with historical Core, Playerbots and Naxx module. The disposable runner applies only two already-documented Playerbots compiler-compatibility substitutions and never launches or deploys Worldserver. A green native build is **not** owner installed gameplay acceptance.
- Before any owner-approved staging switch ON, confirm exact installed Core/Playerbots/IP versions and local modifications, the actual origin/membership of the selected-GUID API, current source-compatible world-thread processing, **restorable** backups of character/world/auth/playerbots DB and config, a disposable staging server, source-matched binary and rollback. Explicit approval required even for staging changes.
- Test an existing race10/11 verified RNDbot saved stage8+ at level <=60 under staging; verify rewarded hidden quests remain and no achievements disappear; validate type0/human/Altbot/Selfbot/Unknown negatives and compare OFF legacy behaviour. Stage0 fresh bot behavior must remain unchanged (not F14-compliant yet), and no real WHO/visibility claims can be made.
- Complete F14 before release: positive identity and TBC entitlement, verified first-spawn stage8 when allowed, handling of existing saved stage0–7 with owner decision, safe every-write boundary including GM achievement effects, group direct-add preflight, and pre-map rndbot-only visibility/interaction. Keep assigned Naxx F14 work coordinated through [Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9).

## Rollback

Reject or revert this unmerged DRAFT branch to restore source. For any later approved staged build, restoring the old binary and previous config flag OFF returns the prior login algorithm; this **does not restore previously deleted quest markers**, so restorable snapshots of the test character and databases are mandatory beforehand. No actual backups, server startup, gameplay observations or production deployment are asserted by this review.

**Strict status: Source code committed in isolated DRAFT only; Owner's live server and accounts remain untouched.**
