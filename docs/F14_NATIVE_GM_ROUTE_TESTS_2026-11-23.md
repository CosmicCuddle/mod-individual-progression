# F14 native GM routes and nested mutation recovery

For CosmicCuddle's approved F14 temporary group-progression feature. Test-only
child of IP194, paired with Core's test-only child of Core71. Production code,
gameplay rules, default settings and established custom features are unchanged.

The adapter runner now compiles the complete native `Player::SetGameMaster`
and GM on/off command methods from the explicitly supplied Core checkout,
alongside actual Core security/quest writers and observer dispatch. Engine,
group/session lookup, account classification, chat and script registries remain
explicit doubles. Native GM side effects use isolated fixture methods.

New executable checks exercise publication and owned-update callbacks inside a
native GM transition, rejection while the leader is GM, restoration after GM
off, and both nesting directions between actual rewarded-quest writes and GM
commands. Completing the inner pair must leave the outer fence intact. Recovery
uses the current leader's earned stage; the bot's earned stage/write count is
preserved. Existing session/quest/role capability combinations remain covered.

Local Clang 20.1.2 through Zig 0.15.2 passed the final C++20 eight-mode matrix:
94/113/96/121/96/115/98/123 checks. Source contracts: 15 passed. The paired
workflow pins Core 90f6fe51a0614b16db955f214671cfaedda36948 and executes Core's
41 native-route checks, nine source checks, and this matrix under GCC/Clang in
C++17/NDEBUG and C++20. Hosted results are recorded in the PR and review queue.

The Core inventory separates construction/load hydration from session role
setters and records AccountMgr's distinct RBAC/database update route. It does
not establish notifications for those database transactions or change authority.
Command RBAC dispatch, actual spell bindings/casts, native Core/module ABI and
Worldserver link, real account classifiers, threaded ownership/TSan, installed
state and gameplay remain outside this source-fixture evidence.

No SQL/config/DBC/client dependencies or migration are added. Rollback is removal
of these test-only children; the existing paired Core71/IP194 rebuild and owner
acceptance requirements remain. Nothing is merged, deployed or enabled here.
