/*
 * CosmicCuddle IP: F14 stage-write policy with EXTERNALLY VERIFIED identity.
 *
 * Pure C++17 policy, deliberately NOT bound to PlayerScript/Playerbots yet.
 * No player/quest/SQL/API access, global state, side effects, or thread hops.
 * Never substitute account name/race/level/currentBots lookup for a positive
 * type proof at the destructive pre-login write boundary.
 */
#ifndef INDIVIDUAL_PROGRESSION_RANDOM_BOT_STAGE_POLICY_H
#define INDIVIDUAL_PROGRESSION_RANDOM_BOT_STAGE_POLICY_H

#include "IndividualProgressionGroupPolicy.h"
#include <cstdint>

namespace IndividualProgressionRandomBotStagePolicy
{
    enum class Identity : std::uint8_t
    {
        Unknown,
        VerifiedRandomBot,
        VerifiedOther // human, Altbot, Selfbot, excluded or other verified non-rndbot
    };

    enum class TbcAccess : std::uint8_t
    {
        Unverified,
        Unavailable,
        Available
    };

    enum class Entry : std::uint8_t
    {
        NewSelection,
        SavedEventRestore,
        PlayerLogin,
        AdminStageWrite,
        BotGroupSync,
        GroupAdmission,
        ProgressionReward
    };

    enum class Result : std::uint8_t
    {
        DeferToExisting,    // Not an F14 rndbot; NEVER treat as write approval
        HoldUnknownIdentity,// Could be an F14 rndbot; resolve identity first
        KeepDormant,        // No verified TBC entitlement/cap
        HoldMigration,      // Saved invalid/missing stage; no silent migration
        InitializeNew,      // Positively new and eligible, stage 8 only
        PreserveExisting,   // Existing legal stage, no re-write necessary
        RejectWrite,        // Illegal request, no mutation permitted
        PermitWrite        // Stage 8..18 write within verified limits only
    };

    struct Request
    {
        Identity identity;
        std::uint8_t race;
        TbcAccess access;
        Entry entry;

        // Only a proven newly created, never-persisted positive RandomBot
        // can initialize its first stage without a migration decision.
        bool verifiedNewUnsaved;
        bool hasCurrentStage;
        std::uint8_t currentStage;
        bool hasRequestedStage;
        std::uint8_t requestedStage;

        // Existing IP semantics: 0 means UNLIMITED; NOT "Vanilla".
        // A progression limit is not, by itself, evidence TBC is unlocked.
        std::int32_t progressionLimit;
    };

    struct Decision
    {
        Result result;
        bool mayActivate;
        bool mayWrite;
        std::uint8_t targetStage; // meaningful ONLY when mayWrite=true
    };

    constexpr Decision Resolve(Request const& in)
    {
        using namespace IndividualProgressionGroupPolicy;

        if (!IsExpansionRace(in.race) || in.identity == Identity::VerifiedOther)
            return { Result::DeferToExisting, false, false, 0 };

        if (in.identity != Identity::VerifiedRandomBot)
            return { Result::HoldUnknownIdentity, false, false, 0 };

        // Fail closed for invalid/unreviewed cap. Zero does NOT imply TBC.
        if (in.progressionLimit < 0 || in.progressionLimit > LastSupportedStage)
            return { Result::KeepDormant, false, false, 0 };

        if (in.access != TbcAccess::Available ||
            (in.progressionLimit && in.progressionLimit < FirstTbcStage))
            return { Result::KeepDormant, false, false, 0 };

        if (in.verifiedNewUnsaved)
        {
            // "New" cannot be used as a shortcut to migrate a saved bot.
            if (in.entry == Entry::NewSelection && !in.hasCurrentStage &&
                in.hasRequestedStage && in.requestedStage == FirstTbcStage)
                return { Result::InitializeNew, true, true, FirstTbcStage };
            return { Result::HoldMigration, false, false, 0 };
        }

        // A missing saved stage is NOT proof the character has never existed.
        if (!in.hasCurrentStage ||
            !IsAllowedRandomBotStage(in.race, in.currentStage) ||
            (in.progressionLimit && in.currentStage > in.progressionLimit))
            return { Result::HoldMigration, false, false, 0 };

        if (!in.hasRequestedStage || in.requestedStage == in.currentStage)
            return { Result::PreserveExisting, true, false, 0 };

        if (!IsAllowedRandomBotStage(in.race, in.requestedStage) ||
            (in.progressionLimit && in.requestedStage > in.progressionLimit))
            return { Result::RejectWrite, true, false, 0 };

        return { Result::PermitWrite, true, true, in.requestedStage };
    }

    static_assert(!Resolve({ Identity::VerifiedRandomBot, 10, TbcAccess::Available,
                             Entry::PlayerLogin, false, true, 8, true, 0, 0 }).mayWrite,
                  "F14: existing TBC stage cannot reset to Vanilla at login");
    static_assert(Resolve({ Identity::VerifiedRandomBot, 11, TbcAccess::Available,
                            Entry::SavedEventRestore, false, true, 12, false, 0, 0 }).result ==
                      Result::PreserveExisting,
                  "F14: saved TBC/Wrath stage must survive restoration unchanged");
    static_assert(Resolve({ Identity::VerifiedRandomBot, 10, TbcAccess::Unavailable,
                            Entry::NewSelection, true, false, 0, true, 8, 0 }).result ==
                      Result::KeepDormant,
                  "F14: never force stage 8 while TBC is unavailable");
    static_assert(Resolve({ Identity::Unknown, 10, TbcAccess::Available,
                            Entry::PlayerLogin, false, true, 8, true, 0, 0 }).result ==
                      Result::HoldUnknownIdentity,
                  "F14: unknown account must never be guessed to be a rndbot");
}

#endif // INDIVIDUAL_PROGRESSION_RANDOM_BOT_STAGE_POLICY_H
