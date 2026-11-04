# F14 Core viewer-target link transition prototype

**Owner:** CosmicCuddle. **Maintenance scope:** isolated test-only Core patch, before any combined game test.
**Branch:** `research/cs01-f14-per-viewer-delivery-20261104`.
**Status:** DRAFT, **NO HOST**, F14 flags default OFF. No verified permission to hide anything.

## Implemented Core observation (test-only patch)

The optional Core patch `tests/core_patches/f14_per_viewer_server_link_observed_prototype.patch` is applied **after** the original three #56 F14 observer/WHO/attack hooks and #67 Core after-pass observer. It adds one opt-in script event:

`WorldObjectScript::OnF14ViewerTargetLinkChanged(Player const* observer, WorldObject const* target, bool nowLinked, bool batched)`

This event is called at **actual server-side viewer/target link transitions** in both existing visibility routes:

1. `Player::UpdateVisibilityOf(WorldObject*)`: the direct per-target branch has already sent a server-side create/destroy operation and changed the player's visibility link. The callback has `batched=false`.
2. `Player::UpdateVisibilityOf(T*, UpdateData&, ...)`: the batched map-visit/initial visibility branch has changed the visibility link and queued `UpdateData` for a future packet build. The callback has `batched=true`. Packet delivery is **not** complete at callback time.

Only actual transitions with `HaveAtClient(target)` changed are reported; no-change checks emit nothing. Only Player objects or **non-pet creatures** are observed. Pet-removal callbacks may destroy pet objects during visibility cleanup, so this prototype explicitly excludes them rather than risking a stale pointer. The callback is synchronous, holds no target pointer afterwards, and must never re-enter the visibility scanner or alter a target.

This supplies **per-viewer server-side bookkeeping evidence**, not a network/client acknowledgement. No packet acknowledgement or client rendering is verified. Until a separate independent safe deny rule exists, the effective Core behaviour remains **default ALLOW**. A hook listener cannot treat `nowLinked` as a guarantee that WoW 3.3.5a has displayed or removed an object.

## Still absent

- A complete, positively verified rights/quest/loot/threat/DoT/pet and viewer safety decision, including existing target, in-flight spell, group credit and aggression reach.
- A **target-scoped immediate event-driven visibility refresh** on every relevant combat/loot/progression/identity event, after the F14 lifecycle mutex has been released. The existing Core non-player forced route can revisit nearby clients, but that does not mean a caller has yet triggered it at all required times.
- Complete F14 bot visibility and stage-8 gating with logged-in RNDBOT identity and GM/excluded-account safeguards.
- **No verified permission to hide** the bot or creature; the hook runs only in a disposable compile when its test patch is applied. It does not set `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION`, never returns a deny decision and does not change gameplay settings.

The future source consumer must prove that the target change invalidated all cached viewer-pair generations, that all interested nearby viewers were covered, and that corpse/respawn restores any corpse loot/quest sparkle. Only then can a distinct owner-approved readiness gate be considered.

## Evidence and rollback

Run the pinned Core patch application and source tests, followed by a full native Worldserver build **without launching** on a disposable GitHub CI runner. No installed server/DB/SQL/configuration/DBC/Playerbots files are changed. The patch is additive to #67 only, and can be discarded by closing the isolated DRAFT PR. The original historical #67/#70 successful evidence remains unchanged.

This is a development milestone, **not a combined test candidate** or permission to deploy.
