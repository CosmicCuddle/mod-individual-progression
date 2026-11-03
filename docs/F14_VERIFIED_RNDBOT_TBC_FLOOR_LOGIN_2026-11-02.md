# F14 — positively selected race10/11 RandomBot minimum TBC stage at IP login (2 Nov 2026)

**Owner:** CosmicCuddle. **Priority:** P0 because genuine Blood Elf and
Draenei RandomBots now appear in the live Vanilla era. **Status:** isolated
DRAFT source, default OFF; source tests NOT proof of actual game behavior.

**Parent:** IP combined candidate #34 exact `a6ac77d9fae9690be91f6ddb97422476164c0ca2`.
This is a new branch, not an edit to the existing #19/#24/#34 owner or
Claude's reserved receipt source. Existing Hunter pet, group policy, paid
source, GM safety and SQL/data are unchanged by this child.

## Bounded behavior

`IndividualProgression.F14.EnforceVerifiedRandomBotTbcFloorOnLogin = 0`
is independent and OFF by default. When ON, on the **existing non-normal
account login path** only, the code additionally requires:
- the IP BotAccountsRegex classifies the account as bot;
- race is Blood Elf (10) or Draenei (11);
- session is headless;
- the source-verified Playerbots manager reports a **positive selected
  RandomBot** for the GUID.

When all signals agree, the bot receives a raw tier floor stage8 for
level <=70, and stage13 for level 71+. If already rewarded at or above
that floor (max valid 18), **NO destructive ForceUpdate** occurs. If
saved below floor, the existing IP `ForceUpdateProgressionState` assigns
the floor; this writes awarded quest markers, so **do not enable on live
characters without a source-matched, restorable Characters DB backup and
explicit owner approval**. No human, Altbot, SelfBot or identity-unknown
character is intentionally targeted. With flag OFF, exact baseline login
flow is intact. The previous partial `PreserveVerified...` flag remains
untouched (the new guarded case takes priority when both are set).

**Vanilla cap:** raw stage8 can be assigned even if global
`IndividualProgression.ProgressionLimit` is 7. This intentionally
distinguishes a race's identity/progression band from a global Vanilla
world cap, **not** a promise that stage8 has effective TBC access.
Existing `MayGroup` rejects stage8 above cap and cross-era Vanilla
grouping. Test real `GetEffectiveProgression`, spawn visibility and IP
limits before production.

## Known incomplete gates

This only covers a verified RandomBot at Core's IP OnPlayerLogin point.
The Playerbots manager's selected-GUID set may not be populated for all
startup/gen/reconnect paths; those remain **separate F14 blockers**.
Other PB reinit/reset and IP group/command writers may erase stage and
need dedicated preservation guards; source-only C1 ResetQuests patch is
not yet installed. **No world or /who visibility code** is included,
and old race-only /who #145 would also hide humans, so it is unsafe.

Need 1/60/70/71/80 levels, saved 0/7/8/12/13/18,
positive RandomBot vs human, Altbot, SelfBot, excluded/missing API and
global cap7/8/13 testing; ensure zero unexpected world/character writes
with switch OFF. Older strict #19 tests are kept by subtracting only the
new pilot block and verifying the old region remains byte-identical.
New pure model C++/Python tests plus native pinned full Worldserver
tests (still separate), then owner-operated offline disposable real bot
+ human visibility + DB verification. Rollback: switch OFF and replace
candidate module; **undoing a past stage-marker write requires an
owner-approved restored DB snapshot**, not simply switching the flag.

**References:** [F14 coordination #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9),
[Master P0 list #28](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/issues/28#issuecomment-6587910058),
[dedicated Grok review #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6587871094).
