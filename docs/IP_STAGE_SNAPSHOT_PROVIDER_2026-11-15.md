# IP cache-only stage snapshot, API v1 (Coordination #59)

## Status and scope
- **Status:** isolated DRAFT on `feature/cs03-ip-stage-snapshot-provider-20261115`, from IP master `05ff032b`.
- **Approval:** contract ACKed with amendments by CS01 (#59 6892638392).
- **Purpose:** gives Playerbots AutoGear the commanding human's stage **without** a DB query or a foreign `Player` dereference.
- **No feature flag.** The snapshot is passive bookkeeping, and no existing IP behaviour changes.

## Contract
- **Header:** `src/IndividualProgressionStageSnapshot.h` (pure; `COSMIC_IP_STAGE_SNAPSHOT_API_VERSION 1`).
- **Getter:** `IndividualProgression::GetStageSnapshot(ObjectGuid const&) const`. Any thread may call it.

**Getter guarantees:**
- a shared lock and a value copy only;
- never a DB query, a `Player` dereference or an insertion;
- a miss returns `{Unknown, NotPublished}`.

**Consumer rules:**
- **Unknown ⇒ deny** before any destructive gear operation.
- **Re-read immediately before destroying gear,** and proceed only if `SameKnownSnapshot(first, second)` holds: same session epoch, generation and stages.
- **Key:** the full GUID (`GetRawValue()`).

## Revision and authorization linearization (revision 2: Codex 6892932909, CS01 6892952406)
- **Revision:** `Snapshot::revision` is process-unique and changes on **every** publish and every invalidation, including a same-value republish, a reload, a new session or a marker command. `SameKnownSnapshot` requires an equal, non-zero revision, so it means "the same publication", not "the same values".
- **Proposed authorization contract** (for CS01/Codex acceptance; no global lock):
  1. **Plan** (non-destructive) from snapshot `A`: candidate items, caps, replacements.
  2. **Mutation block**, on the **bot's own** map thread, synchronous: no yield, no foreign `Player` access. It starts with the final read `B`, and proceeds only if `SameKnownSnapshot(A, B)`. **`B` is the linearization point:** a change ordered after `B` (logout, downgrade, invalidation, marker removal) is ordered after this gear operation, exactly like a command that completed a moment before the change.
  3. **Post-check:** read `C` right after the mutation. If `!SameKnownSnapshot(B, C)`, PB schedules a **corrective pass** on the bot's next own update. It may only remove or re-cap items above the cap of the then-current `Known` snapshot, and **never grants**. Further gearing is denied until a `Known` snapshot exists.
  4. **Never hold an IP lock** across gear mutation or callbacks. Every IP call is a value copy under a shared lock.
- **Lease alternative** (if the weaker semantics above are not accepted): a bounded per-character lease that records "broken" when an invalidation happens during it. It can't block GM or IP changes either, so its outcome is the same corrective pass. That's why the post-check variant is proposed as simpler, with no new state in IP.
- **Composition with #184:** on the combined branch, `CompletePaidExpansionPurchase` invalidates before the confirmed bit is set (lock order is paid → snapshot only).

## Lifecycle
| Event | Where it runs | What it does |
|---|---|---|
| Login | `OnPlayerLogin` | `Register`: fresh session epoch; `Unknown(NotPublished)`; dirty |
| Owner update | `OnPlayerUpdate`, on the map thread that owns the player | if dirty: take a ticket (epoch, generation, invalidation count) under a shared lock; compute with **no lock and no DB** (cache-only `TryGetAccountMatch`; paid facts from IP's caches); publish under a unique lock only if the ticket still matches |
| IP stage write | `ForceUpdateProgressionState` (after the floor guard), `UpdateProgressionState` | `Invalidate` before any marker write or removal |
| Paid flag change | `SetPaidExpansionFlag` | `Invalidate` before the setting or bit changes |
| Marker reward outside IP | `OnPlayerCompleteQuest` (66001–66018) | `Invalidate`. A reward only raises the stage, so the old value was conservative |
| GM command naming a marker | `AllCommandScript::OnTryExecuteCommand`, before Core runs it | `InvalidateAll`. Any number 66001–66018 in the command text matches |
| Config reload | `OnBeforeConfigLoad` (before `LoadConfig`) and `OnAfterConfigLoad` | `InvalidateAll` twice, then re-warm the account cache for online sessions (world thread, outside the snapshot lock) |
| Logout | `OnPlayerLogout` | `Erase` |

**Rejected publishes** (they change nothing):
- the character logged out (not registered);
- a different session epoch: logout→relogin ABA;
- a generation change: reload during the computation;
- an invalidation of this character during the computation.

**Account cache miss:** publishes `Unknown(AccountUnresolved)` and stays dirty, so the owner retries once the cache is warm.

**Paid purchases:** only a durable confirm sets the bit, so Pending or Unconfirmed purchases keep the conservative effective stage. When this branch is composed with #184, `CompletePaidExpansionPurchase` must call `InvalidateStageSnapshot` before setting the bit; that's an integration item.

## Known gap (Core lane)
- Core's quest-clear **spell effect** (`SpellEffects.cpp`, `RemoveRewardedQuest`) has no script hook, so a spell that removes an IP marker would not invalidate the snapshot.
- No such spell is known.
- **Request:** an `OnPlayerRemoveRewardedQuest` Core hook.
- `.quest remove` is covered by the command hook.

## Evidence (developer-reported)
- **Native model:** `tests/stage_snapshot_model_test.cpp`, 2,112/2,112 under GCC/Clang × C++17/20.
  - It covers full-GUID keys, the lifecycle, logout→relogin ABA, a stale reload writer, invalidation while computing, the unresolved-account retry, a downgrade, command marker detection and `SameKnownSnapshot`.
  - It includes a reader/publisher/invalidator **ThreadSanitizer** run, which is clean.
- **Contracts:** `tests/test_ip_stage_snapshot_source.py`.
- **Mutations:** W1–W12 and H1–H8 all caught.
- **Build:** object compile and full worldserver link on the synced stack.
- **Not covered here:** gameplay, and the PB IP-absent build (Codex's side).

## Rewarded-quest mutation audit and proposed Core hook (CS01 6893262981; design only)
Core `21ff1973` plus modules. All direct writers of `Player::m_RewardedQuests`:

| Kind | Writer | Callers | Notes |
|---|---|---|---|
| insert | `Player::SetRewardedQuest` (`PlayerQuest.cpp:906-910`) | `RewardQuest` (`:849`; already followed by `OnPlayerCompleteQuest`); `Spell::EffectQuestComplete` (`SpellEffects.cpp:5009`, **no hook**); IP `GrantAttunements` (`IndividualProgression.cpp:394`, attunement quests) | `insert` on an existing id is a no-op |
| erase | `Player::RemoveRewardedQuest` (`PlayerQuest.cpp:1587-1597`) | `Spell::EffectQuestClear` (`SpellEffects.cpp:5382`, **no hook**); GM `.quest remove` (`cs_quest.cpp:173`); IP `ForceUpdateProgressionState` (markers, already invalidated), `CleanUpVanillaPvpTitles` (`:1498`), `OnPlayerCompleteQuest` (`IndividualProgressionPlayer.cpp:706`); PB `DropQuestAction` (3 sites); PB `PlayerbotFactory::ResetQuests` (`:1702`, bots) | a missing id is a no-op |
| bulk load | `_LoadQuestStatusRewarded` (`PlayerStorage.cpp:6493-6517`) | login load | before the character is in world; the snapshot isn't registered or published until after `OnPlayerLogin` / the first owner update |
| none | daily/weekly/monthly/seasonal resets | `World.cpp` | they clear other sets, **not** `m_RewardedQuests` |

**Gap:** `OnPlayerCompleteQuest` fires only from `RewardQuest`, after its side effects. `EffectQuestComplete`, `EffectQuestClear`, `.quest remove`, PB drop/reset and module calls have **no** hook. IP's `OnTryExecuteCommand` covers only GM commands.

**Proposed narrow Core hook** (Core lane, for review):
- `PlayerScript::OnPlayerBeforeRewardedQuestChange(Player* player, uint32 questId, bool rewarded)`.
- Called inside `SetRewardedQuest` and `RemoveRewardedQuest`, **only when the set actually changes** (`find` first), and **before** the insert or erase.
- Not called from `_LoadQuestStatusRewarded` (load is not a change).
- **Threading:** it runs on whichever thread mutates the Player, the same as the mutation itself, so it adds no new cross-thread access.
- **IP consumer:** `if (IsStageMarkerQuest(questId)) InvalidateStageSnapshot(player->GetGUID())`. It does no Player mutation and takes only the snapshot lock, so it's **reentrancy-safe**; the owner thread republishes afterwards.
- Once this hook exists, the `OnTryExecuteCommand` text scan and the `OnPlayerCompleteQuest` invalidation become redundant. They can stay as defense in depth.
- **Not covered:** writes that bypass `Player` (raw SQL on `character_queststatus_rewarded` while the character is online) — none found among the audited runtime writers. PB orphan cleanups are offline/startup.
