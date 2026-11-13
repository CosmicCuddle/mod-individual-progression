# F14 v4 source readiness — pinned evidence

Owner CosmicCuddle; [coordination #51 v4 queue](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6858091821). This note is a moving source audit. It does not assert an installed binary, merge, runtime activation or gameplay result. F14/Paid stay OFF. The Altbot/excluded-account temporary group-floor population is unresolved in [Information #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10#issuecomment-6858551237); no runtime population is selected here.

## 1. Current candidate graph (GitHub heads checked 13 November)

| Candidate | Parent → current HEAD | Changed-path class |
| --- | --- | --- |
| IP [#174](https://github.com/CosmicCuddle/mod-individual-progression/pull/174) | `aed05a92f262362608f5d643c8570b504189aae0` → `3bf97b4d6edc746cb0ff7e7e02a55d6ab9fc5b32` | Register, pure pass header, note, C++/Node tests |
| IP [#175](https://github.com/CosmicCuddle/mod-individual-progression/pull/175) | `aed05a92` → `005d8949334b11c7c985225d6aa9a1cc120157a1` | Register, pure floor header, note, C++/Node tests |
| IP [#176](https://github.com/CosmicCuddle/mod-individual-progression/pull/176) | #175 `005d8949` → `971ad2ce160e29a8c426d2650e1d665c56e2ee01` | Register, pure consumer header, note, C++/Node tests |
| IP [#177](https://github.com/CosmicCuddle/mod-individual-progression/pull/177) | #174 `3bf97b4d` → `49af2465898e318481e9ad9de17699aea6f9338d` | Register, test-only witness/note/Node contract |
| IP [#178](https://github.com/CosmicCuddle/mod-individual-progression/pull/178) | `aed05a92` → `701c4a41b3034731eb9916965bccc301b6172345` | Register, offline preflight/test/docs/example |
| Claude social S1 | `aed05a92` → `146dbdfccedbf1e4096f158502a5a7aff7a65117` | Config, IP runtime, new social policy/script, Register, docs/tests/workflow; branch is moving |
| Claude T1 child candidate | social `146dbdf` → `34bdc7524bcf947bd8fc551a44a6ead3847db50e` | Separate, unmerged T1 guard/world-thread marker source, tests, docs/workflow |
| F15 [#310](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/310) | `827f5618c991611c483941cabb0ba910fee09180` → `044fde67fe5b56acfae050b9188fc50e5cac14b1` | Separate Mod-Naxxramas-Core offline staging preflight/docs/example/tests |
| F15 [#311](https://github.com/CosmicCuddle/Mod-Naxxramas-Core/pull/311) | #310 `044fde67` → `9495c5ec41e481b4088d0b687e593eb492a6426f` | Separate F15 Register, staging backup/rollback template/test |

The five Codex IP candidate **deltas** and Claude social all touch `docs/CUSTOM_MODIFICATIONS_REGISTER.md`, and no other exact path overlaps. #176 inherits #175's floor header; #177 inherits #174's pass header. Claude social changes `src/IndividualProgression.cpp`, `.h`, `src/IndividualProgressionPlayer.cpp` and `conf/individualProgression.conf.dist`; a future runtime group adapter could overlap those paths, but the current pure #175/#176 do not. F15 is a separate repository and shares no IP path. This is a path matrix, not semantic compatibility proof.

| Pair class | Exact changed-path intersection | Semantic relationship |
| --- | --- | --- |
| Any two of #174–#178 | Register only | #176 is a child of #175; #177 is a child of #174; sibling Register rows need reconciliation |
| Claude social vs each #174–#178 | Register only | Future runtime IP/group source overlap is a design dependency; Claude branch must not be edited |
| F15 #310/#311 vs IP candidates | None (different repositories) | #311 is a child of #310; evidence manifests and source pins must distinguish repos |

Stale v3 references corrected: Claude advanced from `d8a43a77` through `7b343a22`, S1 `cb23a099`, `021be3af` and current social `146dbdf` (workflow path allowlist only since `021be3af`); #176/#177/#178 advanced to the heads above. Claude's T1 `34bdc752` is a **child candidate**, not part of the social branch. Re-read [Claude #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13) and the branch ref immediately before any actual rehearsal, because S1/T1 is active. The older `d8a43a77` remains a historical ref, not the present social input.

## 2. #177 exact-head standalone native recipe

Run only from an exact checkout of `49af2465898e318481e9ad9de17699aea6f9338d` (or a verified file-identical source snapshot). This is a **recipe, not a claimed run in this workspace**; no GCC/Clang C++ compiler is available locally. It needs no server, database, network package install or hosted Actions.

```sh
test "$(git rev-parse HEAD)" = 49af2465898e318481e9ad9de17699aea6f9338d
node tests/test_f14_negative_integration_source.js
for compiler in g++ clang++; do
  for standard in c++17 c++20; do
    for mode in normal ndebug; do
      flags=""
      if test "$mode" = ndebug; then flags=-DNDEBUG; fi
      "$compiler" -std="$standard" -Wall -Wextra -Werror -pedantic $flags -Isrc \
        tests/f14_negative_integration_witness.cpp -o /tmp/f14-negative-witness
      /tmp/f14-negative-witness
    done
  done
done
```

Expected successful output: Node prints `PASS F14 negative integration source: exact ephemeral evidence, raw-GUID debt, no completion`; each of eight binaries prints `PASS F14 negative composition: pass/correlation loss retains restore debt`. Any compiler/test nonzero exit is a failure. [Claude's independent prior matrix](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6857731743) passed 8/8 at `b75cabf7`, not at this head. Between `b75cabf7` and `49af2465`, only the witness C++ test, Node source contract and its documentation changed (72 insertions, one deletion); the inherited pass header blob remains `e95477c4378e3364d580b53108abfc3beb9a8e2d` at both refs. The new tests cover send/handoff/debt routes and G2's nominated send before `ObserveBatch`. Structural similarity does **not** establish exact-head native PASS. Full Worldserver link, Core callback production and gameplay remain separate.

## 3. #178 robustness challenge

At `e75e2fe7` the existing local offline suite passed **7/7**, covering duplicate JSON keys, symlink escape, case collisions, main/linked/detached checkouts, shallow metadata, multiple remote names, unsupplied submodules, missing evidence, index-byte preservation, unknown fields and refs without a repo path. A concrete gap was a missing promised blob in a partial clone. #178 advanced test/docs only to `701c4a41b3034731eb9916965bccc301b6172345`: its disposable fixture removes one header blob after marking a nonexistent **local** remote promisor. Evidence needing that object remains `UNKNOWN`; the blob is not recreated. `node --test tests/test_module_sync_preflight.js` **8/8 PASS** and `git diff --check` PASS. The fixture does not prove every Git version/remote helper; an installed inventory remains unknown.

## 4. #176 producer-contract proof table v2

`AVAILABLE` means a source fact exists for its narrow context, not that #176 can consume it. `INSUFFICIENT` means a producer exists but fails the required proof. `MISSING` means no issuer is in the examined candidate. `UNKNOWN` means runtime ordering or installed source is not evidenced. See the [detailed producer audit](F14_GROUP_GENERATION_PRODUCER_AUDIT_2026-11-13.md).

| Required fact | Candidate producer → actual hook | Monotonicity / reset | Thread or lock owner | Verdict |
| --- | --- | --- | --- | --- |
| `sessionEpoch` | IP `_f14NextRandomBotLoginEpoch`, login entry invalidation and logout | Strict process-local increase, refuses uint64 overflow; resets at restart; selected verified RandomBot login only | `_f14GroupCacheLock` protects cache; cross-callback owner and Altbot/excluded eligibility unproven | **INSUFFICIENT** for #176; narrow producer AVAILABLE |
| `mapGeneration` | Experimental IP `CurrentCombatActor` issues `CombatMapIssuer()` only when actor is observed after map change | Global issuer increases until exhaustion but lazy observation misses unobserved A→B→A | Marker mutex exists; complete transfer/queue proof absent | **INSUFFICIENT** |
| `membershipGeneration` | Core #25 `GroupScript::OnAddMember/OnRemoveMember/OnDisband` | Events exist; no per-bot/global issuer or old-context capture. Per-group counter fails A→B | Synchronous dispatch at call site; full callers/thread owner unknown | **MISSING** issuer |
| `leaderRevision` | Core #25 `OnChangeLeader` after change, possibly during removal | No per-bot/global issuer; away→back must still increase | Callback sequence/owner across bot actions unproven | **MISSING** issuer |
| Group-era change, same leader | Existing IP `SyncBotsProgressionToLeader` reads leader stage on add | Stage/rights can change without leader GUID change; no #176 revision event/immutable snapshot | Existing sync may write progression; temporary era must not enter it | **MISSING** event/version contract |
| Process restart | Process-local issuers and tracker reconstructed | Old in-process callbacks terminate, but persisted/external actions and new GUID namespace unproven | No durable incarnation/queue fence evidenced | **UNKNOWN** cross-process provenance |
| Bot/group GUID reuse | Core/PB lifecycle | GUID equality alone does not order reuse; non-reusing session/membership versions needed | Recycle policy/queue drain/installed data unknown | **UNKNOWN** |

The temporary floor **population** is a separate policy dependency. [Information #10](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/10#issuecomment-6858551237) must distinguish verified RandomBots, Altbots, excluded non-bot accounts and ordinary humans. Claude's reported owner statement on Altbots does not settle the older group-sync rule; CS01 explicitly blocked selection pending #10. Social isolation remains verified RandomBot-only. No race-only classification or runtime `eligibleBot` selection is made here.

## 5. Tombstone retirement proof research

#176 retains one slot per observed **or blindly revoked** bot GUID for its lifetime. Hard refusal at 64 distinct GUIDs is a safe finite prototype, not a sustained service design. The question is whether an old callback could still reach the same tracker after a tombstone is forgotten.

| Boundary | Delayed callback / GUID rebound | Barrier and owner proof | Safe per-bot retirement now? |
| --- | --- | --- | --- |
| Bot logout | IP invalidates identity; pending group/map/PB AI work not shown drained; GUID may log in again | No join/cancel barrier across logout/map/group | **No proof** |
| Session destruction | Ends connection, not necessarily retained in-world Player/pending work | No queue drain or epoch fence | **No proof** |
| Map removal | Experimental pre-removal event; unobserved A→B→A and queued leave possible | No all-map barrier/ownership transfer proof | **No proof** |
| Group disband/destruction | Core remove/disband hooks; delayed captured event may outlive group | No issuer or callback quiescence proof | **No proof** |
| RandomBot deletion/recreation | PB `currentBots` may remove/add low GUID | No in-flight AI cancellation or durable GUID incarnation | **No proof** |
| Server shutdown | In-process callbacks and tracker memory terminate together | Whole process ends; does not justify retirement while running | **Whole-tracker lifetime ends only** |
| Process restart | Fresh tracker lacks prior tombstones; old process tasks cannot execute in it | Persisted external action not inventoried | **Not a live per-bot retirement rule** |

Until an owner/queue barrier is proven, keep hard saturation and fail to earned-only/no temporary override at capacity. Plan for peak **distinct GUIDs per tracker lifetime**, not concurrent group size, including blind revokes. Raising capacity postpones exhaustion; timeout/LRU does not prove safety.

## 6. S1 social group-member scan observer

At Claude social `146dbdf`, `OnPlayerCanGroupAccept` checks the leader first, then `AllowGroupMembers` only when the accepting player satisfies `HumanStillVanilla`. It iterates `Group::GetMemberSlots`, skips the accepting human/leader, resolves each member with `ObjectAccessor::FindPlayer`, skips unresolved/offline members, and calls `ReadBot` rather than classifying a slot by race. `ReadBot` requires the existing verified RandomBot cache plus headless/non-GM/race policy. The result uses the existing rate-limited human-to-bot reply path; there is no new world-concealment path. `OptionOn()` is an early guard, and the no-Playerbots build is documented to force the option OFF. Core `GroupScript` and member-slot APIs are source-present at the pinned Core restack. CS01's [independent S1 source review](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6858478911) passed at `cb23a099`; the executable social script diff from that commit through `146dbdf` changes only an explanatory T1 comment. Thus the narrow S1 member-scan logic remains source-equivalent.

**Verdict: PASS WITH EXISTING RUNTIME CONDITIONS for narrow S1 source behavior.** Skipping offline/unresolved members is a fail-open stock choice, not proof that a restricted bot is absent. Core member-slot iteration and retained `Player*` values across map threads lack a proved lifetime/lock contract in this audit. Claude's T1 finding shows bot AI can call the handler on its map thread and read a human on another map; its separate child candidate `34bdc752` is under review. In that child, `MayReadPair` reads the other player's `FindMap()` before its fail-open decision; Core25's `FindMap()` returns a plain `Map*` with no synchronization shown. [I raised that ownership proof gap in #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6859588519). [A subsequent correction](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6859632602) retracts my initial `atomic<std::thread::id>` portability objection because C++17 requires `thread::id` to be trivially copyable. The special Playerbots `BattleGroundJoinAction::group->AddMember` route bypasses invite/accept hooks and remains outside this scan. No source assertion here upgrades S1 to complete group enforcement or gameplay PASS. The social branch remains Claude-owned and was not edited.

## 7. Shared value-only identity contract proposal

Share **evidence provenance and invalidation**, not one universal eligibility Boolean. A source-owning producer would copy `ActorEvidence {fullPlayerGuid, accountId, loginEpoch, processIncarnation, sourcePin, capturedAtGeneration, inWorld, connected, headless, gm, positivelyVerifiedRandomBot, positivelyVerifiedAltbot, classificationKnown}` under its verified lifecycle owner; all values are immutable after publication. The `sourcePin` identifies the installed Playerbots/IP/Core implementation that established the predicates, not the mere Git HEAD of a development checkout. A zero epoch, exhausted issuer, missing PB header, disconnected session, contradictory flags or stale capture yields `UNKNOWN`. Neither race nor bot-account regex can set `positivelyVerifiedRandomBot` or `positivelyVerifiedAltbot` by itself. A future Altbot proof needs its own positively sourced field rather than `!isNormalAccount` being silently relabeled as bot identity.

| Consumer | Additional facts and allowed predicate | UNKNOWN behavior |
| --- | --- | --- |
| Claude social and social `/who` | Positive verified RandomBot + headless + non-GM + race10/11 actor; ordinary human counterpart with separately proven raw stage <8 | Stock allow/include; never conceal or restrict from race/account alone |
| #176 temporary group era | Policy-selected group-sync population **pending #10**, event-time earned stage, exact session/map/group/membership/leader tuple | No temporary override; exact captured teardown may only remove its own old context |
| Future WHO F15/player visibility | Positive actor classification plus viewer rights/epoch and current source hook; social WHO remains separate from world concealment | Include/visible when uncertain, unless a separately approved policy says otherwise |
| Future auction browse | Buyer capability snapshot captured before worker enqueue, immutable auction tag and policy version; no live Player/DB/ScriptMgr on worker | Explicit policy choice needed for stale/unknown snapshot/tag; never infer unrestricted from missing version |

Invalidate or advance the lease on every login (before republish), logout, retained-Player session rebind/disconnect, positive PB selection change, config generation change and GUID reuse; map-bound facts need their own complete transfer generation. Publication and revocation must share one lock/owner so a reader cannot combine identity from one epoch with stage/map/group from another. A worker receives only a copied value and its version; it cannot call `IsRandomBot` or dereference `Player*` after enqueue. Persistent stage, entitlement and restoration debt retain their own authorities and are never derived from a temporary identity snapshot. This is an interface design, not a new runtime producer or policy decision.

## 8. Offline composition preflight

`node tests/f14_composition_preflight.js --manifest docs/F14_REGISTER_RECONCILIATION_MANIFEST_2026-11-13.json` inspected only pinned local Git objects; its `git` subprocesses allow `diff`/`show`/`rev-parse`, disable optional locks, lazy fetch, fsmonitor and prompts, and never apply patches. The manifest labels Claude social paths/defaults as **externally supplied metadata**, not locally verified source. The tool requires the local exact candidate heads and their parent ancestry through the Register inspector. It reports changed paths, Register overlap, executable runtime overlap, changed config values, literal SHA pins in source contracts, and non-base parent-tree requirements. Its focused Node suite is **3/3 PASS**; combined with Register tests below, **8/8 PASS**.

At the pinned inputs, all 15 candidate pairs intersect only on `docs/CUSTOM_MODIFICATIONS_REGISTER.md`; executable runtime intersections and duplicate stable IDs among locally verified rows are empty. No local Codex candidate changes `conf/individualProgression.conf.dist`. #176 needs #175's parent tree, and #177 needs #174's parent tree. Claude's externally supplied config defaults are isolation `0` and reply cooldown `60`; the local tool cannot verify the moving external branch, so that result must be refreshed at use time. Empty literal SHA lists are **not** proof that source contracts remain valid after composition. File-path disjointness is not an API/ABI, thread, lock or gameplay compatibility result. This preflight intentionally creates no shared integration branch, merge, rebase or fetched ref.

## 9. Register reconciliation and remaining rows

`node tests/f14_register_reconcile.js --manifest docs/F14_REGISTER_RECONCILIATION_MANIFEST_2026-11-13.json` compares each candidate's Register blob to its exact parent. All five add one distinct `IP-*` row; none removes or modifies an earlier section, and the local added IDs are unique. The expanded validator checks owner, owning component, intended behavior, affected paths, parent/source reference, tests, dependencies, rollback, explicit non-live status, contradictory live claims and a claimed current/exact HEAD. Its focused tests are **5/5 PASS**. These are text-presence/source checks, not semantic proof of every field.

Two rows remain incomplete in the already-reviewed candidate heads: #174 lacks an explicit **Owning component** field; #175 lacks an explicit **Parent/source reference** in its row. Their supporting notes and PR graph supply that context, but the Register itself needs a later additive correction in a controlled composition or follow-up. Rewriting either reviewed PR head merely for a row would invalidate exact-head review evidence. Claude's social row is represented by a supplied heading only; it has no stable `IP-*` feature ID in that heading. Its full blob is not in the local graph, so the other Claude fields are **UNKNOWN**, not assumed missing. Coordinate a stable ID with Claude/CS01 before Register composition. No live-status claim is upgraded.

## 10. F15 #310/#311 cross-repository audit

F15 lives in `Mod-Naxxramas-Core`, with #310 parent `827f5618` and exact head `044fde67`, and #311 child exact head `9495c5ec`. Its offline #310 preflight consumes operator-supplied `status`/`value`/`source` envelopes, requires VERIFIED host facts, allows PROPOSED scheduling only, leaves the shipped example UNKNOWN, and rejects self-authorization. The #311 template keeps active logs outside retention, requires copy before approved overwrite, preserves diagnostic evidence through rollback, and separates file staging, manual observation, synthetic snapshot/retention rehearsals, and scheduled hook integration. Neither PR supplies host evidence or authorizes installation. This audit read exact GitHub source/docs only; it did not run their Python tests or inspect the owner host.

The IP composition manifest is **repository-scoped**. F15 can be represented safely only as a separate `{repository, parent, HEAD, source}` entry and compared at a cross-repository evidence layer; reusing an IP branch/ref or treating same-looking path names as one Git tree would be false. A checkout revision is not a running World/Auth binary fingerprint. #310 asks for both build-version and source-revision sources, while the #311 template also requires exact installed-file hashes, original metadata, free space and rollback owner. None is available from these source commits. The F15 register's #311 row says only “DRAFT #311; source-only planning branch” rather than its current exact SHA, so that row needs a pin at handback. The preflight checks supplied provenance syntax, not whether an operator's evidence is authentic; the human review remains necessary. One narrow source/documentation mismatch to review: `PurePosixPath` normalizes repeated separators, so the implementation appears to accept an input like `/root//cosmic_core/logs/x` even though the note claims all daemon paths are normalized. This needs a focused source test before declaring a defect or changing CS01-owned code.

## 11. Auction #53 policy alternatives, no selection

The [Maintenance source answer](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/53#issuecomment-6854601250) and [test oracle](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/53#issuecomment-6858032584) remain the basis: capture a viewer value before worker enqueue, tag an auction before publication, run a pure comparison in normal/default browse and `getAll`, and leave owner/bidder management separate. Bid/buyout must revalidate current eligibility. Information/owner must choose the following policy details before implementation:

| Choice | Option A | Option B | Consequence to decide |
| --- | --- | --- | --- |
| Encoding | Core-managed requirement/capability mask with allocated namespaces and **all required bits** semantics | Small ordered class with a single versioned owner of the classification | Mask supports independent rules if allocation/combination is explicit; class is simpler but risks collapsing independent restrictions |
| Multiple providers | Compose immutable requirements by union, viewer grants by independently justified grant sources | Assign one authoritative aggregator that emits the final tag/snapshot | Never let a later provider clear another provider's restriction silently; conflicts need an explicit error/UNKNOWN state |
| Unknown/stale tag or viewer version | Hide from ordinary browse and reject purchase until republished/reissued | Return an explicit search error and require refresh/rebuild | Either preserves the no-leak intent but affects availability; choose failure behavior and observability together |
| Retag after policy/config/source change | Rebuild all searchable copies before serving under new version | Maintain two versions during a bounded drain, comparing only matching pairs | Requires a proven queue/barrier and startup-loaded auction coverage; version mismatch cannot mean unrestricted |
| Owner in ordinary browse | Apply buyer eligibility to everyone including owner | Exempt owner in ordinary browse | Owner-list always preserves management; Information's answer determines ordinary browse behavior |

No auction source was edited. These options are a decision request for Information/owner, not a covert selection of gameplay policy. No API or core branch is proposed for merge on this evidence.

## 12. Handback and proposed v5 queue

Current ready evidence is source-only: exact graph, #177 recipe, #178 offline robustness, #176 producer/tombstone limits, S1 observer, identity design, composition/Register validators, F15 cross-repo and auction decisions. #176's independent native exact-head result is [CS01 8/8](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6858618370); the local environment cannot compile #177 or a full Worldserver. No source composition, runtime F14/paid activation, positive concealment, merged code, production deployment or gameplay test is claimed.

Safe v5 follow-through, in dependency order:

1. Refresh moving Claude social, Core and PB heads and compare them against the pins above.
2. Ask Claude/CS01 to stabilize the social Register feature ID and reconcile the two Codex Register field gaps in a future integration Register.
3. Obtain an independent **exact #177 HEAD** native GCC/Clang C++17/20 normal/NDEBUG result using the recipe, with stdout and compiler versions.
4. Run a disposable local source-composition rehearsal only within [CS01's authorized boundary](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/51#issuecomment-6858470185), rechecking all inputs immediately before, discarding the tree after evidence capture and publishing no composed branch.
5. Probe the F15 repeated-separator normalization claim with a source-only test and send a narrow result to CS01; do not edit F15-owned source.
6. Keep #176 runtime wiring parked until a complete event-time session/map/membership/leader issuer, ownership/barrier, population decision in Information #10 and tombstone retirement plan are proven.
7. Keep the social T1 cross-map thread fix and Playerbots direct-add route with Claude/CS01; review the `FindMap()` ownership proof in [Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6859588519) without editing their branch.
8. Request Information's explicit auction encoding, multi-provider, unknown/version and owner ordinary-browse answers in Issue #53 before any Core implementation.
9. Carry forward F15 #310/#311 only as separate repository evidence, and distinguish operator attestations from binary/host proof.
10. Prepare an owner-review integration checklist with exact HEADs, native/full build and gameplay gates, backup/rollback and F14/Paid OFF conditions. No deployment or enablement is implied.
