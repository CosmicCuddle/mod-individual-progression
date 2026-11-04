# F14 post-event visibility refresh requester — experimental source implementation

**Owner:** CosmicCuddle. **Maintainer:** Server-Side-Maintenance (CS01).  
**Status:** Isolated DRAFT source, not merged or deployed; no gameplay or packet proof.  
**Base:** IP stage-3 source [#78](https://github.com/CosmicCuddle/mod-individual-progression/pull/78) at `4bb8a8e32b20947d298ed0fb7974453b07829cc1`.  
**Core dependency:** Separate default-false post-event hook [#77](https://github.com/CosmicCuddle/mod-individual-progression/pull/77) at `6182893d18e0ab2bb1d581900910346c1c0819b2` applied to a disposable newest Grim Core; not installed.  
**Playerbots:** unchanged pinned `e88cc4099e69f35126b28942a412ca702d20453b`. Naxx #292 and Services #16 unchanged.

## Problem this implementation addresses

F14's lifecycle registry deliberately retires a creature stamp when its last verified attacker leaves and on corpse/respawn. A future Core refresh callback that asks *only* whether that stamp or positive hide eligibility still exists would miss the **reveal** of the formerly hidden creature exactly when that evidence disappears.

The Core currently has a default-FALSE request hook after the event callbacks. This implementation supplies a guarded, **one-shot request consumer** without granting any right to conceal an NPC.

## Actual new C++ behavior

- In `IndividualProgressionF14LifecycleScript.cpp`, a `Creature::CustomData` record now holds two extra *value-only* flags under compile-time `COSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT >= 1`: `everTracked` and `refreshPending`. A newly tracked creature marks both.
- Before an already tracked creature's attack begins/ends, target changes, combat ends, or its incarnation retires for corpse/respawn, the map-thread event marks `refreshPending`. The pending bit survives `SetStamp(creature, 0)`; a respawn may request a pass even though the corpse callback already retired the stamp.
- The new opt-in `UnitScript::OnF14ShouldRefreshCreatureVisibility(Creature const*)` consumes that pending bit exactly once under `LifecycleLock` and returns TRUE only if the creature was previously tracked, still has an in-world nonpet object and a pending request.
- The Core callback executes **after** the lifecycle callback has returned and the mutex guard has been released. Returning TRUE lets Core **request** `Creature::UpdateObjectVisibility(true)` outside the IP lock; the IP method never directly performs visibility, denies a target, sends packets, updates quest/tap/loot rights or accesses Playerbots.
- On the ordinary/older Core build this experimental method and fields are **excluded at compilation**. The build-only preprocessor gate is not the general feature-ready macro. `COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION` is deliberately **NOT defined**; no new config is enabled.

## Boundaries and remaining blockers

This is **not creature hiding or confirmed client restoration**. No current source grants complete participant, rights, tap, quest, pet, DoT or owner-specific proof. Even a Core visibility pass does not guarantee that the WoW client acknowledged a change, and the existing observer-hook prototype is not sufficient for gameplay readiness.

Actor logout without Core `AttackStop` is still a blocker: identity expiry invalidates registry entries, but there is not yet a safe map-thread creature-target refresh fanout or a durable per-viewer "was hidden" ledger. A creature being removed from its map cannot be refreshed through the removed object; future observer state must fail visible on absence. A future signed-off design must track which observers actually had an object link concealed and explicitly restore it on attack stop, target switch, corpse, logout, config OFF and map transition.

**Acceptance tests required before enabling:** selected bot first/last attacker, multiple bots, human simultaneous damage/DoT/heal/tap/rights, corpse loot sparkle, respawn, logout without AttackStop, map unload, NPC instance/GUID reuse, GM/self and human Blood Elf/Draenei visibility, actual 3.3.5a packet/client redraw and successful quest/loot rights. The owner wants one complete combined in-game test after all systems are ready.

## Safety, tests, integration and rollback

New source: `src/IndividualProgressionF14LifecycleScript.cpp` (guarded C++ only).  
Source contracts: `tests/test_ip_f14_postevent_reveal_request_source.py` (five negative/safety checks).  
Native tests: an isolated workflow must compile/link against the exact Core #77 stack with `-DCOSMIC_IP_F14_POSTEVENT_REFRESH_EXPERIMENT=1`; run only on disposable GitHub runner, no Worldserver launch. Any live config, SQL/DBC, binary, client or account/character data remains untouched.

**Deployment status:** NOT DEPLOYED. **Decision:** preserve default OFF and keep source as DRAFT pending exact native build + review. **Rollback:** close the DRAFT; no host rollback needed. Production installation later requires backed-up installed-source inventory, dependency check and CosmicCuddle's approval.
