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
