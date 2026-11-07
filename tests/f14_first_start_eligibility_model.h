#ifndef COSMIC_IP_F14_TEST_ONLY_FIRST_START_DECISION_H
#define COSMIC_IP_F14_TEST_ONLY_FIRST_START_DECISION_H

// TEST ONLY — UNWIRED. This is a fail-closed eligibility MODEL, not a
// permitted stage writer. Caller asserts flags; no real Playerbot creation
// epoch, game account data, cap, DB, entitlement or map-thread provenance
// has been verified. Never invoke ForceUpdateProgressionState from this model.
#include <cstdint>

namespace CosmicF14FirstStartModel
{
enum class Verdict : std::uint8_t
{
    NoAction,       // Feature OFF, wrong race, unverified actor
    NeedsReview,    // Ambiguous existing data/cap/owner exception
    PreserveEarned, // Already earned >=8, no quest write is authorized
    EligibleNew     // Only hypothetical first-time, verified stage8 grant
};

struct Facts
{
    bool PilotEnabled = false;
    bool PositiveVerifiedRandomBot = false;
    bool Headless = false;
    bool GenuineFirstCreationEvidence = false;
    bool ExistingCharacterData = true; // fail closed: unknown is existing
    bool WorldTbcEnabled = false;
    bool OwnerApprovedBotStartingException = false;
    bool PaidEraFeatureEnabled = false;
    bool OwnerApprovedPaidBotException = false;
    std::uint8_t Race = 0;
    std::uint8_t SavedStage = 0;
    std::int32_t ProgressionLimit = -1; // -1 unknown; 0 is an explicit no cap
};

constexpr Verdict Decide(Facts const& f)
{
    if (!f.PilotEnabled || !f.PositiveVerifiedRandomBot || !f.Headless ||
        (f.Race != 10 && f.Race != 11))
        return Verdict::NoAction;

    // Unknown/invalid progress or cap NEVER grants anything.
    if (f.SavedStage > 18 || f.ProgressionLimit < 0 ||
        f.ProgressionLimit > 18)
        return Verdict::NeedsReview;

    // A 0 cap is "uncapped", not evidence that the world has TBC.
    if (!f.WorldTbcEnabled ||
        (f.ProgressionLimit != 0 && f.ProgressionLimit < 8) ||
        !f.OwnerApprovedBotStartingException)
        return Verdict::NeedsReview;

    if (f.PaidEraFeatureEnabled && !f.OwnerApprovedPaidBotException)
        return Verdict::NeedsReview;

    if (f.SavedStage >= 8)
    {
        // This preserves data as-is but never writes it, including stages
        // above current cap which need separate owner review.
        if (f.ProgressionLimit && f.SavedStage > f.ProgressionLimit)
            return Verdict::NeedsReview;
        return Verdict::PreserveEarned;
    }

    // Existing stage 0..7 may be genuinely earned, or already damaged by
    // a historical PB ResetQuests. Neither can be silently "fixed".
    if (f.ExistingCharacterData || !f.GenuineFirstCreationEvidence)
        return Verdict::NeedsReview;

    // A fresh actor with saved stage 1..7 is inconsistent and cannot be
    // treated as a new/empty character without an explicit migration.
    if (f.SavedStage != 0)
        return Verdict::NeedsReview;

    // Required external proof is STILL missing in the live server:
    // a unique, verified creation event on the safe owner lifecycle.
    return Verdict::EligibleNew;
}
}
#endif
