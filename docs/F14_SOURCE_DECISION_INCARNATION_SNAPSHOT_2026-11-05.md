# F14 exact Core suppression-decision creature incarnation snapshot (DRAFT/OFF)

**Owner:** CosmicCuddle
**Maintainer:** CS01 Maintenance; Claude CS03-IP on owner-confirmed break
**Date:** 5 November 2026
**Status:** isolated experimental source, unmerged, no host/DB/client changes

## Problem and this source's actual scope

The previous [#105](https://github.com/CosmicCuddle/mod-individual-progression/pull/105) actual, opt-in human `PlayerScript` can provide a current live *normal-account* viewer GUID and non-reused session epoch. [#104](https://github.com/CosmicCuddle/mod-individual-progression/pull/104) receives Core #93/#95 **post-send-call** per-GUID OUT/CREATE attempts, but by then the creature may already be unlinked and its original incarnation cannot be inferred from the GUID alone. Feeding a guessed incarnation to original Claude [#94](https://github.com/CosmicCuddle/mod-individual-progression/pull/94) or pure [#103](https://github.com/CosmicCuddle/mod-individual-progression/pull/103) would be unsafe.

This draft adds a **fourth observation hook** to the existing compiled #104 `WorldObjectScript`, with exact Core #86 `WORLDOBJECTHOOK_ON_F14_VIEWER_CREATURE_SUPPRESSION_DECIDED`. The callback carries **both live WorldObject pointers** at an explicitly F14-attributed *positive suppression decision*, rather than claiming a generic Core visibility/link change was F14. It is still under `COSMIC_IP_F14_TAGGED_GUID_OBSERVER_EXPERIMENT=1`, an opt-in **CI build macro only**, and the real Core **has no registered positive F14 suppression policy**—so this callback cannot be reached from the current OFF-only F14 path.

While the objects are available synchronously, the handler samples only the current caller-provided facts:
- Current non-headless, normal-account viewer full GUID and current login epoch from that Player object's `CustomData` (#105), current in-world status and map/instance.
- Current non-pet, non-totem, non-player-controlled Creature full GUID and its existing *real* `F14CreatureIncarnation` stamp in the Creature's `CustomData`; no new stamp is created.
- Matching **same live Core map pointer**, map and instance identifiers.
- An existing, unpoisoned **#92 lifecycle registry** entry looked up by that exact stamped incarnation, whose stored creature full GUID, map/instance and token match the object.

The registry mutex is acquired with **`std::try_to_lock`**, so recursion or concurrent lifecycle access fails the observation rather than blocking a Core visibility callback. Every pointer is used during the call and discarded; only anonymous aggregate valid/invalid counts and first-event generic diagnostics remain. No GUID, session epoch, Creature pointer, registry entry, map pointer or raw packet is stored in the observer.

A new **pure** `IndividualProgressionF14SuppressionDecisionIdentity.h` classifies missing/wrong source attribution, human viewer, live NPC, map/instance, stale/poisoned or incorrect registry incidence as negative, and returns **`CurrentSourceDecisionIdentitySnapshotOnly`** when its *synthetically or synchronously supplied* facts match. This is deliberately **not** a permission to hide, modify a prior-hidden ledger or infer client receipt.

## Important limitations

- The **Core source decision observation is post-visibility-decision**, not necessarily before the viewer's link flag changes; it is not a client packet acknowledgement.
- Matching current map pointers and a try-locked registry does **not independently prove** the callback is on the map's owning thread. This experiment does not establish a reliable historical per-viewer pair record for a later OUT-OF-RANGE event, and **does not** feed #103/#94. Do not use these facts to authorize visibility.
- Any confirmed snapshot says only the current #92 NPC lifecycle stamp matches its registry. It does **not** establish full human combat/quest/item/loot/group/DoT/assist/kill-credit rights, approval to hide an NPC, a successful CREATE or targetability on a 3.3.5a client.
- The callback is dormant unless a future separately approved **positive F14-only** script is installed; that is NOT done here. Thus the new native code is useful for interface/provenance engineering, not a feature that can be tested in normal gameplay yet.

## Files and test plan

Exactly **seven** reviewable changes on child branch of #105 `48c0e3613473b2b48cbea4830fdba68b968bdc74`: new pure header, narrow macro-gated change in existing `src/IndividualProgressionF14LifecycleScript.cpp`, pure C++ test, Python exact source-regression tests, scoped CI workflow, this documentation and append-only Custom Modifications Register.

The Python source gate reconstructs **byte-identical #105 lifecycle** when removing the new hook/callback/counters, pins Claude original #94 ledger/protocol and #103 adapter Git blobs, validates all experimental flags OFF and no positive F14 policy. The C++ model tests current and missing/mismatched viewer, map, instance, source attribution, stamp, recycled creature and poisoned registry states in GCC/Clang C++17/20 normal/NDEBUG combinations. CI also must compile/link the **actual pinned Grim Core + full IP/Playerbots/Naxx #292/Character Services #16 Worldserver** under the test-only observer macro and verify its callback symbol; **do not report full native success until its exact HEAD CI has completed**.

No deployment, account/character/DB/schema/migration, protected Core fork, PB/Naxx/Services code, DBC, client or server configuration change. Rollback: reject/close this DRAFT. Owner approval, full rights, installed baseline, backup and actual 3.3.5a client restoration/targeting tests remain required for the complete combined candidate.
