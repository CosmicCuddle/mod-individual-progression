# F14 per-creature batched out-of-range **send-attempt** provenance (Core CI-only)

**Owner:** CosmicCuddle. **Component:** Grim Core per-observer visibility + Individual Progression. **Date:** 5 November 2026. **Base:** F14 #86 exact `2933d939bea7ebf9abca224c585ddc3c08744eda` (default-FALSE suppression; Worldserver tested).

## Exact problem

The batched `Player::UpdateVisibilityOf` modifies an in-memory viewer link and adds the creature to `UpdateData` *before* `VisibleNotifier::SendToSelf` builds and hands `SMSG_UPDATE_OBJECT` to `Player::SendDirectMessage`. Generic link removal is NOT proof of F14 concealment, and a batched link event is NOT proof even of a packet send call. Claude's Issue #13 source trace correctly separates policy intent, F14-linked server transition, batch send attempt and actual 3.3.5a client display.

## Implementation in disposable Core ONLY

`tests/core_patches/apply_f14_batched_guid_send_attempt.py` does exact-anchor, all-or-nothing edits of SEVEN actual C++ Core files after the #86 observer hook and pinned upstream F14 #56/#67/#76/#77 prototypes. It never alters a GitHub Core repository, production Core or player/client data.

1. `UpdateData.h/.cpp`: hold a **separate non-network `GuidVector`** of F14-attributed out-of-range GUIDs, MAX 512 entries per batch. It does **not** change normal `SMSG_UPDATE_OBJECT` serialization. On overflow it drops **all evidence**, marks attribution overflow and makes the post-send callback ineligible, rather than asserting a partially observed send. `Clear()` drops metadata too. Existing standard out-of-range and create/update payloads are unchanged.
2. `PlayerUpdates.cpp`: only on the batched branch, after an actual `f14PositiveSuppression` decision for a stock-Core-visible **nonpet creature**, and after building the normal out-of-range block, record that exact creature GUID in this batch's side metadata. Ordinary distance/phase/GM/despawn unlinks, direct visibility path, human/Altbot, pets and withheld (unlinked) creates do NOT record F14 send evidence.
3. `GridNotifiers.cpp`: after `i_data.BuildPacket(packet)` and the actual `i_player.SendDirectMessage(&packet)` call, and only if batch attribution has not overflowed, emit one no-listener `ScriptMgr::OnF14VisibilityBatchOutOfRangeAttempt(viewer, guid)` event **for every individually marked F14 creature GUID**.
4. `ScriptMgr.h` and `WorldObjectScript.h/.cpp`: event declaration and default-empty virtual observer; no registered positive listener or client proof consumer. The `OnF14ShouldSuppressCreatureForViewer` query from #86 STILL defaults FALSE and is UNWIRED.

## VERY IMPORTANT: evidence limits

The new event proves a F14-sourced GUID was put into the batch that reached the `SendDirectMessage` **function call**; that is more precise than an F14 server-link decision alone, and includes only F14-attributed GUIDs rather than an undifferentiated update packet. **It DOES NOT prove** successful transport queueing, socket send, packet receipt, object rendering, restore, or that the viewer session/creature incarnation is unchanged. The callback passes only viewer and GUID; it is not safe to call a per-viewer restored-ledger completion based on it. A missing callback does not prove the client never saw a destroy packet.

**Overflow remains a critical safety blocker** for future concealment. The metadata overflow is conservative about *evidence*, not a complete client restore or proof that F14 concealment is safe under load. Even though no positive F14 listener is connected in this build, any future concealment design MUST fail-visible and preserve prior-client restoration obligations across overflow/relog/map unload/death/config OFF. The pure #85 ledger and Claude's new Issue #13 model stay independently UNWIRED.

## Test and rollback

Python source contracts check exact seven Core touched files, batched-only GUID source attribution, generic sweep excluded, post-send call event, 512 cap/poison, clear, byte-unchanged normal packet serialization, no added client-ACK fiction, default-FALSE query and no runtime IP consumer. New hosted CI composes pinned newest Grim Core `0d754d19` plus existing F14 #56/#67/#76/#77/#86 patches and this one prototype, unchanged Playerbots `e88cc409`, Naxx #292 and Services #16, and actually compiles/links Worldserver. **No game server launch or gameplay/client acceptance.** Full-ready concealment macro absent; all F14/paid options OFF. There are no SQL/DBC/client-patch dependencies.

**Live status:** DRAFT/CI-only, not installed or enabled. **Rollback:** discard this isolated child; never merge experimental Core prototype into production without CosmicCuddle's explicit approval, installed source audit, backup/rollback and actual WoW 3.3.5a observer testing.
