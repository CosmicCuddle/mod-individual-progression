# F14 optional RandomBot actor identity — reject in-flight login publication after .reload

**Owner:** CosmicCuddle. **Date:** 8 November 2026. **Maintenance:** CS01-IP.
**Parent:** [IP review DRAFT #133](https://github.com/CosmicCuddle/mod-individual-progression/pull/133), immutable head `b4c2eb8073bb7178a4fbac8e744cb456f6b8eaf2`.
**Status:** isolated DRAFT, OFF by default, **not merged, deployed, game-tested or owner-approved**.

## Why this narrowly follows #132/#133

IP132 added a useful atomic publication-only enable switch and revoked old full-GUID actor snapshots on IP config reload. IP133 corrected an inherited source-test literal and passed its own source/pure model CI, while native exact five-owner CI is running independently. Neither draft prevented this interleaving:

1. Verified RandomBot login reads `Player` facts and establishes `canPublishActor` while the publication switch is ON.
2. That login is paused before acquiring `_f14GroupCacheLock`.
3. `BeginF14ActorPublicationConfigReload` obtains the identity lock, sets publication OFF, clears snapshots; `LoadConfig` completes and `FinishF14ActorPublicationConfigReload(true)` re-enables the switch.
4. The **old** login writer now obtains the lock. Its IP132 gate sees ON again and can publish the pre-reload identity. There was no verified login started after the reload, contrary to the source's and previous policy's promise.

There is no evidence this sequence occurred on the owner's server. WHO/player/creature hooks remain literal/default FALSE, so this is a **preventive optional evidence-producer correction**, not proof of a current gameplay exploit.

## Exact fix and trust boundary

- Add `_f14ActorPublicationReloadGeneration` as a **separate atomic uint64**, initialized zero and compiled only under the already undefined/off F14 actor-publication macro. It never changes the legacy group epoch, earned IP stage, quest, purchases, or paid entitlement.
- At the very start of `IndividualPlayerProgression::OnPlayerLogin`, **before** calling `Forget` and before the original Playerbots `isBotAccount`/`IsRandomBot` verification, capture a `F14ActorPublicationLoginLease` containing the generation and whether publication was ON. Pass this value unchanged into `RememberF14VerifiedRandomBotGroupIdentity`; a token captured only inside `Remember` would be too late for a login already paused immediately before that call. Preserve the original source-of-truth verified `IsRandomBot` login path.
- At `BeginF14ActorPublicationConfigReload` inside the existing unique identity lock, set OFF, increment generation once (if below uint64 max), and clear only the new actor publication map. No old group/epoch cache clear, no SQL.
- A verified login may publish only with the existing positive facts **and** switch ON under lock **and** captured switch ON **and** captured generation exactly equal to current generation while holding that lock. This rejects an old in-flight writer even after OFF/ON. It also rejects a verification started while reload was OFF.
- At uint64 generation exhaustion, stop rearming the optional publication switch rather than wrap and reuse a previously issued generation. No additional hide is granted; existing historical group protection remains intact.
- Original IP133 exact inherited source predicate `if (canPublishActor && _f14ActorPublicationEnabled.load(std::memory_order_acquire))` stays in place; this child adds a **second** stricter generation/entry-state check inside it. No scope expansion to WHO or client visibility consumers.

## Review/test boundaries

This child changes **only** production `src/IndividualProgression.{h,cpp}` and the guarded login-entry lease capture in `src/IndividualProgressionPlayer.cpp`, append-only register, this document and two new isolated source/native model tests plus CI workflow (**eight paths**). Preserve IP133, its old tests and all other modules unchanged. Automated checks require exact parent SHA, verify all inherited source safety, demonstrate forced pre-reload-vs-post-reload and during-reload interleavings as negative cases, and run native C++17/20 GCC/Clang normal/NDEBUG pure model. The native model is **not** the actual module C++ or a gameplay test.

Required later: actual Core6/PB10/IP-this-child/Naxx292/Services16 Worldserver link with optional macro enabled only on disposable CI, then no-positive human/Altbot gameplay and eventual approved restoration testing. **Do not confuse source tests with an owner-installed build**. No paid/F14 flags toggled. No SQL, DB, DBC, client, owner PC operations. Any future offline testing must first protect tracked, untracked **and ignored** custom SQL cleanup/migrations and prove recoverable DB and file backups, and get CosmicCuddle's approval.

**Rollback:** reject this isolated DRAFT; original #133/#132 unchanged. No production data touched.
