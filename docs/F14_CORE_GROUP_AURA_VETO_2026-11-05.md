# F14 — group membership and applied-aura source veto (draft, OFF)

**Owner:** CosmicCuddle
**Maintenance:** CS01, covering active work while Claude CS03-IP is away
**Date:** 5 November 2026
**Status:** CI-only, isolated, no merge or install

## Exact code and source ancestry

Development branch is a **six-path child** of [IP DRAFT #99](https://github.com/CosmicCuddle/mod-individual-progression/pull/99), exact parent `0856c31fb43731821f45405ce6be8f8bebc7d4bc`. #99's disposable Grim Core `PlayerUpdates.cpp` direct-viewer veto already checks player/pet targets and combat, tap/group loot, GM, target death and map validity **only if** a future positive F14-specific per-viewer hook asks to hide. The hook defaults FALSE and no positive IP listener is installed.

The new `tests/core_patches/apply_f14_core_group_aura_veto.py` is an **exact-anchor extension** of that one **disposable Core helper**, **not** an IP gameplay-source change:

1. The viewer's **charmed unit or Guardian** currently targets or fights the creature, or the creature fights that controlled unit, is a negative veto. These Core relationships may exist beyond the player's primary pet. The pinned Core's `GetCharm()` and `GetGuardianPet()` APIs are checked, with tests in both directions.
2. A normal real viewer who currently belongs to a group/raid is always a negative veto: unknown group-shared quest/loot/kill rights must **not** be inferred away. This is intentionally conservative and can keep many creatures visible.
3. Examine the creature's current Core `Unit::GetAppliedAuras()` container **on the owning map visibility pass**. If it contains more than 64 entries, or an application/aura is null, or the caster GUID is empty/unverifiable, **veto concealment**. Scan at most 64 applications and retain no pointers.
4. Any `Aura::GetCasterGUID().IsPlayer()` — including **RandomBots** — vetoes hiding, rather than guessing whether an aura is a real player's DoT, heal, hostile effect, friendly effect or bot effect.
5. A creature with only a small number of positively non-player-caster auras is not *automatically safe*: the veto may return false, but F14 still needs an independent **complete human rights/identity policy** to return TRUE, which does not exist yet.

The existing #99 player/pet/loot veto, both direct/batched per-viewer call sites, original Core script callback, Core packet bytes and previous F14 #93/#95 source attribution remain intact. The C++ helper is an **extra negative safety layer only**, not an authorization, active invisibility policy, client ACK or prior-hidden ledger clearance.

## Tests and exact limits

- Original #99 patcher/source is SHA-pinned and unchanged; new patcher matches exactly one helper in #99 Core.
- Python source tests validate six changed repository paths; no `src/`, config, SQL, DBC/client, PB/Naxx/Services or host change. They verify short-circuiting remains behind the default-FALSE F14 hook, no mutations, and parent helper preservation.
- A C++ stub harness compiles and executes the **actual newly transformed helper text** in GCC/Clang C++17/20, with/without NDEBUG. Tests absent/valid group, human/pet/charm/guardian targeting and combat, tap, GM, corpse/map, no aura, 64/65 aura bound, human or bot caster, unknown caster and null application. Stubs exercise control-flow logic; they are **not** real Worldserver/map/client gameplay.
- Hosted job also checks the pinned real Grim Core revision `0d754d1921261dfd939f91060ebc022eae280f71` has relevant `GetGroup`, `GetAppliedAuras`, `GetBase` and `GetCasterGUID` APIs. Full C++ Worldserver compilation of this specific new overlay is **still required** before native compatibility can be claimed.
- No positive F14 source, no Packet/Socket/Game client restore evidence, no full quest/DoT/pet/guardian/assist/group credit audit and no proof a hidden client can retarget the creature after combat/death/relog. All F14 and paid feature options remain OFF.

## Next gate and rollback

Only after #99 native link result is independently checked and #100's exact source/behaviour checks are green should the new Core overlay be included in a disposable **full pinned combined native build**. A complete real-player rights/quest/loot/target census, exact selected BE/Draenei RNDBOT identity, client 3.3.5a hide/restore/targetability tests, installed source/config/SQL/DBC/client inventory, tested backup and rollback, and CosmicCuddle's explicit combined release approval are still prerequisites before any gameplay enablement.

Rollback: close the DRAFT branch; original #99, #94, protected repositories and live server unchanged.
