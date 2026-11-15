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
