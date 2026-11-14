# F14 optional actor publication — atomic reload barrier (8 Nov 2026)

**Owner:** CosmicCuddle. **Review-only child** of [IP #130](https://github.com/CosmicCuddle/mod-individual-progression/pull/130), exact `fb689e84b2cc6f42b8625940d06edda7beaaac4b`. No changes to the owner's machine, SQL/DB/DBC/client, paid grants, quest stages, or F14 positive suppression.

## Specific source problem

IP129/130's experimental `TryReadF14VerifiedActorPublication` read legacy `IndividualProgression::enabled` directly. The existing `WorldScript::LoadConfig` writes this **non-atomic** field during startup/reload; a future WHO worker using the optional snapshot might concurrently read it. Also, toggling IP OFF/ON without a subsequent verified RandomBot login could let an old full-GUID/account/race/epoch publication reappear, despite IP130 rejecting it only while IP was disabled.

## Exact minimal correction

For the otherwise **undefined, OFF** `COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT >= 1` only:

- Create a new atomic `_f14ActorPublicationEnabled`, initialized false. This is a **publication-only** guard; existing public IP `enabled`, original group/progression/paid configurations and other legacy readers stay unchanged.
- `LoadConfig` calls `BeginF14ActorPublicationConfigReload()` before changing options. The existing identity exclusive lock protects the opt-in publication map while the atomic gate is set false and the map cleared. The older verified group/epoch records are deliberately preserved for their original, independent safeguards.
- After `LoadConfig` succeeds, call `FinishF14ActorPublicationConfigReload(ipEnabled)` to set the atomic publication gate to the new IP state. Config reload does **not** republish an actor; only a subsequently positively verified eligible login can publish under normal conditions.
- The opt-in actor writer checks atomic gate under its existing unique lock. The read-only WHO/actor snapshot uses acquire atomic checks before and after taking the existing shared lock, and never dereferences a Player or mutable RandomPlayerbotMgr on the WHO worker. Because snapshots are copies, even a successful read remains invalid immediately after the lock is released.

Source tests cover the lock/write/read/config ordering, mutation attempts to skip revocation, preserved original old group/epoch caches and literal FALSE newer Core6 actor/WHO scripts. **This does not prove that all historic `enabled` readers in the older IP code are race-free**, or that a protected read remains valid through a client send, logout or reload; it only removes this new optional reader's specific unsynchronised access and stale map across reloads.

## Safety and next validation
Normal Core+IP binary compiles this extension out; no runtime config flag is enabled. A new exact five-repository compile with the diagnostic macro ON is still needed if this child is chosen after IP130 native CI; source-only tests cannot establish full C++/Worldserver compatibility or real client reappearance. No WHO/player/NPC hide, stage8 or paid entitlement is granted. Restore and human rights remain separately blocked.

**Rollback:** discard only this child and preserve original IP130. Before owner-approved test/deployment preserve all tracked, untracked **and ignored local custom SQL cleanup and migration files**, installed Core/modules/config, DB/DBC/client/binaries and prove restoration. Effective `Playerbots.DeleteRandomBotAccounts=0` before isolated DB startup.
