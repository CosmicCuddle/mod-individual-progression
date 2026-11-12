#include "IndividualProgressionF14CurrentPairRightsRisk.h"

#include <cassert>
#include <cstdint>

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
    return f;
}

int main()
{
    using R = Risk::Result;

    {
        auto f = Baseline();
        assert(Risk::Evaluate(f) == R::NoKnownVeto_NotAuthority);
        assert(!Risk::MustRemainVisible(f));
        assert(!Risk::IsConcealmentAuthorized(f));
    }

    // Invalid/lifecycle/GM/dead facts always veto.
    {
        auto f = Baseline(); f.viewerPresent = false;
        assert(Risk::Evaluate(f) == R::InvalidOrUnavailablePair);
    }
    {
        auto f = Baseline(); f.sameMapAndInstance = false;
        assert(Risk::MustRemainVisible(f));
    }
    {
        auto f = Baseline(); f.viewerIsGameMaster = true;
        assert(Risk::Evaluate(f) == R::ViewerNotOrdinaryEligibleHuman);
    }
    {
        auto f = Baseline(); f.creatureAlive = false;
        assert(Risk::Evaluate(f) == R::CreatureNotLiveEligibleTarget);
    }

    // Historical #99 direct/player-owned/loot negative evidence.
    for (std::uint32_t bit = 0; bit < 7; ++bit)
    {
        auto f = Baseline();
        switch (bit)
        {
            case 0: f.viewerTargetsCreature = true; break;
            case 1: f.viewerAttacksCreature = true; break;
            case 2: f.creatureAttacksViewer = true; break;
            case 3: f.ownedUnitTargetsOrAttacksCreature = true; break;
            case 4: f.creatureAttacksOwnedUnit = true; break;
            case 5: f.viewerIsLootRecipient = true; break;
            case 6: f.anyGroupLootRecipient = true; break;
        }
        assert(Risk::MustRemainVisible(f));
        assert(!Risk::IsConcealmentAuthorized(f));
    }

    // Historical #100 group/aura negative evidence.
    {
        auto f = Baseline(); f.viewerIsGrouped = true;
        assert(Risk::Evaluate(f) == R::GroupOrLootInterest);
    }
    {
        auto f = Baseline(); f.auraEnumerationOverflow = true;
        assert(Risk::Evaluate(f) == R::AuraEvidenceUncertainOrPlayerCast);
    }
    {
        auto f = Baseline(); f.auraEvidenceUnknown = true;
        assert(Risk::Evaluate(f) == R::AuraEvidenceUncertainOrPlayerCast);
    }
    {
        auto f = Baseline(); f.anyPlayerCastAura = true;
        assert(Risk::Evaluate(f) == R::AuraEvidenceUncertainOrPlayerCast);
    }

    // Historical #101/#102 quest negative evidence.
    {
        auto f = Baseline(); f.activeQuestTemplateMissing = true;
        assert(Risk::Evaluate(f) == R::ActiveQuestTemplateUncertain);
    }
    {
        auto f = Baseline(); f.activeQuestTargetsCreatureOrKillCredit = true;
        assert(Risk::Evaluate(f) == R::ActiveQuestKillCreditInterest);
    }
    {
        auto f = Baseline(); f.viewerHasQuestLootForCreature = true;
        assert(Risk::Evaluate(f) == R::QuestLootInterest);
    }

    // Multiple facts choose a conservative veto; no combination can authorize.
    {
        auto f = Baseline();
        f.viewerIsGrouped = true;
        f.anyPlayerCastAura = true;
        f.viewerHasQuestLootForCreature = true;
        assert(Risk::MustRemainVisible(f));
        assert(!Risk::IsConcealmentAuthorized(f));
    }

    return 0;
}
