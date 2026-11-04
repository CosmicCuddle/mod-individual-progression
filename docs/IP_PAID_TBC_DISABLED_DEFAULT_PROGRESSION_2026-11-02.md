# Paid TBC: refuse sale if default progression is disabled (2 Nov 2026)

**Owner:** CosmicCuddle. **Status:** separately scoped, default-OFF paid
feature draft. Parent: [strict paid-ledger IP PR #39](https://github.com/CosmicCuddle/mod-individual-progression/pull/39)
at exact `8c7d0532082e92f862a3bd715c7f08ad1289a0f0`.
No branch is merged, and no owner real character, gold or settings change.

## Why

The owner-final TBC timeline is KT40 earned raw7 -> Vezrath per-character
purchase (raw remains7) -> complete quest10259 Into the Breach to earn raw8.
The inherited `IndividualProgressionPlayer.cpp` rewards quest10259 into
stage8 **only when** `IndividualProgression.DisableDefaultProgression=0`.
With that setting set to 1, an otherwise legitimately paid player can
permanently lose the only nonrepeatable quest turn-in while staying raw7.

**Smallest safe temporary policy, review only:** inside the TBC-only
`CanPurchaseExpansion` preflight, treat the entry as unavailable when
`disableDefaultProgression` is true. The existing generic purchase
result is `EntryUnavailable`, displayed by the invoking Character
Services module's result UI. Both `PurchaseExpansion` and the Vezrath
menu already call `CanPurchaseExpansion`; thus a normal payment should
not proceed and no gold is taken.

Wrath has **no quest-reward dependency**, so this safeguard does not
reject Wrath purchases. Paid-expansion setting remains default OFF and
all established progression behaviour remains unchanged when OFF or
when `DisableDefaultProgression=0`.

## Release blockers and decisions

This is an explicit **conservative source candidate**, not an owner
approval that paid TBC must stay disabled when default IP progression
is disabled. An alternative would be a special paid-mode reward
exception, but such an exception needs owner review, more gameplay
tests and careful proof against bypassing established disabled-progression
rules. Keep fail-closed until approved.

No changes to paid receipt format, Core patch, SQL, vendor prices,
stage-marker quests, Character Services code, world quests or GM
commands. Test the ordinary sale UI and failed-purchase message,
default-off behaviour, existing paid buyers when configuration changes
after purchase, correct 10259 single reward and abort without gold
loss, Wrath unaffected. This **does not** solve missing first-time
receipt rows or the .pdump policy; those remain critical separate tasks.

**Rollback:** discard this child branch; no database rollback required
because the branch itself never changes a database. If deployed after
approval, changing a server option does not restore any past gold or
quest status. Verify restorable character database before live changes.


## 2 Nov — existing-paid buyer protection (source-only follow-up)

Grok identified a second stranding case: a character may buy TBC while default
progression is enabled, then the operator may enable
`IndividualProgression.DisableDefaultProgression=1` before that character
rewards the nonrepeatable quest 10259. The earlier sale-refusal check cannot
protect a purchase that already happened.

A new `MustHoldPaidTbcQuestReward` predicate now checks the **actual paid
character flag**, normal gated account, raw progression below stage 8, and
lack of an existing stage-8 start privilege. When the paid feature is active
and default progression is disabled, the quest-completion hook refuses the
quest **without consuming it or modifying money/stage/receipt**. Existing
TBC-starting races/classes, bots/excluded accounts, already stage8+, unpaid
players and paid-feature OFF retain their previous paths.

This is a draft **conservative no-data-loss safeguard**, not owner approval
of a gameplay policy for custom progression realms. The owner may later choose
a narrowly scoped special reward exception instead. It remains necessary to
test startup config, real paid character settings, quest log and staged
completion with DDP switching, as well as actual Core hook ordering; CI is
source/policy evidence only. Undo by reverting this draft's header/policy/hook
additions before merging. No host operation or payment has occurred.

## 2 Nov — separate packet-level defence-in-depth, not full reward protection

The pinned Core `QuestHandler.cpp::HandleQuestgiverChooseRewardOpcode` calls
`CanRewardQuest` followed by `RewardQuest` for an already-COMPLETE quest;
it does not call `CompleteQuest` (the PlayerScript veto) again. That is a
**separate, real remaining route** past the original paid DDP guard.

On an **isolated child of PR #41**, the new
`IndividualProgression_PaidTbcQuestRewardPacketGuard` checks the normal
`CMSG_QUESTGIVER_CHOOSE_REWARD` packet before Core processes it. It returns
false only for a verified *already-paid, stage<8, gated normal human lacking
stage8 start privilege*, when default progression is disabled, and only for
quest 10259. It leaves all other packets to Core, does not alter the received
packet, charge/refund money or mutate character/quest data. This is an
extra safeguard for **the normal client reward hand-in**, not an assertion
that command/script/server-internal `RewardQuest` calls are covered.

**Release blocker stays active:** the pinned Core supplies no general
pre-`RewardQuest` veto in its PlayerScript interface; all nonpacket paths,
thread/context safety and complete quest hand-ins need independent Core
review and compatible native build + disposable gameplay. No owner runtime,
config, SQL or production changes authorised. Parent #41 remains frozen
while its own native CI finishes. Rollback is discarding this unmerged child.

## 3 Nov — normal client hand-in denial explains preserved quest

The optional, default-OFF paid 10259 packet veto now sends a system message
*only* on the exact client reward packet that would otherwise silently be
dropped for a verified paid raw-stage<8 normal character with DDP=1.
It does not modify quest, stage, gold, receipt, packet cursor or unrelated
quest reward paths. Existing GM/direct Core RewardQuest bypass remains a
blocker pending an authoritative Core seam. Source and native CI must still
pass before owner-controlled gameplay acceptance. Rollback: revert this
small additive message/test revision on unmerged IP #42.

## 3 Nov — safe whole-ledger write before single save (isolated draft)

A genuine first-time TBC receipt must contain all eight v1 fields.
Pinned Core's `UpdatePlayerSetting` only grows a new vector through the
largest index written. The previous TBC purchase source wrote indices
0/3/4/5 and thus would produce six tokens on an initially absent vector,
which the strict login reader correctly rejects. The new purchase path,
after successful `RecordPurchase` and eligibility/refund safety checks,
writes the **entire already-validated** `stagedLedger.fields[0..7]` before
its one `SaveToDB`. Neither the paid bit nor previously evaluated state is
invented, and no second DB transaction or price alteration is added.

Importantly, this does **not** enable a first purchase for a character whose
paid-source row is genuinely MISSING. Current source still refuses such a
purchase before any money is taken. Whether owner-approved commissioning or
first-use empty rows are permitted is a separate pending decision. Existing
three-token/malformed/unknown rows must never be overwritten or treated as
unpaid. All paid switches remain OFF. The portable test simulates Core
vector resizing and compiles the real v1 decoder, but a source-matched
Worldserver plus disposable database relog/rollback acceptance remain
mandatory before live use.
