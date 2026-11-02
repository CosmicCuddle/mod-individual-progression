# IP paid-era TBC quest crossing — isolated implementation slice (2 November 2026)

**Owner:** CosmicCuddle. **Status:** DRAFT / unmerged / OFF by default / **NOT READY to enable**. Child of [IP #32](https://github.com/CosmicCuddle/mod-individual-progression/pull/32) exact `6c180c0a78cdadcf6c79e8e627f2cc2e91e0fdae`. Final owner specification: [Coordination Issue #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6566353358); supersedes all earlier 'buy TBC immediately moves to 8' instructions.

## Intended owner-visible behavior

1. Normal character completes KT40 to earn raw **7** and eligibility to purchase TBC passage from Chronomancer Vezrath.
2. **TBC purchase does NOT grant stage 8:** debit once, set per-character TBC entitlement and future durable receipt, **remain at raw/effective stage7**. Expose quest **10259 Into the Breach** only after buying. After finishing that quest, award stage8/Outland and lift level-60 restriction.
3. Wrath remains distinct: Kil'jaeden earns raw13 but unpaid effective12; payment opens effective13/Northrend immediately, **no Wrath entry quest and no stage14**.
4. Playerbots/excluded accounts, historic start privileges (DK/TBC races), GM overrides and disabled paid mode keep owner-approved exemptions. Never change original stage marker meanings.

## Exact implementation in this slice

- `src/IndividualProgressionExpansionPolicy.h`: `PurchaseGrantStage` returns 0 for both purchases; `MaxServiceGrantableStage` caps a paid normal-human **raw7** character at **7** so the separate Character Services catch-up tier service cannot sell a stage8 skip. Raw stages below7 retain a 6 ceiling; after completing 10259 at raw8 the within-era service ceiling remains12. Privileged start>=8, non-normal accounts and feature OFF retain previous service rules.
- `src/IndividualProgressionExpansion.cpp`: remove automatic `UpdateProgressionState(8)` on purchase; **preflight both quest templates** (hidden marker 66008 and actual Into the Breach 10259) before money changes, and **refuse an already-rewarded 10259** until a reviewed recovery solution exists, so the character cannot be charged and stranded. After debiting and recording TBC entitlement verify **unchanged raw progression and cached paid TBC flag**, rather than falsely requiring a new raw8 quest marker. Keep existing `SaveToDB(false,false)` and refund/flag revert on failure; Wrath effective13 check remains.
- `tests/expansion_purchase_policy.cpp`: update portable pure-policy tests and labelled purchase model for bought-but-still-raw7, no stage8 skip, quest hand-in model, service cap, feature OFF, bot/start exceptions, and unchanged Wrath.
- `tests/test_ip_paid_expansion_source.py`: verify runtime purchase no longer calls progression writer, ordering remains validate→debit→flag→verify→refund/save, and the new policy contains explicit no-stage/skip guard.
- `docs/IP_PAID_EXPANSION_ENTITLEMENT_2026-11-01.md` and `docs/CUSTOM_MODIFICATIONS_REGISTER.md`: correct prior stale design wording, record present and future blockers.

## NOT DONE — release blockers, do not enable TBC paid mode yet

1. **Quest availability:** IP currently has no condition to hide/refuse quest **10259** for unpaid normal characters. Must verify Core stock world quest giver/ender, repeatability, actual quest/party-share/reward hooks and ConditionScript behavior first. Grok has [independent read-only hook review assigned in Issue #11](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/11#issuecomment-6566397071); Information will check source/DB intent in [Issue #18](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/18#issuecomment-6566398122). No speculative SQL or live DB query.
2. **Previously rewarded 10259:** characters who completed 10259 unpaid before this feature may have the quest permanently rewarded at stage7. This child currently **refuses the TBC sale before debit** if 10259 was already rewarded, preventing lost money but **not resolving their progression**. Need owner-compatible targeted recovery (verify actual reward first; no blind quest reset/bulk progression write) and staging tests before enablement.
3. **DisableDefaultProgression:** when true the current 10259 reward hook intentionally does not advance. Decide how to preserve the owner-requested TBC crossing with that config without unexpectedly re-enabling unrelated default progression.
4. **Entitlement persistence/receipt:** Claude [Issue #13](https://github.com/CosmicCuddle/Naxxramas-Server-Coordination/issues/13#issuecomment-6566397602) owns exactly-once payment receipt, malformed settings strict parser, missing-row protection, same SaveToDB transaction and transfer refusal/reconciliation. A paid raw7 may lose the quest on a character dump because Core does not include character_settings.
5. **World condition regression:** quest66013 is earned KJ, **not paid Wrath**; existing direct SQL conditions may expose unpaid content, requiring per-row review, **never a mass replace**.
6. **Native/hosted/staging:** no exact-head hosted test, full pinned Core/Playerbots/Character Services Worldserver link, actual installed source/config, real 10259 accept/share/turn-in, money, storage, backup/restore, server or deployment verification has been established by this source edit. Feature stays OFF.

## Tests and release/rollback

- Inspect exact branch diff and build GCC+Clang C++17/C++20 pure policy; execute existing Python source tests at HEAD, then pinned native Worldserver+Character Services integration.
- After all blockers corrected, with an owner-approved isolated staging realm and restorable backups, test raw6 purchase refusal; raw7 unpaid quest hidden and unshareable; paid raw7 stays7, service 8 unavailable and quest visible; quest turn-in advances8; pre-rewarded quest recovery; repeated purchase no extra debit; raw13 unpaid/effective12 then Wrath purchase effective13/no14; bots, races, GM, OFF, DisableDefaultProgression; relog, reload, transfer fail-closed and error rollback.
- Rollback in source: discard this **unmerged child** to restore #32; no database or live rollback applies. Do not merge/enable/install or perform owner DB/money/log operations without CosmicCuddle's separate explicit approval.

**Custom Modifications Register ID:** IP-PAID-TBC-QUEST-PURCHASE-SLICE-20261102.
