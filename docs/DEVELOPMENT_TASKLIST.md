# CosmicCuddle — Individual Progression Fork: Development Task List & Plans

**Owner and final decision-maker:** CosmicCuddle  
**Planning baseline:** 1 November 2026 (update after each accepted milestone)  
**Primary repository:** [CosmicCuddle/mod-individual-progression](https://github.com/CosmicCuddle/mod-individual-progression)  
**Working baseline:** `develop/cosmic-ip`; this document is first proposed on a separate **documentation-only DRAFT branch**.  
**Source of modification history:** [Custom Modifications Register](CUSTOM_MODIFICATIONS_REGISTER.md)  
**Two-way decisions / cross-project requests:** [Naxxramas Server Coordination](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination)  
**Important:** This is the IP fork's *planning and execution checklist*, **not** proof that a feature is compiled, installed or live. Keep the register for what was actually modified and how to preserve it.

## 1. Working principles and how to update this file

1. **Development first:** understand the immediate defect, implement useful code in an isolated branch, run focused checks, combine completed features, then run full compatibility/regression/gameplay tests before any owner-approved installation. Do not make every task another test-only PR.
2. **Never overwrite existing gameplay customisations.** Compare installed source/config/SQL/DBC/client state to the proposed changes before merge/deployment. Current GitHub refs and historical CI pins are **not** the installed server.
3. **Record four separate truths:** planned/approved gameplay; source committed in DRAFT; exact-head compiled/linked/tested; owner-staged/live behaviour. These states must never be inferred from one another.
4. **Use task IDs below** in PRs and updates. For each task, record owner/developer, source branch/PR, dependencies, decisions, evidence, tests, rollback and outstanding acceptance. Mark `[x]` only for a *specific verified subtask*; never use a checked item to imply a release.
5. **No live operations** (merge to release, server restart, config toggle, database migration, character stage rewrite, existing archive deletion, source overwrite or new payments) without CosmicCuddle's explicit approval, backup/restore plan and compatibility gates.
6. **Cross-project scope:** this file covers the IP fork and its Character Services, Playerbots, Core and Naxx dependencies. Broader server P1–P4 release-critical work stays tracked in [coordination Issue #4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4) and [Naxx Core Issue #28](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/issues/28); it remains the immediate release priority, not silently replaced by this roadmap.

### Status legend

- **PLANNED** — goal agreed or proposed; implementation has not been proven.
- **DRAFT CODE** — executable source exists on an unmerged PR; does not establish installed or accepted behaviour.
- **DRAFT TEST/DOC** — evidence/protection work, not an executable gameplay fix.
- **BLOCKED / DECISION** — exact owner gameplay rule, dependency or API is still unresolved.
- **NATIVE CI PASS** — only an explicitly cited *historical pinned exact-head* compile/link, not a running server.
- **GAME TESTED / LIVE** — require separate concrete evidence; **no entry here currently claims either**.

## 2. Master work register — existing IP fork plans

| ID / priority | Task and intended result | Verified development evidence | Next actionable work / blocker |
| --- | --- | --- | --- |
| **IP-01 / P1 IP** | **Hunter pet spell-rank autocast ON/OFF continuity.** Preserve the actual *same loaded pet*, valid rank chain and action bar; do not transfer choices across pets, infer stale human intent or enable A02 relay. | [DRAFT #1](https://github.com/CosmicCuddle/mod-individual-progression/pull/1), combined [#10](https://github.com/CosmicCuddle/mod-individual-progression/pull/10), stacked [#19](https://github.com/CosmicCuddle/mod-individual-progression/pull/19) and [#20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20). **DRAFT CODE**; historical pinned native links passed for exact #19/#20 sources, not gameplay. | Complete focused source review, exact installed Core/PB/DBC comparison, tests of autocast ON and OFF with real rank changes, relog/pet swap/logout regression. No per-pet saved-choice replay in this IP scope. |
| **IP-02 / P1 IP** | **Expansion-band group eligibility.** When existing `EnforceGroupRules=1`, allow same-era different raid stages; prevent Vanilla/TBC/Wrath cross-era groups, and prevent Blood Elf/Draenei joining Vanilla groups under the recorded grouping rule. Preserve OFF behaviour. **Humans remain visible** (visibility is separate). | [DRAFT #3](https://github.com/CosmicCuddle/mod-individual-progression/pull/3), integrated [#10](https://github.com/CosmicCuddle/mod-individual-progression/pull/10), [#16](https://github.com/CosmicCuddle/mod-individual-progression/pull/16), [#19](https://github.com/CosmicCuddle/mod-individual-progression/pull/19); [coordination #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10). **DRAFT CODE.** | Verify grouping policy against actual *paid expansion entitlement* proposed in `IP-11` instead of treating raw stage 13 as an unambiguous Wrath purchase; test all invitation paths, cross-era/race matrix, party→raid, BG/LFG and OFF legacy. |
| **IP-03 / P1 IP** | **Prevent missing/offline group leader from rewriting a bot's saved stage to zero.** Preserve other accepted grouping behaviour. | Standalone [DRAFT #15](https://github.com/CosmicCuddle/mod-individual-progression/pull/15), integrated [#16](https://github.com/CosmicCuddle/mod-individual-progression/pull/16), inherited by [#19](https://github.com/CosmicCuddle/mod-individual-progression/pull/19)/[#20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20). **DRAFT CODE.** | Preserve no-stage-write before leader identity is confirmed; confirm this works on final composed build and both grouping switch settings. |
| **IP-04 / P1 IP** | **IP GM command null/target safety.** Fix dangerous missing/offline-target dereferences while keeping established command permissions and intent. | Standalone [DRAFT #14](https://github.com/CosmicCuddle/mod-individual-progression/pull/14), integrated [#20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20). Historical #20 native Worldserver link and 15 GM tests passed; **not gameplay**. | Preserve fixes through integration. Check `.ip set`, `.ip tele`, `.ip setbot`, `.ip setrep`, `.ip attune` against effective world DB `command` permissions; policy question [coordination #14](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/14). |
| **IP-05 / P1 IP** | **Prevent an OFF-mode invite from changing stage before Core later rejects that invite.** Do not erase bot progression through rejected side-effect ordering. | [DRAFT #21](https://github.com/CosmicCuddle/mod-individual-progression/pull/21) proves a **test-only hazard**, not a fix; see also direct Playerbots side-effect review [#5](https://github.com/CosmicCuddle/mod-individual-progression/pull/5)/[#9](https://github.com/CosmicCuddle/mod-individual-progression/pull/9). | Implement a narrowly scoped *real* pre-admission guard/commit ordering change after source ownership review; test both accepted/rejected and direct PB `Group::AddMember` routes. Ensure no player or bot stage is changed on a failed admission. |
| **IP-06 / P2 IP** | **F14 rndbot expansion identity and safe stage floor.** Genuine race 10/11 **Random Bots only** should never start/downgrade into Vanilla stage 0–7 when TBC eligibility is permitted. Respect effective progression cap; preserve previously rewarded 8+ stages and never misidentify humans, Altbots or unknown accounts. | [DRAFT docs #4](https://github.com/CosmicCuddle/mod-individual-progression/pull/4), [synthetic #6](https://github.com/CosmicCuddle/mod-individual-progression/pull/6), [policy-only #11](https://github.com/CosmicCuddle/mod-individual-progression/pull/11)/[#12](https://github.com/CosmicCuddle/mod-individual-progression/pull/12), narrow OFF login guard [#13](https://github.com/CosmicCuddle/mod-individual-progression/pull/13) integrated [#19](https://github.com/CosmicCuddle/mod-individual-progression/pull/19). **PARTIAL DRAFT CODE only**. | Identify a positive, restart-safe PB rndbot identity (default account naming assumptions alone are not sufficient for custom config). Wire verified stage policy only after actual PB dependency review; cover login, GM commands, grouping sync, reset, reload and character creation. Do **not** simply force everyone to stage 8. |
| **IP-07 / P2 IP** | **F14 rndbot-only Vanilla observer visibility and /who.** Hide *positively identified* TBC-race rndbots from Vanilla observers where technically valid. **Never hide human Blood Elves/Draenei**; Altbots excluded from this first policy unless specifically approved. | [Coordination #9](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/9), Naxx F14 history [#143](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/issues/143). **NOT IMPLEMENTED/ACCEPTED as complete world and /who behaviour**. | Determine single policy owner and Core observer-specific hook; reconcile Naxx developer work rather than duplicate. Test real visibility, search counts, friends/guild/channels, combat/interaction and GM/self, not merely one packet. |
| **IP-08 / P2 IP** | **C1 protect rewarded hidden IP stage quests 66001–66018 from Playerbots reset.** Avoid losing (or unexpectedly surfacing) saved stages. | [DRAFT test-only #17](https://github.com/CosmicCuddle/mod-individual-progression/pull/17) plus disclosure [#18](https://github.com/CosmicCuddle/mod-individual-progression/pull/18). Claude correction/ACK in [coordination #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13). **NO RUNTIME FIX**. | Preserve Playerbots original `GetQuestStatus==NONE -> continue`; prior shadow-NONE candidate was **superseded**. If an actual PB patch is chosen, protect rewarded markers without rewriting shadows, first use pinned source and real integration tests. No new canonical table, SQL rewrite or stage8 policy without separate approval. |
| **IP-09 / P2 IP** | **Full cross-module integrated candidate.** Combine IP-01/02/03/04 plus bounded F14 with real code and selected dependencies, keeping current defaults safe. | Frozen [#19](https://github.com/CosmicCuddle/mod-individual-progression/pull/19) source received **two historical pinned full Worldserver link successes**, [#20](https://github.com/CosmicCuddle/mod-individual-progression/pull/20) one; both unmerged DRAFT. Review request [Grok/CS02-IP #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11) for exact #19 still outstanding when last checked. | Finish independent #19 review or owner-confirmed replacement; resolve rejected-invite stage-write hazard; integrate only chosen code, then test full gameplay with installed sources. Avoid rerunning previous clean historical builds without a changed cause. |
| **IP-10 / P2 IP** | **Safe upstream synchronisation and preservation.** Track Grimfeather commits, conflicts, source/SQL/DBC/Chromaggus behaviour, rollback and tested versions; don't silently delete custom content. | [DRAFT preservation #7](https://github.com/CosmicCuddle/mod-individual-progression/pull/7), [Custom Modifications Register](CUSTOM_MODIFICATIONS_REGISTER.md). **AUDIT/DOC; no automatic safe merge**. | Confirm owner's installed SHA/config/local differences and known Chromaggus script decision; use targeted reviewed upstream commits, backups and actual in-game regressions. Continue updating register after each source change. |
| **IP-11 / NEW — expansion passage** | **Mandatory earned final-era prerequisite AND paid Chronomancer Vezrath access.** This is a *new owner gameplay request*, not the current character-services behaviour. | Exact owner clarification and ZIP source findings in [coordination #18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18). **PLANNED — NO CODE for new rule**. | Implement IP/Character Services contract described in ``3–4 below; first reconcile milestone, stage/entitlement, cross-era catch-up and existing character migration rules. |
| **IP-12 / NEW — Character Services fork** | **Create CosmicCuddle's own fork of Grimfeather Character Services**, import/preserve ZIP customisations, then correct old progression and payment logic. | Owner-provided `mod-character-services.zip`; [upstream Grimfeather Character Services](https://github.com/Grimfeather/mod-character-services). **FORK NOT YET CREATED**. | Owner GitHub fork step, verify provenance, create isolated preservation/import branch, reconcile with upstream, implement safe purchase transaction + exact rules; see `4. |
| **IP-13 / P3 IP** | **Readiness, regression, release and records.** Compare actual installed module/Core/PB/Character Services/Naxx versions; test before deployment, retain backups, rollback plan and Custom Modifications Register. | Coordination [#4](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/4) + development handovers. **INSTALLED STATE UNVERIFIED.** | Owner-approved read-only inventory, restorable DB/config/source snapshots, same-pins full build, gameplay/operational matrix, release decision and post-install smoke test. |

**Cross-project sequencing:** Owner's immediately blocked raid/release work remains **P1 Warlock, P2 Hunter ammo, P3 World/Auth diagnostics and 30-day archives, P4 quest loot** in [Mod-Naxxramas-Core](https://github.com/CosmicCuddle/Mod-Naxxramas-Core). The IP fork's tasks above must not delay that unrelated agreed release work by assuming a new IP/Character Services feature is already approved to deploy.

## 3. NEW feature plan — earn final era completion, then purchase the next expansion (IP-11)

### 3.1 Owner's required experience

**An earned final boss milestone and a gold purchase must NEVER be alternative ways of entering the next expansion. BOTH are required, in sequence.**

| Character condition | Allow next expansion? | Vezrath action |
| --- | --- | --- |
| Vanilla content not completed | **NO** | Explain what final prerequisite remains; do not charge |
| Final Vanilla milestone earned; TBC not purchased | **NO** — remain Vanilla and level-cap 60 | Offer **one** TBC passage purchase; send yellow reminder |
| Eligible character paid successfully | **YES, TBC entry** | Persist purchase and remove reminder; no next raid-tier award |
| TBC final content not completed | **NO Wrath access** | No Wrath payment |
| Final TBC milestone earned; Wrath not purchased | **NO** — remain TBC and level-cap 70 | Offer **one** Wrath passage purchase; send yellow reminder |
| Eligible character paid successfully | **YES, Wrath entry** | Persist purchase and remove reminder; **do not skip Wrath's first raid tier** |

**Current source evidence, not final owner gameplay acceptance:**
- Individual Progression currently maps **Vanilla 40-player Kel'Thuzad -> stage 7 (`PROGRESSION_NAXX40`)** and **Kil'jaeden -> stage 13 (`PROGRESSION_TBC_TIER_5`)**. Verify the complete configured final requirements with Information before coding the eligibility gate.
- The current stage 8 (`PROGRESSION_PRE_TBC`) is used for Vanilla→TBC gating; **stage 13 already appears in Northrend/level-70 checks**, despite grouping's historical convention of stages 13–18 as Wrath-era. The existing Character Services ZIP advances paid Wrath to stage 14; **stage 14 is Wrath's first completed raid tier**, so blindly awarding it on purchase could skip content. **This is a design-blocking stage-model conflict to reconcile**, not permission to shift existing historical stages silently.
- An achievement/rewarded hidden quest proves **earned progression**, not necessarily a **paid expansion entitlement**. Raw stage alone must never serve as both without a reviewed migration.

### 3.2 Engineering work items

- [ ] **IP-11A — Source contract:** enumerate every automatic boss/achievement/custom kill/login/GM/bot/`ForceUpdateProgressionState`/account-tier path which can cross era boundaries; mark each as earned stage, payment-authorised stage, or privileged explicit override.
- [ ] **IP-11B — Single-authority earned check:** implement a reusable character-specific eligibility predicate from the actual final Vanilla/TBC requirements. No account-wide achievement shortcut to buy an unearned next era.
- [ ] **IP-11C — Durable paid entitlement:** define and version per-character `TBC_Purchased` and `Wrath_Purchased` (names/schema provisional). Keep separate from raid-completion markers; restore correctly after relog/restart; design migration/rollback before any SQL is executed.
- [ ] **IP-11D — Stage / access mapping:** choose the safest representation of **earned Vanilla final**, **TBC purchased**, **earned TBC final**, **Wrath purchased** without incorrectly awarding Wrath's first tier or changing all existing 0–18 meanings. Update XP caps, teleports, instance access, TBC/Wrath phase/visibility, starting zones and IP expansion grouping consistently.
- [ ] **IP-11E — Auto-progression isolation:** a final boss kill can award its documented *final current-era milestone* and eligibility but must not trigger paid entitlement/next expansion entry on login, achievement scan, bot sync, custom kill or other path.
- [ ] **IP-11F — Vezrath API / NPC:** Character Services displays relevant offer only when the IP policy confirms eligibility and no purchase. Recheck at click, protect against forged action IDs, return success/failure before debiting money, and make repeat clicks/retries safe. Preserve existing player services.
- [ ] **IP-11G — Reminder:** yellow in-game text on eligible login, e.g. “You have completed the requirements to unlock The Burning Crusade! Visit Chronomancer Vezrath to purchase access.” Repeat for Wrath; stop immediately once purchased. **`/reload` may only reload client UI**, so verify an addon/client hook or document login-only limitation rather than asserting it works automatically.
- [ ] **IP-11H — Old characters:** decide grandfathering for already advanced/purchased characters and handle unpaid-but-completed milestones without revoking earned progress or charging twice. Rehearse migration on anonymous disposable copies; backup/restore mandatory before any live DB migration.
- [ ] **IP-11I — Special identities:** explicitly preserve approved Human, Blood Elf/Draenei race starting zones, Death Knight initial tier, GM/excluded account, rndbot vs Altbot behaviour; never assume a bot should pay as a human or that a paid account flag covers every character. Owner approval needed for unresolved exemptions.
- [ ] **IP-11J — Account catch-up:** existing Character Services also offers paid per-tier catch-up based on account-highest progression. **Within-era catch-up** is a distinct existing feature; decide whether it remains. **No generic tier purchase, account-highest value, GM-compatible gossip or 8/14 shortcut may bypass a cross-era earned prerequisite/payment.**
- [ ] **IP-11K — Regression and acceptance:** test real human A (earned/not paid), human B (not earned/account already advanced), no gold, one-click, double-click, forged action, character relog, NPC unavailable, server restart, staged restore, actual reward/boss, XP/teleport, F14 and grouping, TBC race and DK, and the true Wrath start. Log proof of successful payment without sensitive data; no silent gold loss.
- [ ] **IP-11L — Deployment:** compile/link with actual selected IP/Character Services/Core/Playerbots/Naxx refs; test both eras on staging, backup config + character DB, define rollback that restores entitlement and gold consistently, then request CosmicCuddle's explicit go/no-go.

**Open gameplay decisions for Information ([Issue #18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18)):** exact boss/achievement/quest bundle, cross-character catch-up policy, previously purchased / already-advanced character treatment, per-race/bot/GM exceptions, effective price/source, and UI reload notification behaviour. Do not treat developer guesses as settled owner rules.

## 4. NEW repository and module plan — Character Services (IP-12)

### 4.1 Preserve before changing

- [ ] **CS-01 — Create real GitHub fork** of [Grimfeather/mod-character-services](https://github.com/Grimfeather/mod-character-services) under **`CosmicCuddle/mod-character-services`**, keeping upstream remote/ref and original licence/credit. **NOT CREATED** when this document was first drafted. Do not create an unrelated source-less repository as a substitute for fork provenance.
- [ ] **CS-02 — Preserve exact owner ZIP snapshot** on an isolated branch from the fork; record upload checksum, base upstream HEAD and every changed file in the modification register. **No assumption that ZIP equals owner-installed version.**
- [ ] **CS-03 — Reconcile exactly two custom files** against current upstream: `src/CharacterServices.cpp` and `conf/character_services.conf.dist`. All other 21 ordinary source files in the owner's supplied ZIP matched Grimfeather's checked upstream at the time of review. Copy intended behaviour forward; do not wipe upstream fixes wholesale.
- [ ] **CS-04 — Restore and improve Vezrath expansion service**: preserve distinct passage offers, dialogue, confirmation, gold cost, and independent normal Character Services. Owner ZIP config specifies **1,000 gold Vanilla→TBC** and **3,000 gold TBC→Wrath**; these are ZIP distributed defaults, **not verified live price**. Validate against actual installed config before release.
- [ ] **CS-05 — Fix older payment flaws**: uploaded generic tier handler can deduct gold before validation; old action 100+ route and 200/201 expansion route can both write progression. Consolidate pre-validation, idempotent authorised purchase and post-success state change. Reject stale/forged actions and disabled feature settings **without charging**. Review transaction/error recovery before any character DB writes.
- [ ] **CS-06 — Resolve NPC identity**: ZIP/upstream SQL for entry 390011 calls NPC **Chronomancer Amberwind**, while owner reports installed **Chronomancer Vezrath**. Record existing world DB/name/spawn/script binding **read-only when approved**; preserve existing NPC and placement, do NOT overwrite SQL blindly or spawn duplicates.
- [ ] **CS-07 — Integrate shared IP policy API**: Character Services should process the purchase, but IP should authoritatively decide earned prerequisite and paid expansion access. Avoid two copies of eligibility logic; make dependencies, version guards and fallback behaviour explicit.
- [ ] **CS-08 — Build and test**: baseline Core + IP + Character Services + Playerbots + Naxx; NPC visibility, human vs bots, both prices, account catch-up policy, failed charges, race/GM exceptions, login notice, no skipping raid tiers, SQL upgrade and rollback.
- [ ] **CS-09 — Document ownership and upstream maintenance**: fork README, `CUSTOM_MODIFICATIONS_REGISTER.md` or equivalent for the Character Services fork, changelog, upstream updates/merge guidance and dependency matrix; link this task list.
- [ ] **CS-10 — Get approval before merging/installing**: owner's fork/branch review, backups, staged test and rollback plan. Never replace the installed module or NPC automatically.

**Historical ZIP provenance:** The uploaded `mod-character-services.zip` has 23 actual files under one module directory (directory entries excluded); two changed tracked file blobs relative to checked [Grimfeather upstream](https://github.com/Grimfeather/mod-character-services). See the precise blob IDs and preservation issues in [coordination #18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18). Treat that public evidence as source comparison only; do not upload private operational configs or database dumps into GitHub.

## 5. Cross-dependency and task ownership

| Owner / contact | Responsibility | Links / current boundary |
| --- | --- | --- |
| **CosmicCuddle** | Final rule/pricing/grandfathering decision; approve staged testing, merges and live changes. | [Coordination #18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18) records proposed expansion rule and open decisions. |
| **CS01-IP Maintenance** | Integrate IP changes, protect custom code, coordinate Character Services fork, dependency matrix, source fixes where assigned, test/release gates. | This document + [custom register](CUSTOM_MODIFICATIONS_REGISTER.md). |
| **CS03-IP Claude — IP-only developer** | Code separately claimed IP work; post exact commits/tests/limits; not the separate Naxx Claude. No automatically active assignment. | [Coordination #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13). Last confirmed delivery #19 frozen; no new assigned work should be invented. |
| **CS02-IP Grok / owner-approved replacement** | Independent review of frozen IP #19. Neither a previously completed scanner review nor CS01 self-review replaces this. | [Coordination #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11); exact review verdict outstanding at first draft. |
| **Server-Side-Information** | Preserve exact gameplay rules/history: final requirements, commands, account catch-up, special races, era definitions, original Vezrath service decisions. | [Coordination #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10), [#14](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/14), [#18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18). |
| **Separate Naxx CS03/Claude and other developers** | Continue Mod-Naxxramas-Core P1–P4 and existing F14/shared feature work in their own repository. Do not duplicate without an explicit owner-coordinated handoff. | [Naxx #143](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/issues/143), [Naxx master #28](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/issues/28). |

### Dependencies that can invalidate a seemingly successful build

- Era grouping currently treats **stage 13 as Wrath band**, while new purchase requirements treat stage 13 as the **earned final TBC milestone with Wrath still unpaid**. **Do not release both rules together unexamined.** Choose a distinct paid-era classification or equivalent reviewed entitlement-aware policy.
- IP's existing XP/teleport/phase checks use stage thresholds; a server-side Character Services menu change **alone** cannot enforce the paid lock.
- Changing IP rewarded quest markers may conflict with PB `ResetQuests` and `ForceUpdateProgressionState`; inspect actual persisted state before migrating.
- Different `BotAccountsRegex` and PB random-account prefixes may undermine apparent identity guarantees on nondefault configs.
- Character Services' gold debit and IP's quest advancement must be correct under restarts/double-clicks; verify persistence/atomicity rather than assuming `ModifyMoney` is a database transaction.
- Client `/reload` may not invoke `OnPlayerLogin`. An addon messaging trigger must be explicitly verified.
- The fork may use a different AzerothCore source version and modules from any earlier CI; inspect actual installed state, DB schema and DBC/client requirements before staging.

## 6. Suggested execution sequence — keep delivery moving

**Now (no live changes; release priorities remain first):**
- [ ] Keep P1–P4 Mod-Naxxramas-Core functional completion moving; no IP fork tasklist should imply raiding is delayed until a *new* expansion-service feature ships.
- [x] Establish this IP master task list as a DRAFT documentation change, with links to current PRs, current known risks and owner decisions. **Mark a merged version separately after owner review; this checked line means authoring only.**
- [ ] Check Information's reply on [#18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18); acknowledge it in the same issue before closing or revising rules.
- [ ] Assign one **bounded actual coding task** to CS03-IP when available (not auto-assigned here), with no overlap with in-progress P1–P4 or Grok review.

**Next development milestone:**
- [ ] Resolve outstanding IP-05 pre-admission data-loss path; complete #19 independent review; assemble final IP-01/02/03/04/selected F14 actual-source candidate and test against installed versions.
- [ ] Create the real Character Services fork; preserve ZIP as a separate reviewable import and prepare safer reconciled code.
- [ ] Implement IP-11 earned/purchased policy and Character Services CS-05/06/07 shared purchase path together; ensure both expansions have identical safety contracts.

**Before any installation:**
- [ ] Complete human/bot matrix, expansion-gate and financial consistency tests; test owner-staged progression including Kel'Thuzad, final TBC gate, login, payment, relog and XP/instance access.
- [ ] Record exact build commits, final configuration, SQL/DBC/client dependencies, backed-up character data, rollback/recovery steps and test results in custom registers and release notes.
- [ ] Request **CosmicCuddle's approval** for any production merge/deployment/config enable/data change. Never silently force stages or withdraw existing paid access.

## 7. Maintenance template for every future task-list update

For each change, update the matching ID with:
- **Decision:** what CosmicCuddle approved, or precisely what remains undecided.
- **Code:** owner component, branch, PR, exact commit, affected source/config/SQL/DBC/addon paths.
- **Evidence:** source-only checks vs pinned native build vs actual staged gameplay vs verified live.
- **Dependencies/conflicts:** other modules, characters, group rules, bots, economy, DB and upstream.
- **Rollback:** revert path, backup/restore conditions and migration compensation.
- **Next action & owner:** one concrete item, not repeated investigation.

When the active conversation changes, link the latest [Maintenance handover](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/tree/main/handovers/Server-Side-Maintenance). Keep this checklist maintained on the reviewed IP development branch, with the Custom Modifications Register as the durable source-of-change record.
