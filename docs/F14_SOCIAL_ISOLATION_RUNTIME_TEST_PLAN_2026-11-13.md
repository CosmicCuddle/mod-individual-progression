# F14 social isolation — runtime test plan (PLAN ONLY)

**Owner:** CosmicCuddle. **Developer:** CS03-IP. **Date:** 13 November 2026.
**Applies to:** social branch head with the group-accept member scan or later. **Status:** a plan for a future, owner-approved **test realm**. Nothing here has been run. It doesn't authorise any action on the live server, its databases or its configuration.

## Why a runtime plan
The native and source tests prove the policy and the wiring, but these things can only be shown on a running Worldserver with a real client:
- hook ordering on login and relog;
- map transfers;
- concurrent /who;
- the Playerbots code paths;
- what the client actually displays.

## Preconditions (test realm only)
- **Build:** Core25-compatible, with Playerbots, and with Core's `COSMIC_F14_WHO_GUID_QUERY_EXPERIMENT` for the /who cases (W1–W3). Without it, W1–W3 are expected to be inert.
- **IP options:**
  - `IndividualProgression.Enable = 1`;
  - `IndividualProgression.F14.RandomBotSocialIsolation = 1`;
  - every other F14 option and Paid stay `0`.
- **Characters:**
  - **H0**, a human player at stage 0–7 (set with `.ip set <name> 7` from a GM account; read back with `.ip get <name>`);
  - **H8**, a second human at stage 8 or above;
  - **G**, a GM account.
- **Bots:** at least one Blood Elf or Draenei RandomBot (**R**) and one ordinary-race RandomBot (**N**). Both must **log in after** the option is on, because identity is only cached at login.
- **Optional bots:** an Altbot (**A**) and a SelfBot (**S**) of race 10/11, if available.
- **Logging:** the module writes **no log lines** for this feature. All evidence comes from client chat, the trade window, group frames and the /who list.

## Cases
Expected results assume the option is ON. Each case lists the steps and then the expected result.

| ID | Steps | Expected |
|---|---|---|
| P1 | H0 whispers R | the whisper isn't delivered; H0 receives one in-character whisper from R |
| P2 | H0 whispers R five times within 60 s | still exactly one reply |
| P3 | wait 60 s or more, then H0 whispers R | one more reply |
| P4 | H0 opens trade with R, then invites R to a group | every attempt is declined. Replies follow the per-pair cooldown, which is shared across surfaces. (Accepting an invite from R can't be reached here, because R's own invite to H0 is already declined at the invite step) |
| P5 | R initiates (if a bot can be made to whisper, trade or invite H0) | declined silently, with no reply |
| P6 | H8 does P1 and P4 | normal stock behaviour |
| P7 | `.ip set H0 8` while H0 is online, then H0 whispers R | delivered normally (the restriction lifts at once). Then `.ip set H0 7` and repeat: restricted again |
| N1 | H0 interacts with N, and with A and S if available | normal stock behaviour (never decided by race) |
| N2 | G in `.gm on` interacts with R | stock |
| N3 | R logged in **before** the option was turned on, then `.reload config` | R stays unrestricted until it relogs (documented fail-open) |
| W1 | H0 runs /who for R's name or zone | R isn't listed, and the result count excludes R |
| W2 | H8 runs the same /who | R is listed |
| W3 | R logs out, then H0 runs /who repeatedly; R logs back in | no stale entry and no error; R is still excluded for H0 after the relog (checks S10) |
| L1 | H0 relogs within 60 s of a reply, then whispers R | no new reply until the cooldown ends (the cooldown is in memory and keyed by GUID) |
| L2 | R relogs, then H0 whispers R | restricted. One reply, subject to the pair cooldown |
| L3 | H0 whispers R at the moment R zones or changes map | no crash. Either restricted, or Core's own "not found" |
| L4 | H0 whispers R, then logs out immediately | no crash; the reply is simply lost |
| C1 | two humans on different maps whisper R at the same time, for 60 s | no crash; each human gets at most one reply per cooldown |
| G1 | H8 leads a group that contains R; H0 accepts H8's invite | **H0 is refused** and receives one reply from R (member scan, CS01 decision 6857686419) |
| G2 | the same group, but R is offline when H0 accepts | H0 joins (an offline member is never classified). Once R is back online, whispering, trading and inviting R are still declined |
| G3 | H8 leads a group with N, A or S only; H0 accepts | H0 joins (stock) |
| PB1 | ask R, through Playerbots commands or its AI, to invite or trade with H0, on the same map and on a different map | On Playerbots `0d754d19`, the source shows these call Core's handlers, so they are expected to be declined silently. Confirm on the installed version. Cross-map is the T1 thread case: watch for crashes |
| LFG1 | H0 (stage ≤7) and R queue the Dungeon Finder for the same dungeon | **never matched into one group**; both still get groups with others |
| LFG2 | H8 (stage ≥8) and R queue for the same dungeon | may be matched (stock) |
| LFG3 | H0 queues, then `.ip set H0 8` while still queued | stays kept apart from R until H0 re-queues (conservative) |
| LFG4 | H0 queues with N, A or S | stock matching |
| BG1 | H0 queues Warsong Gulch, then R (solo) tries to queue for the same battleground or the random battleground at the same bracket | R's queue join is quietly refused |
| BG2 | H0 is inside a battleground; R is invited to that same instance | R's "enter battle" is refused; its invite lapses and it leaves the queue |
| BG3 | H8 (TBC) queues and enters with R | stock |
| BG4 | R is already queued or inside, then H0 queues | **never matched into R's battleground**: H0 is placed in another match, or waits (option (d)) |
| BG6 | several Vanilla humans, TBC humans and restricted bots queue at once | no single match holds both a still-Vanilla human and a restricted bot; TBC humans and ordinary bots fill normally |
| BG5 | H0 leaves the queue (or reaches stage 8), then R queues | allowed (stale entry pruned by live re-validation) |
| U1 | a BE/Dr character whose identity was never verified (logged in before the option was on) interacts with H0 | stock, never classified from race |
| T1a | R on the **same** map as H0 invites or whispers H0 through Playerbots | declined silently (ownership is proven on the bot's map thread) |
| T1b | R on a **different** map invites H0 through Playerbots (for example via LFG chat) | allowed (stock; owner fail-open), with no crash under load |
| D1 | H0 challenges R to a duel | the cast fails (no request reaches R); H0 receives one in-character reply |
| D2 | H8 challenges R; H0 challenges N | stock duel requests |
| M1 | H0 mails R while R is online | refused with a mail error; one reply |
| M2 | H0 mails R while R is offline, after R was online earlier in this run | refused (confirmed earlier), with no reply |
| M3 | H0 mails a BE/Dr RandomBot that hasn't logged in since the restart | delivered (unknown) |
| GI1 | (patched Core only) H0's guild officer invites R; R's guild invites H0 | no invitation is sent; H0 gets one reply when H0 started it |
| GI2 | (patched Core only) H8 invites R | stock |
| OFF | set the option to `0`, `.reload config`, repeat P1, P4 and W1 | stock behaviour everywhere |

## Future cases (only once a #176 runtime producer exists; not testable today)
- **Group-floor population:** a BE/Dr RandomBot in an H0-led group never shows a Vanilla era. An ordinary-race RandomBot does. An Altbot (normal account) is never synced at all. An account added to `ExcludedAccountsRegex` (if configured) gets the floor.
- **Map A→B→A, group A→B→A and leader A→B→A** while grouped: the temporary era is never read from a stale context (needs the eager map, membership and leader generation issuers).
- **Unknown classification:** a member with no account match and no verified identity stays outside every population.

## Cases that need the Playerbots candidate (CS01/Codex lane)
| ID | Steps | Expected |
|---|---|---|
| PBD1 | with map worker threads > 0: bots invite, accept, trade and accept battleground invites, with humans and with each other | as before, at most one world tick later; no crash or race (ideally a ThreadSanitizer build) |
| PBD2 | the same with map worker threads = 0 | as before |
| PBD3 | a bot accepts a human's invite | the follow-up still runs (master set, strategies, summon) through `GroupAcceptOperation` |
| PBB1 | `.playerbots bot add <offline RandomBot character>` | refused with a clear message (owner option B) |
| PBB2 | `.playerbots bot add <own alt>` / linked-account alt / `addclass` | accepted, unchanged |

## Pass criteria and evidence
- **Pass criteria:**
  - every expected result above is met;
  - no crash, assert or error spam appears in the Worldserver console;
  - no character's stage changes, except for the explicit `.ip set` steps.
- **Evidence to capture:** for each case, screenshots or chat-log excerpts plus `.ip get` before and after, along with the build fingerprint and the configuration values used.

## Rollback
Set the option to `0` and `.reload config`, or restart without the module build. No database or client change is involved.
