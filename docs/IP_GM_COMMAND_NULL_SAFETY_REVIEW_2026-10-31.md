# IP GM command target and session null-safety (independent DRAFT)

**Owner:** CosmicCuddle. **Review source:** historical upstream and [Grok IP fork review (Coordination #11)](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11).
**Scope:** Only `src/cs_individualProgression.cpp`, five GM commands. No change to IP progression stage formulas, F14 policy, group acceptance, achievements, SQL or any owner-host data.

## Confirmed historical defects

- `.ip set` accepts an optional `PlayerIdentifier` but unconditionally reassigns it to `PlayerIdentifier::FromTargetOrSelf(handler)`; this **ignores an explicit player argument**. The function then dereferences the optional and `target` without a null check. With no target, no connected player, or console context, this risks a null dereference before any intended error response.
- `.ip setbot` uses `handler->GetSession()->GetPlayer()` even though the command table registers `Console::Yes`. In a genuine console command, `GetSession()` can be null, causing a possible crash.

## Minimal DRAFT patch

1. In `.ip set`, only resolve selected target/self **if the explicit player is missing**. Reject a missing player identifier; then reject an offline or not-in-world target **before** reading name/stage/area or calling `CheckProgressionAchievements` and `ForceUpdateProgressionState`.
2. In `.ip setbot`, check `WorldSession*` before calling `GetPlayer()`; also reject a missing/unloaded player before any group lookup. Console invocation now responds with an error instead of dereferencing a nonexistent session.
3. In `.ip tele`, preserve an explicit online character name/GUID instead of overriding it with the current target; refuse an absent/offline target before checking progression, changing raid difficulty, or teleporting.
4. In `.ip setrep` and `.ip attune`, reject absent console sessions and non-world players before group/reputation or attunement operations.
5. **Intentionally preserve** valid connected-player and grouped-bot progression logic, command registration, achievements and feedback semantics. This patch **does not** prevent stage downgrades, unsafe manual stage requests under F14, `ProgressionLimit` bypass, or prewrite achievement cleanup in an otherwise valid command. Those require a separate, owner-approved stage-write policy.
6. No character, account, SQL, config, DBC, client-patch, Core, Playerbots or Naxx source changed by this isolated branch.

## Source and tests

- Parent: current `develop/cosmic-ip`, not the experimental Hunter/grouping or F14 branch. This patch is independent and can be reviewed on its own.
- Fifteen Python source regressions require explicit-name precedence, null guards before mutation, existing ability/teleport/reputation/attunement rules and unchanged valid-path write calls. These are textual guards, **not behavioral or gameplay evidence**.
- The historical pinned full Worldserver CI compiles `cs_individualProgression.cpp` from the **exact PR head** with Core `8daffdabe9202c228e45d833c9a1d399597a15cd`, PB `e88cc4099e69f35126b28942a412ca702d20453b` plus two **runner-only compatibility adjustments**, and the historical Naxx module. That is a disposable source/build check only, never the actual owner's installed binary.

## Required staged and rollback verification (not yet completed)

Test `.ip set` with explicit online name, GUID, selected target, numeric first argument, missing target, nonexistent or offline player; `.ip tele` with explicit named/GUID online target and different selection, missing/offline target and console invocation; `.ip setbot`, `.ip setrep`, `.ip attune` from console (returns safely), and from valid in-game contexts, including no-group errors. Avoid changing real progression; **use backed-up disposable test characters only**. Verify quest rewards/achievements and phase updates are unchanged for valid commands. A passing compile does not establish runtime success.

**Owner safety:** No merge or deployment without CosmicCuddle's approval. This narrow crash fix doesn't make other IP stage writers safe. Revert this independent draft to restore source. Actual historical rewarded quest deletion requires a restorable character DB snapshot for any risky staging tests. Record any future merge as a separate Custom Modifications Register event.
