# F14 — Blood Elf/Draenei rndbot eligibility and TBC start: source investigation

**Owner:** CosmicCuddle  
**Date:** 2026-10-30  
**Status:** READ-ONLY RESEARCH, not an implementation, compile or gameplay test.  
**Source branch:** `research/f14-rndbot-eligibility-20261030`, based on protected `develop/cosmic-ip`.  
**Communication:** [Shared F14 Issue #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), [group expansion Issue #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10).  
**Bounded evidence:** CosmicCuddle IP historical source `f0044987cc2d6f0b73c1cd25d17012a26a764beb` (Hunter code commit); Playerbots historical source `e88cc4099e69f35126b28942a412ca702d20453b`. Updated owner-host Git HEADs and actual installed effective configs are **unverified**. Do not claim they match these references.

## 1. Owner-required behaviour — DO NOT CONFLATE RULES

1. **Rndbot TBC starting progression:** positively identified **Random Bots** of race **10 Blood Elf** / **11 Draenei** must start at **IP stage 8 (first TBC)** and never receive Vanilla stage **0–7**, whether by birth/login, manual assignment, level adjustment, group acceptance, sync or later relog.
2. **During Vanilla progression:** those rndbots must not become a usable/visible Vanilla rndbot population. Do **not** force them to 8 in a way that grants TBC content through a Vanilla progression limit. They may be **pre-created and retained** for later activation if safely excluded. **No deleting/renaming bots or database changes** without a separate explicit approval.
3. **Observer-specific visibility:** Vanilla observers must not see **verified race-10/11 rndbots** in the agreed visibility surfaces, whereas TBC+ observers may. **Human Blood Elves/Draenei always remain visible**. Altbots, Selfbots and Unknown identities also remain stock/visible unless the owner separately changes their rule. Never classify from race or bot-account name alone.
4. **Grouping is distinct:** `EnforceGroupRules=1` groups by Vanilla 0–7, TBC 8–12, Wrath 13–18 and bars race-10/11 from Vanilla groups. `EnforceGroupRules=0` retains normal grouping, but must **not** permit the **rndbot's stage floor** to be bypassed. Grouping code is in separate unmerged [DRAFT PR #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3).
5. **Tier is NOT character level:** a low-level Blood Elf/Draenei may legitimately have TBC **progression** tier 8 in design; *that alone* does not mean level 61. The server's current max level 60 does not prove that the content/tier 8 is unlocked. Evaluate the effective **IP progression limit**, not simply current level, alongside owner decisions.

## 2. Verified IP write-path defects relative to the new rule

| Existing path | Pinned source finding | Why it matters |
| --- | --- | --- |
| Login | `src/IndividualProgressionPlayer.cpp:28–38`: non-normal-account characters at level <=60 are **force-set to stage 0** regardless of race; level 61–70 go to 8, level >70 go to 13 | Any race-10/11 rndbot at level <=60 is assigned Vanilla stage in existing code. This is a **source observation**, not proof that one actually logged in on the owner's current host |
| Force-update primitive | `src/IndividualProgression.cpp:76–103` removes/adds rewarded hidden progression quests but does **not itself check a positive RandomBot identity or stage-8 floor** | Patching the login function alone would leave other writes |
| Group acceptance | `src/IndividualProgressionPlayer.cpp:595–655` can force-copy a leader's stage (Vanilla included), even with `EnforceGroupRules=0` | Accept path must not reset race-10/11 rndbot to Vanilla |
| Group add/sync | `src/IndividualProgression.cpp:431–456` copies a normal human leader's nonzero exact stage to **non-normal-account** members; stage 0 is skipped there | Could force stage 1–7; cannot rely on skipping stage 0 for safety |
| Account heuristic | `IndividualProgression::isBotAccount` uses configurable account-name matching | Bot-account designation does **not** prove live positive RandomBot vs AltBot/SelfBot and is not a safe identity oracle for observer-specific visibility or group threads |

**Proposed invariant:** all intended rndbot stage-changing pathways should go through one testable *authoritative positive identity + race + stage-floor validation*, and the result must reject invalid stage/era writes rather than rewrite other kinds of characters.

## 3. Verified Playerbots generation and activation surfaces

**Pin:** [mod-playerbots/mod-playerbots at e88cc409](https://github.com/mod-playerbots/mod-playerbots/tree/e88cc4099e69f35126b28942a412ca702d20453b).

| Surface | Source fact | Consequence |
| --- | --- | --- |
| Race/class generation | `src/Bot/Factory/RandomPlayerbotFactory.cpp:97–109` `IsValidRaceClassCombination(race, cls, expansion)` suppresses BE/Draenei only if `expansion < EXPANSION_THE_BURNING_CRUSADE`; at :117–139 race candidates are evaluated using `sWorld->getIntConfig(CONFIG_EXPANSION)` | This depends on **Core's global expansion mode**, not a player-specific IP tier. WotLK-client server installations can allow BE/Draenei creation while IP is Vanilla. **Do not assert the owner's actual CONFIG_EXPANSION value without reading it** |
| Character birth | Same file :750–787 constructs bot `WorldSession` with `EXPANSION_WRATH_OF_THE_LICH_KING`, then `CreateRandomBot`, `SaveToDB`, and caches `getRace()` | Bot accounts and characters can exist before the era is available; copying the account-prefix logic to IP would be fragile. Creation may be intentionally allowed, but activation must be gated |
| **Already-created population** | `src/Bot/RandomPlayerbotMgr.cpp:713–780` `AddRandomBots()` reads each candidate's `guid, rRace, rClass, accountId`. The `tryLoginBot` lambda rejects already-scheduled/online bots and configured Death Knights, **but does not inspect rRace or IP tier** | **Strong potential pre-login interception point**: world-thread population selection could skip race-10/11 while IP era/cap is Vanilla **without deleting saved characters**. This is a *Playerbots* code surface, not existing IP-only API. Avoid duplicate owner rules between modules |
| Identity | `src/Bot/RandomPlayerbotMgr.cpp:2262–2284`: `IsRandomBot(Player*)` excludes SelfBot if AI is visible, then GUID overload checks `IsInRandomAccountList(...)` and `currentBots.contains(guid)` | Better evidence than account regex, but **only for active/scheduled rndbots**. Cannot classify candidates solely with this API *before* inserting `currentBots`; use the manager's own positively typed candidate pool and actual account assignment there. Snapshot thread-safety/AI masking and owner-installed version remain unproven |
| Level management | `src/PlayerbotAIConfig.cpp:412–415` `RandomBotMaxLevel` is clamped to global `CONFIG_MAX_PLAYER_LEVEL`, and `src/Bot/RandomPlayerbotMgr.cpp:2001–2148` randomizes levels | Level-60 cap is separate from the TBC **IP tier**; changing bot level alone is not a valid TBC eligibility guard |
| Dangerous deletion switch | `src/Bot/Factory/RandomPlayerbotFactory.cpp:510–650` contains a **`DeleteRandomBotAccounts` branch** that deletes bot characters and related data and triggers shutdown | **DO NOT set `Playerbots.DeleteRandomBotAccounts=1`** as a way to hide existing BE/Draenei. Keep it OFF; not needed for this feature |

### Architecture recommendation (not a grant of development ownership)

**Preferred separation of responsibilities:**

- **IP fork:** own and document a single source of truth for per-character progression era and the BE/Draenei **positive rndbot stage-8 floor**. Handle stage writes at a central reviewed primitive plus existing login/group/commands. Never infer rndbot identity from race or name alone. Honour `ProgressionLimit`.
- **Playerbots integration, minimal and independently reviewed:** if an existing public integration hook cannot gate candidate activation, add a **small reversible default-OFF adapter** to `AddRandomBots::tryLoginBot` that queries an approved IP-derived *server/era eligibility decision* before scheduling race-10/11 candidates. This protects **pre-existing saved rndbots** and avoids first loading them into a Vanilla map. The exact safe API/ABI and execution thread are NOT proven; confirm actual checked-out versions and any existing customization. **Do not edit Playerbots just because a research file says so.**
- **Visibility beyond spawning:** re-use the existing Naxx F14 identity snapshot research [#197](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/197), [#202](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/202) and surface audit [#234](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/234). An offline/hidden bot may still leak race through name-query/cache. Core hook feasibility is unresolved. **No concealment implementation in this research branch.**

**Open policy question:** Vanilla **server** era vs **observer** era are distinct with individual progression. A TBC human and Vanilla human may coexist: globally suppressing all BE/Draenei rndbots would block legitimate TBC access. A fully observer-specific visibility solution therefore may still need world and WHO filtering even if pre-login suppression is used for the present all-Vanilla stage. Do not assume permanent global suppression is correct for the intended future multi-era server.

## 3A. Why existing IP race settings do NOT solve the new rndbot rule

Current fork [`conf/individualProgression.conf.dist`](https://github.com/CosmicCuddle/mod-individual-progression/blob/develop/cosmic-ip/conf/individualProgression.conf.dist) already defines three relevant controls:

| Existing option | Source-backed meaning | Why it is insufficient alone |
| --- | --- | --- |
| `IndividualProgression.TbcRacesUnlockProgression` (default `0`) | Race creation unlocks for ordinary account characters when account progression reaches a configured stage | `IndividualPlayerProgression_AccountScript::OnPlayerCanCreateCharacter` explicitly **lets bot/excluded accounts bypass the human race-unlock check**, so configuring it does not establish a rndbot stage floor or activation block |
| `IndividualProgression.tbcRacesStartingProgression` (default `0`; optional `8`) | `IndividualPlayerProgression::OnPlayerLogin` applies this only in the **normal-account** branch for human Blood Elf/Draenei starting progression | The **non-normal-account** branch above it still forces **any level <=60** character to IP stage **0**, and level 61–70 to stage 8, regardless of race. Changing this setting to 8 is **not** a solution for positively verified rndbots and might change established human gameplay if done globally |
| `IndividualProgression.ProgressionLimit` (default `0`) | `0` means **no IP progression cap**, while `7` would limit recorded progression to the final Vanilla stage | Owner's current **character level 60 cap is NOT proof** this particular IP config setting is 7. Stage eligibility should be based on actual effective IP rules + era unlocks, not assume max level and IP cap are identical |

**No settings change is proposed or performed here.** The owner already has functioning custom configuration and a working server. Test the real installed values read-only before coding against them.

## 3B. Scheduling and login sequence: promising but unproven integration boundary

Historical Playerbots [`RandomPlayerbotMgr.cpp:760–780`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/RandomPlayerbotMgr.cpp#L760-L780) schedules a selected stored rndbot and inserts its GUID into `currentBots` **before** later player loading. [`PlayerbotMgr.cpp:260–281`](https://github.com/mod-playerbots/mod-playerbots/blob/e88cc4099e69f35126b28942a412ca702d20453b/src/Bot/PlayerbotMgr.cpp#L260-L281) later calls `botSession->HandlePlayerLoginFromDB(holder)`, and invokes `sRandomPlayerbotMgr.OnPlayerLogin(bot)` before queueing the post-login operation.

Consequences:

- Positive identity **may** be available to World-thread callbacks for **already-scheduled** rndbots via the manager's verified account-list + active GUID checks. This is more robust than an IP regex, but **exact ScriptMgr OnPlayerLogin ordering, installed Playerbots version, optional module compilation and map-thread call safety remain unverified**. Never call a mutable manager set concurrently from map workers just because it appears in a pinned historical file.
- For **candidate selection before login**, `IsRandomBot(Player*)` cannot be used because there is no loaded Player; the selection manager itself already has candidate race and account metadata, which is the appropriate place to consider a **non-destructive** pre-login eligibility test.
- Newly pre-generated race-10/11 rndbot characters may exist in the database while TBC is locked, but **must not become active Vanilla rndbots**. Do not pre-emptively set or grant TBC rewarded progression quests while the Vanilla cap is active. The first **eligible activation** should start with a verified tier-8 or higher effective stage; safely handling dormant pre-existing characters requires a separate migration/test plan, not a character delete.
- The world may contain Vanilla and TBC individual-progression players simultaneously later. A hard global suppress-all-BE/Draenei switch would then wrongly hide eligible rndbots from TBC players; keep observer-specific visibility and activation eligibility as separate concerns.

**Implementation gate:** identify the verified positive RandomBot classification at every stage-writing entry point, prove the effective server/IP cap and cross-thread lifecycle, and confirm precise owner gameplay behaviour before any executable change.

## 3C. Reuse the existing F14 identity work; avoid duplicating it

The separate [Naxxramas F14 P1 draft #197](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/197) has a **pure positively separated** `Human` / `RandomBot` / `AltBot` / `SelfBot` / `Unknown` classifier; [P5 draft #202](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/202) samples facts through a not-yet-wired world-thread adapter/snapshot. This was explicitly acknowledged in [the existing Naxx F14 development Issue #143](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/issues/143). These are useful inputs for a future approved solution, **but neither is a running identity service for IP right now**.

**Important old-policy hazard:** the P1 prototype's `HideTbcRow` currently offers “hide all” and “hide all except human.” **Neither implements CosmicCuddle's newer approved scope**: hide only when race is 10/11 **and the target is positively `RandomBot`**; `Human`, `AltBot`, `SelfBot`, `Unknown` remain visible. The owning developer already recognised this in Issue #143; do not merge/enable either old policy unmodified, and don't create a duplicate implementation in IP without an owner-approved integration plan.

## 4. Required test matrix before code or production

| Test | Expected |
| --- | --- |
| Positive rndbot BE/Draenei at creation | Persisted/first effective stage never 0–7; if TBC not available, never expose as a Vanilla playable rndbot |
| Positive rndbot BE/Draenei already saved | Keep character, suppress Vanilla selection/activation safely, later permit at TBC when eligible; no DB delete |
| Normal Vanilla-race rndbot | Still allowed to start at Vanilla tier 0 and sync within expansion |
| Human BE/Draenei, Altbot, SelfBot, Unknown | No rndbot-only tier mutation or concealment |
| Manual forced tier 0/1/7, group accept, group-add sync, relog, spawn/randomize | Ineligible race-10/11 rndbot tier write rejected in every path; legal TBC+ transitions preserved |
| Vanilla `ProgressionLimit=7` and max player level 60 | No tier-8 force-bypass or TBC quest leak; no misclassification from level alone |
| Individual TBC human while someone else still Vanilla | Real human visible; rndbot visibility only for eligible viewer; no incorrect global hiding |
| `EnforceGroupRules=0` vs `=1` | Group switch remains optional; mandatory rndbot floor cannot be bypassed through OFF |
| GM/self/group/friends/guild/combat and /who/name-query | Exact owner policy and core hooks confirmed; race and activity do not leak improperly; no unintended target/combat effects |
| RandomBot positive identity and map thread access | Consistent before/after scheduling/login, safe snapshot, no stale GUID or concurrent `currentBots` reads |
| Core/Playerbots/IP/Naxx builds; rollback | Full compatible Worldserver compile/link and real gameplay. Separate backup and restore plan before any owner-host modification |

## 5. What has **not** happened

No C++ source changes, config/SQL/DB/character/client patch, Playerbots modification, server build, native hook implementation, experimental launch, merge or deployment. All conclusions above are **read-only findings from explicitly pinned historical Git revisions**. Whether a dedicated Playerbots fork/patch is needed is a **design decision for CosmicCuddle** after source-version verification, not assumed approval.

**Follow-up:** Share this with Server-Side-Information through existing F14 coordination Issue #9, keep all coding under isolated reviewable PRs, and reconcile with the grouping DRAFT #3 after evidence and approval.
