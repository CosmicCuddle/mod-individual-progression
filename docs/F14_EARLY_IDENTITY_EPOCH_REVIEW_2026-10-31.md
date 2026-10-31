# F14 early RandomBot identity: account type and boot-epoch contract

**Owner:** CosmicCuddle
**Date:** 31 October 2026
**Status:** historical source review plus synthetic negative tests, **not a runtime patch**.
**Branch base:** existing unmapped-stage [F14 C++ policy DRAFT #11](https://github.com/CosmicCuddle/mod-individual-progression/pull/11) at `d4ff9374a0b89c42d93e0f57fdc3fc720cfec785`.

## Owner configuration clarification

CosmicCuddle confirms **AddClass bots are not used on their server**. This is a usage decision, **not evidence** that the current effective `Playerbots.AddClassAccountPoolSize` is zero or that existing type-2 rows are absent. No owner data has been read, and no configuration change is requested by this PR.

The pinned Playerbots `Playerbots.RandomBotAccountPrefix` defaults to `rndbot`. `RandomPlayerbotFactory::CreateRandomBots` generates account names from the configured prefix **followed by a number**. However `AssignAccountTypes` allocates types **1=RandomBot, 2=AddClass, 0=unassigned** within that same prefix pool. Even with AddClass disabled, *unassigned* type-0 accounts remain, so prefix alone is not proof of a selected RandomBot.

## NEW source finding — event=add cannot be a durable identity across restart

The historical `RandomPlayerbotMgr::Init()` runs after account assignment and executes `PLAYERBOTS_DEL_RANDOM_BOTS_BY_EVENT` with **`event="add"`**. The prepared statement in `src/Db/PlayerbotsDatabase.cpp` is exactly:

`DELETE FROM playerbots_random_bots WHERE event = ?`

Thus all previous `add` events are deleted when that initialization path runs. It is **incorrect** to design a permanent, across-restart classifier on a saved `owner=0,event=add` row. The manager's `GetBots()` is lazy, returning early if `currentBots` is already nonempty. In `AddRandomBots()`, a newly selected character gets its `add` event and is then inserted in `currentBots`. Later events can remove it. A bot may own a valid type-1 account without being selected in the current bot lifecycle.

**Evidence:** [historical Playerbots RandomPlayerbotMgr.cpp](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/RandomPlayerbotMgr.cpp), [factory](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/Factory/RandomPlayerbotFactory.cpp), and [prepared SQL source](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Db/PlayerbotsDatabase.cpp).

## Current IP lookup is also not positive RandomBot identity

Current combined IP code `IndividualProgression::GetAccountMatch(uint32)` uses a thread-safe regex result cache keyed by account ID; on cache miss it calls `AccountMgr::GetName`, which performs a **synchronous login-database lookup**, and matches `BotAccountsRegex` / `ExcludedAccountsRegex`. This explains why a `rndbot` name might already enter IP's bot behavior, **but regex membership is not Playerbots account type or active character selection**. Reusing `isBotAccount()` to grant TBC stage 8 is not sufficient proof.

Historical Core `WorldSession::HandlePlayerLoginFromDB` triggers IP's player login script before Playerbots' later `sRandomPlayerbotMgr.OnPlayerLogin(bot)`. Core also adds the player to the map before IP's login callback, so that callback cannot by itself guarantee zero Vanilla-observer visibility.

## NEW narrowly proven world-thread identity opportunity (source-backed, still unimplemented)

Further inspection found a simpler possible route than creating a second permanent account/character snapshot. Historical [Playerbots WorldScript `OnUpdate`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Script/Playerbots.cpp) invokes **`PlayerbotHolder::UpdatePendingLogins()` and then `sRandomPlayerbotMgr.UpdateAI(diff)` on its world-thread tick**. `UpdatePendingLogins` processes the headless `WorldSession` query callbacks, whose bot login callback invokes Core's `HandlePlayerLoginFromDB` (containing the IP PlayerScript login). By contrast `AddRandomBots` selects character GUIDs from `rndBotTypeAccounts` **and records `currentBots.insert(charInfo.guid)` before subsequent asynchronous bot login completion**. In that normal chosen-bot path, historical [`RandomPlayerbotMgr::IsRandomBot(ObjectGuid::LowType)`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/RandomPlayerbotMgr.cpp) tests account membership + current selected GUID using in-memory lookups, **without a synchronous database query**.

**Engineering implication:** It may be possible to call this **existing positive predicate only during the verified world-thread headless login path**, avoiding a duplicate snapshot, shared mutable map and extra login DB call. A reviewer must check **exact current Core/PB sources, actual thread affinity, timing of logout/error removal, script order, class/interface visibility and linking**, and restrict access to the world thread. Do NOT reuse the mutable `currentBots` predicate from ordinary map-thread `OnPlayerAfterUpdate`, packet hooks, WHO/map visibility or arbitrary AI worker threads; that would be an unproven data race. `IsRandomBot(Player*)` has a conditional Selfbot check based on pre-attached AI, whereas the GUID overload only checks account and selected GUID, so an explicit headless-session provenance gate may still be needed. The source does not establish a ready-made pre-map visibility filter.

**No decision or executable bridge is implemented by this test PR.** If same-thread API/link feasibility fails, fall back to a dedicated immutable, before-login selection handoff. The earlier epoch-model checks remain valuable for stale/invalid states, but an actual independently exposed `boot_epoch` field may be unnecessary if the existing current-selected set is proven safe at the exact call site.

## Proposed safe runtime interface — not implemented

If the simpler same-world-thread existing predicate cannot be used safely, a future source-matched adapter could publish an immutable/read-only **boot-epoch snapshot** holding: account ID and verified type 1 from the startup assignment; configured generated-name prefix as a consistency check, not authority; current selected character GUID and lifecycle epoch/state from the producer **before** Core login; and actual bot session provenance. IP would use it only to supply `Identity::VerifiedRandomBot` to `IndividualProgressionRandomBotStagePolicy::Resolve`. The source of `TbcAccess::Available` must remain **separately verified**. Type-0/2/human/Altbot/Selfbot/Unknown must never receive F14 stage grants or F14 observer concealment.

Critical lifecycle choices to resolve before coding:

- Publication must precede `HandlePlayerLoginFromDB` and pre-map visibility admission when concealment is required. It must support normal login, successful selection, aborted/login-error paths and restart/reassignment.
- Account type alone cannot tell whether the *character GUID* was selected; old GUID events from a previous boot must not be accepted after the epoch changes. No extra synchronous DB calls on map/login hot paths.
- Publish/update without data races across the actual Playerbots/WorldSession map/thread processors; do not expose mutable `currentBots` without a proven safe interface.
- Know whether disabled-periodic-offline bots should have the *same policy as selected* bots; don't silently decide this gameplay rule.
- Preserve saved stage-8+ quest markers before any command, group, or login side effect; never 'repair' stage 8 after stage-0 reset already deleted achievements. An unknown classification is **not** a license to run the existing destructive login reset against a potentially matching race-10/11 type-1 account.

## What this DRAFT tests

A pure Python model rejects prefix-only cases, unassigned (0), historical AddClass (2), unknown type, unmatched account/GUID, inactive bot session, old-boot selection and unselected type-1 accounts. It accepts a synthetic positive type1 + exact generated-name pattern + matching account/GUID + current boot selection + bot session. The separate source checks assert precise historical Core/Playerbots/IP code and **explicit startup deletion of `add` events**. Read-only GitHub Actions checks out pinned public sources, executes deterministic tests, and ensures **exactly 3 non-runtime files** changed.

**Passing means the limitations are now tested and documented. It does NOT demonstrate the proposed read-only producer snapshot exists, the owner uses matching SHAs, a character is safe, or F14 is enabled.**

## Implementation and release boundary

Use the [first gameplay test plan](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/blob/main/handovers/Server-Side-Maintenance/2026-10-31-ip-first-gameplay-test-plan.md) only after the owner-installed source, restorable backups, staging environment and approval are independently verified. Coordinate source ownership and rndbot-only observer filtering with the already assigned Naxx F14 work in [Coordination #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9); do not create a second competing visibility system.

**No runtime IP/Playerbots/Core/Naxx code, production SQL, config, live account/character, binary or server process is changed by this branch.**
