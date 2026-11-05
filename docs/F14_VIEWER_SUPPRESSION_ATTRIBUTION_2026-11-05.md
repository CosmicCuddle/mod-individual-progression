# F14 per-viewer Core *source-attributed* suppression seam — compile-only pilot

**Owner:** CosmicCuddle. **CS01 Maintenance:** 5 November 2026.
**Parent:** F14 #83 \`1f71b99c2cfff79b2ad42019769da539c89ec61b\`.
**Status:** source-verified-only / CI-only Core C++ patching, DRAFT; **no active feature or live change**.

## Why this code is needed
A normal Core \`Player::UpdateVisibilityOf\` server link disappearance may come from phasing, distance, despawn, player state, GM visibility or many non-F14 reasons. It must NEVER be treated as a positively F14-caused hidden viewer link. The separate [per-viewer restore-ledger DRAFT #85](https://github.com/CosmicCuddle/mod-individual-progression/pull/85) correctly requires an explicit F14-only suppression source, but has no trustworthy input yet.

## Experimental code
\`tests/core_patches/apply_f14_viewer_suppression_attribution.py\` performs exact-anchor, all-or-nothing edits to only FOUR C++ files **in an ephemeral Core checkout**, after the pinned F14 #56/#67/#76/#77 patch stack. It adds:
- \`WorldObjectScript::OnF14ShouldSuppressCreatureForViewer(viewer, creature)\` with the **actual default \`false\`** and a no-listener-default-false \`ScriptMgr\` query. No IP, Naxx, PB or other code implements a positive listener.
- \`ScriptMgr::OnF14CreatureSuppressionDecision\` / \`WorldObjectScript::OnF14CreatureSuppressionDecision\`: telemetry of a positive **F14-specific** suppression decision, not general \`OnF14ViewerTargetLinkChanged\` events. The reported \`wasLinked\`/batched flag is **only server-side bookkeeping**. It cannot prove packet creation, transmission, reception, acknowledgement or a client's screen state.
- Both direct and batched \`Player::UpdateVisibilityOf\` paths now check stock \`CanSeeOrDetect\` exactly once and consult the extra default-false F14 query **only for a baseline-visible nonpet creature**. Thus a vanilla Core deny is not credited to F14; players, real human Blood Elves, Draenei, Altbots, pets, and all noncreature objects bypass this callback. Baseline Core \`CanSeeOrDetect\` and global interaction/aggro logic remain untouched.

## Safety gates
This experimental query **MUST NOT** return true on any deployed build until approved observer-specific rights and complete player combat/target/DoT/heal/threat/tap/quest/loot/kill-credit/party/ownership protections are implemented. A visual-only removal could create an invisible hostile mob interacting with a real Vanilla player. The current branch has **no caller** and deliberately never defines the \`COSMIC_F14_VISIBILITY_REFRESH_HOOK_VERSION\` full-ready macro.

Do **not** wire this source to #85's restore ledger yet. A "positive Core suppression decision" is not proof the 3.3.5a client hid the creature, and a later server visibility pass is not proof that an already-suppressed viewer got the creature back. We still need explicit server-link transition attribution, durable restore, overflow/map unload restoration and packet/actual client proof. No GM bypass assumptions; owner scope is selected verified BE/Draenei RNDBOT-only, humans and Altbots never hidden.

## Tests and operational status
Target CI composes newest pinned Grim Core \`0d754d19\` + #56/#67/#76/#77 exact patches, original F14 #83 IP, unchanged Playerbots \`e88cc409\`, Naxx #292 and Character Services #16, applies this narrow Core transformation and compiles/links the real disposable Worldserver. Source contracts check direct+batched paths, single baseline query, false default, only 4 Core files touched by patcher, original F14 registrations and OFF settings. **A green compile proves neither client delivery nor gameplay.**

**Rollback:** close the unmerged isolated DRAFT. No live host, server binary, config, SQL, DB, DBC, client or player records touched.
