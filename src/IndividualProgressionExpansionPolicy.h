/*
 * CosmicCuddle Individual Progression — paid expansion entitlement policy.
 *
 * Pure, side-effect-free rules (no AzerothCore types) so they can be unit
 * tested on their own. IndividualProgression gathers the facts for one
 * character and asks this header what is allowed.
 *
 * Owner rule: earning the final milestone of an era makes a character
 * ELIGIBLE to buy the next expansion from Chronomancer Vezrath. Only the
 * purchase opens the next era. Neither step alone is enough.
 *
 * Existing stage meanings are NOT renumbered:
 *   7  = Kel'Thuzad 40 (final Vanilla milestone)
 *   8  = TBC entered (previously only from the Into the Breach quest)
 *   13 = Kil'jaeden (final TBC milestone); legacy code also treats 13 as
 *        "Wrath open", so an UNPAID 13 is capped to 12 for access checks
 *   14 = first Wrath raid tier completed (never granted by a purchase)
 */
#ifndef INDIVIDUAL_PROGRESSION_EXPANSION_POLICY_H
#define INDIVIDUAL_PROGRESSION_EXPANSION_POLICY_H

#include <cstdint>

namespace IndividualProgressionExpansionPolicy
{
    constexpr std::uint8_t VanillaFinalStage = 7;
    constexpr std::uint8_t TbcEntryStage = 8;
    constexpr std::uint8_t TbcFinalStage = 13;
    constexpr std::uint8_t WrathAccessStage = 13;
    constexpr std::uint8_t UnpaidWrathAccessCap = 12;
    constexpr std::uint8_t WrathFirstCompletionStage = 14;
    constexpr std::uint8_t LastSupportedStage = 18;

    // Values stored per character. 0 = not owned.
    constexpr std::uint32_t FlagNone = 0;
    constexpr std::uint32_t FlagPaid = 1;
    constexpr std::uint32_t FlagGrandfathered = 2;

    // Purchasable expansions. Raw values arrive from gossip actions, so any
    // other value is treated as a forged/invalid request.
    enum class Era : std::uint8_t
    {
        Tbc = 1,
        Wrath = 2
    };

    enum class PurchaseResult : std::uint8_t
    {
        Success,                   // caller may charge and commit
        FeatureInactive,           // feature OFF or store unavailable
        NotApplicable,             // bot/excluded account: established rules apply
        InvalidRequest,            // unknown era value
        AlreadyOwned,              // never charge twice
        NotEligible,               // final milestone of the previous era not earned
        BlockedByProgressionLimit, // server progression cap does not reach the era
        EntryUnavailable,          // persistence/marker template missing: fail closed
        InsufficientFunds
    };

    struct CharacterFacts
    {
        bool featureActive = false;        // config ON and character store available
        bool gated = false;                // normal (human) account
        std::uint8_t rawStage = 0;         // highest rewarded IP stage marker
        bool tbcPaid = false;              // stored TBC flag
        bool wrathPaid = false;            // stored Wrath flag (paid or grandfathered)
        std::uint8_t startStage = 0;       // configured start privilege (race/DK/server)
        std::int32_t progressionLimit = 0; // 0 = unlimited
    };

    constexpr bool Applies(CharacterFacts const& f)
    {
        return f.featureActive && f.gated;
    }

    constexpr bool HasTbc(CharacterFacts const& f)
    {
        return !Applies(f) || f.rawStage >= TbcEntryStage || f.tbcPaid || f.startStage >= TbcEntryStage;
    }

    // Stage 14+ means Wrath content was already completed, so access is
    // never revoked from such a character.
    constexpr bool HasWrath(CharacterFacts const& f)
    {
        return !Applies(f) || f.wrathPaid || f.rawStage >= WrathFirstCompletionStage ||
               f.startStage >= WrathAccessStage;
    }

    constexpr bool Owns(CharacterFacts const& f, Era era)
    {
        return era == Era::Tbc ? HasTbc(f) : HasWrath(f);
    }

    // The stage that access checks (XP, travel, phasing, spells, grouping
    // bands) must use. With the feature OFF it is always the raw stage.
    constexpr std::uint8_t EffectiveStage(CharacterFacts const& f)
    {
        if (Applies(f) && f.rawStage >= WrathAccessStage && !HasWrath(f))
            return UnpaidWrathAccessCap;
        return f.rawStage;
    }

    constexpr bool HasPassed(CharacterFacts const& f, std::uint8_t state)
    {
        return EffectiveStage(f) >= state;
    }

    constexpr bool IsEligible(CharacterFacts const& f, Era era)
    {
        if (!Applies(f))
            return false;
        if (era == Era::Tbc)
            return f.rawStage >= VanillaFinalStage && !HasTbc(f);
        return HasTbc(f) && f.rawStage >= TbcFinalStage && !HasWrath(f);
    }

    constexpr bool ShouldRemind(CharacterFacts const& f, Era era)
    {
        return IsEligible(f, era);
    }

    constexpr bool IsKnownEra(std::uint8_t value)
    {
        return value == static_cast<std::uint8_t>(Era::Tbc) || value == static_cast<std::uint8_t>(Era::Wrath);
    }

    // Lowest stage the server cap must allow before the era can be sold.
    constexpr std::uint8_t RequiredLimitFor(Era era)
    {
        return era == Era::Tbc ? TbcEntryStage : WrathFirstCompletionStage;
    }

    // Stage marker a purchase writes: TBC opens at 8; Wrath writes NO stage
    // (13 is already held and 14 would skip the first Wrath raid tier).
    constexpr std::uint8_t PurchaseGrantStage(Era era)
    {
        return era == Era::Tbc ? TbcEntryStage : 0;
    }

    // Every check happens before any money is taken. Order matters:
    // a repeated click reports AlreadyOwned, never a charge.
    constexpr PurchaseResult CheckPurchase(CharacterFacts const& f, std::uint8_t eraValue,
                                           bool hasEnoughMoney, bool entryAvailable)
    {
        if (!f.featureActive)
            return PurchaseResult::FeatureInactive;
        if (!f.gated)
            return PurchaseResult::NotApplicable;
        if (!IsKnownEra(eraValue))
            return PurchaseResult::InvalidRequest;

        Era const era = static_cast<Era>(eraValue);
        if (Owns(f, era))
            return PurchaseResult::AlreadyOwned;
        if (!IsEligible(f, era))
            return PurchaseResult::NotEligible;
        if (f.progressionLimit && f.progressionLimit < RequiredLimitFor(era))
            return PurchaseResult::BlockedByProgressionLimit;
        if (!entryAvailable)
            return PurchaseResult::EntryUnavailable;
        if (!hasEnoughMoney)
            return PurchaseResult::InsufficientFunds;
        return PurchaseResult::Success;
    }

    // Automatic progression (boss kills, achievements, quests, custom kills)
    // must not open an era the character has not paid for. The whole grant
    // is refused rather than partially applied, so a cross-era target can
    // never award the final milestone of the current era as a side effect.
    constexpr bool AutomaticGrantAllowed(CharacterFacts const& f, std::uint8_t targetStage)
    {
        if (!Applies(f))
            return true;
        if (targetStage >= WrathFirstCompletionStage && !HasWrath(f))
            return false;
        if (targetStage >= TbcEntryStage && !HasTbc(f))
            return false;
        return true;
    }

    // Highest stage a service (e.g. account catch-up) may grant. It stays
    // below the current era's final milestone, so buying catch-up can never
    // make a character eligible for the next expansion.
    constexpr std::uint8_t MaxServiceGrantableStage(CharacterFacts const& f)
    {
        if (!Applies(f))
            return LastSupportedStage;
        if (!HasTbc(f))
            return VanillaFinalStage - 1;
        if (!HasWrath(f))
            return UnpaidWrathAccessCap;
        return LastSupportedStage;
    }

    // Optional owner-controlled migration: on a character's first evaluation,
    // mark an already-held stage 13 as Wrath-entitled. Never applies twice.
    constexpr bool ShouldGrandfatherWrath(CharacterFacts const& f, bool grandfatherEnabled, bool alreadyEvaluated)
    {
        return Applies(f) && grandfatherEnabled && !alreadyEvaluated &&
               f.rawStage >= TbcFinalStage && !HasWrath(f);
    }
}

#endif
