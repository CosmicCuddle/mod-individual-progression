#include "IndividualProgressionF14CurrentPairRightsRisk.h"

#include <cstdint>

#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)

namespace Risk = IndividualProgressionF14CurrentPairRightsRisk;

static Risk::Facts Baseline()
{
    Risk::Facts f;
    f.viewerPresent = true;
    f.creaturePresent = true;
    f.viewerInWorld = true;
    f.creatureInWorld = true;
    f.sameMapAndInstance = true;
    f.creatureAlive = true;

    f.viewerEligibilityEvidenceComplete = true;
    f.creatureEligibilityEvidenceComplete = true;
    f.directInteractionEvidenceComplete = true;
    f.ownedUnitEvidenceComplete = true;
    f.creatureRoleEvidenceComplete = true;
    f.groupLootEvidenceComplete = true;
    f.auraEvidenceComplete = true;
    f.pendingSpellEvidenceComplete = true;
    f.serviceInteractionEvidenceComplete = true;
    f.scriptedRewardEvidenceComplete = true;
    f.questEvidenceComplete = true;
    return f;
}

int main()
{
    using R = Risk::Result;

    // A fully evidenced clean negative snapshot is still not authority.
    {
        auto f = Baseline();
        CHECK(Risk::Evaluate(f) == R::NoKnownVeto_NotAuthority);
        CHECK(!Risk::MustRemainVisible(f));
        CHECK(!Risk::IsConcealmentAuthorized(f));
    }

    // A lifecycle-valid snapshot with omitted evidence families fails visible.
    {
        Risk::Facts f;
        f.viewerPresent = true;
        f.creaturePresent = true;
        f.viewerInWorld = true;
        f.creatureInWorld = true;
        f.sameMapAndInstance = true;
        f.creatureAlive = true;
        CHECK(Risk::Evaluate(f) == R::EvidenceIncomplete);
        CHECK(Risk::MustRemainVisible(f));
    }

    // Every completeness family is independently mandatory.
    {
        bool Risk::Facts::* const complete[] = {
            &Risk::Facts::viewerEligibilityEvidenceComplete,
            &Risk::Facts::creatureEligibilityEvidenceComplete,
            &Risk::Facts::directInteractionEvidenceComplete,
            &Risk::Facts::ownedUnitEvidenceComplete,
            &Risk::Facts::creatureRoleEvidenceComplete,
            &Risk::Facts::groupLootEvidenceComplete,
            &Risk::Facts::auraEvidenceComplete,
            &Risk::Facts::pendingSpellEvidenceComplete,
            &Risk::Facts::serviceInteractionEvidenceComplete,
            &Risk::Facts::scriptedRewardEvidenceComplete,
            &Risk::Facts::questEvidenceComplete,
        };

        for (auto member : complete)
        {
            auto f = Baseline();
            f.*member = false;
            CHECK(Risk::Evaluate(f) == R::EvidenceIncomplete);
            CHECK(Risk::MustRemainVisible(f));
            CHECK(!Risk::IsConcealmentAuthorized(f));
        }
    }

    // Invalid/lifecycle/GM/dead facts always veto.
    {
        auto f = Baseline(); f.viewerPresent = false;
        CHECK(Risk::Evaluate(f) == R::InvalidOrUnavailablePair);
    }
    {
        auto f = Baseline(); f.sameMapAndInstance = false;
        CHECK(Risk::MustRemainVisible(f));
    }
    {
        auto f = Baseline(); f.viewerIsGameMaster = true;
        CHECK(Risk::Evaluate(f) == R::ViewerNotOrdinaryEligibleHuman);
    }
    {
        auto f = Baseline(); f.creatureAlive = false;
        CHECK(Risk::Evaluate(f) == R::CreatureNotLiveEligibleTarget);
    }

    // Direct viewer and threat/combat-reference evidence.
    {
        auto f = Baseline(); f.viewerTargetsCreature = true;
        CHECK(Risk::Evaluate(f) == R::DirectViewerInteraction);
    }
    {
        auto f = Baseline(); f.viewerAttacksCreature = true;
        CHECK(Risk::Evaluate(f) == R::DirectViewerInteraction);
    }
    {
        auto f = Baseline(); f.creatureAttacksViewer = true;
        CHECK(Risk::Evaluate(f) == R::DirectViewerInteraction);
    }
    {
        auto f = Baseline(); f.viewerOnCreatureThreatOrCombatReferences = true;
        CHECK(Risk::Evaluate(f) == R::DirectViewerInteraction);
    }

    // Player-owned/control-unit and creature-role evidence.
    {
        auto f = Baseline(); f.ownedUnitTargetsOrAttacksCreature = true;
        CHECK(Risk::Evaluate(f) == R::PlayerOwnedUnitInteraction);
    }
    {
        auto f = Baseline(); f.creatureAttacksOwnedUnit = true;
        CHECK(Risk::Evaluate(f) == R::PlayerOwnedUnitInteraction);
    }
    {
        auto f = Baseline(); f.ownedUnitOnCreatureThreatOrCombatReferences = true;
        CHECK(Risk::Evaluate(f) == R::PlayerOwnedUnitInteraction);
    }
    {
        auto f = Baseline(); f.creatureIsPlayerOwnedOrSummoned = true;
        CHECK(Risk::Evaluate(f) == R::CreatureOwnershipOrVehicleInterest);
    }
    {
        auto f = Baseline(); f.creatureIsVehicleOrHasPlayerPassenger = true;
        CHECK(Risk::Evaluate(f) == R::CreatureOwnershipOrVehicleInterest);
    }

    // Group/loot and aura evidence.
    {
        auto f = Baseline(); f.viewerIsGrouped = true;
        CHECK(Risk::Evaluate(f) == R::GroupOrLootInterest);
    }
    {
        auto f = Baseline(); f.viewerIsLootRecipient = true;
        CHECK(Risk::Evaluate(f) == R::GroupOrLootInterest);
    }
    {
        auto f = Baseline(); f.anyGroupLootRecipient = true;
        CHECK(Risk::Evaluate(f) == R::GroupOrLootInterest);
    }
    {
        auto f = Baseline(); f.auraEnumerationOverflow = true;
        CHECK(Risk::Evaluate(f) == R::AuraEvidenceUncertainOrPlayerCast);
    }
    {
        auto f = Baseline(); f.auraEvidenceUnknown = true;
        CHECK(Risk::Evaluate(f) == R::AuraEvidenceUncertainOrPlayerCast);
    }
    {
        auto f = Baseline(); f.anyPlayerCastAura = true;
        CHECK(Risk::Evaluate(f) == R::AuraEvidenceUncertainOrPlayerCast);
    }
    {
        auto f = Baseline(); f.creatureCastAuraOnViewer = true;
        CHECK(Risk::Evaluate(f) == R::AuraEvidenceUncertainOrPlayerCast);
    }

    // Grok #171 L2 omitted-interaction categories now veto explicitly.
    {
        auto f = Baseline();
        f.viewerOrOwnedUnitHasPendingSpellOrChannelOnCreature = true;
        CHECK(Risk::Evaluate(f) == R::PendingSpellOrChannelInterest);
    }
    {
        auto f = Baseline(); f.viewerUsesCreatureServiceOrEscort = true;
        CHECK(Risk::Evaluate(f) == R::ServiceOrEscortInteraction);
    }
    {
        auto f = Baseline(); f.skinningOrScriptedRewardInterest = true;
        CHECK(Risk::Evaluate(f) == R::ScriptedOrRewardInterest);
    }
    {
        auto f = Baseline(); f.encounterOrScriptedInteractionInterest = true;
        CHECK(Risk::Evaluate(f) == R::ScriptedOrRewardInterest);
    }

    // Historical quest negative evidence.
    {
        auto f = Baseline(); f.activeQuestTemplateMissing = true;
        CHECK(Risk::Evaluate(f) == R::ActiveQuestTemplateUncertain);
    }
    {
        auto f = Baseline(); f.activeQuestTargetsCreatureOrKillCredit = true;
        CHECK(Risk::Evaluate(f) == R::ActiveQuestKillCreditInterest);
    }
    {
        auto f = Baseline(); f.viewerHasQuestLootForCreature = true;
        CHECK(Risk::Evaluate(f) == R::QuestLootInterest);
    }

    // Known veto wins diagnostically even if another family is incomplete.
    {
        auto f = Baseline();
        f.directInteractionEvidenceComplete = false;
        f.viewerOnCreatureThreatOrCombatReferences = true;
        CHECK(Risk::Evaluate(f) == R::DirectViewerInteraction);
        CHECK(Risk::MustRemainVisible(f));
    }

    // Multiple facts remain conservative; no combination can authorize.
    {
        auto f = Baseline();
        f.viewerIsGrouped = true;
        f.creatureCastAuraOnViewer = true;
        f.viewerHasQuestLootForCreature = true;
        CHECK(Risk::MustRemainVisible(f));
        CHECK(!Risk::IsConcealmentAuthorized(f));
    }

    return 0;
}
