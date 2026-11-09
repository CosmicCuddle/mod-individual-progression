#ifndef COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_ADAPTER_H
#define COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_ADAPTER_H

// ISOLATED, development-only. Included ONLY when the new positive-policy
// compile macro is explicitly set. The ORIGINAL Core6 negative adapter and
// ordinary-build original source contract are completely preserved.
#if !defined(COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_VIEWER_POLICY_EXPERIMENT) || COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_VIEWER_POLICY_EXPERIMENT < 1
#error "F14 positive actor/WHO class must never compile in an ordinary build"
#endif
#if !defined(COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT) || COSMIC_IP_F14_VERIFIED_ACTOR_PUBLICATION_EXPERIMENT < 1
#error "F14 positive Core6 policy requires the current-login verified actor publication experiment"
#endif
#if !defined(COSMIC_IP_F14_CORE6_ACTOR_WHO_NEGATIVE_ADAPTER_EXPERIMENT) || COSMIC_IP_F14_CORE6_ACTOR_WHO_NEGATIVE_ADAPTER_EXPERIMENT < 1
#error "F14 positive Core6 policy requires the exact existing Core6 actor+WHO ABI"
#endif

#include "IndividualProgression.h"
#include "IndividualProgressionGroupPolicy.h"
#include "IndividualProgressionF14Core6PositiveRndbotViewerPolicy.h"
#include "WorldObjectScript.h"

static_assert(RACE_BLOODELF == CosmicIPF14Core6PositiveRndbot::BloodElfRace &&
              RACE_DRAENEI == CosmicIPF14Core6PositiveRndbot::DraeneiRace,
              "F14 positive viewer policy race identifiers changed");
static_assert(PROGRESSION_PRE_TBC == CosmicIPF14Core6PositiveRndbot::FirstTbcStage,
              "F14 positive viewer policy Vanilla stage boundary changed");

class IndividualProgressionF14Core6PositiveRndbotAdapter final : public WorldObjectScript
{
public:
    IndividualProgressionF14Core6PositiveRndbotAdapter()
        : WorldObjectScript("IndividualProgression_F14Core6PositiveRndbot",
            {WORLDOBJECTHOOK_ON_F14_VIEWER_PLAYER_SUPPRESSION_QUERY,
             WORLDOBJECTHOOK_ON_F14_WHO_GUID_EXCLUSION_QUERY}) { }

private:
    static bool IsConservativePositiveViewer(
        Player const* viewer, ObjectGuid const& targetGuid,
        CosmicIPF14Core6PositiveRndbot::Viewer& facts)
    {
        // Runtime pilot is still forced OFF without a *real* Core refresh.
        // Existing IP enabled, pilot flag and paid effective-stage reads are
        // NOT proven concurrent-map/WHO safe; do not enable this adapter.
        if (!sIndividualProgression->enabled ||
            !sIndividualProgression->f14ExperimentalRndbotPlayerVisibility ||
            !viewer || !targetGuid || viewer->GetGUID() == targetGuid ||
            !viewer->IsInWorld() || viewer->IsGameMaster() ||
            !viewer->GetSession() || viewer->GetSession()->IsHeadless())
            return false;

        Player* ordinaryViewer = const_cast<Player*>(viewer);
        if (!sIndividualProgression->isNormalAccount(ordinaryViewer))
            return false;

        uint8 const effectiveStage =
            sIndividualProgression->GetEffectiveProgression(ordinaryViewer);
        facts.confirmedNormalHumanAccount = true;
        facts.confirmedNonGM = true;
        facts.activeNonHeadlessSession = true;
        facts.inWorld = true;
        facts.confirmedNotSelf = true;
        facts.effectiveStageKnown =
            effectiveStage <= IndividualProgressionGroupPolicy::LastSupportedStage;
        facts.effectiveStage = effectiveStage;
        return CosmicIPF14Core6PositiveRndbot::ViewerIsVanilla(facts);
    }

    static CosmicIPF14Core6PositiveRndbot::Publication ReadVerifiedPublication(
        ObjectGuid const& targetGuid, uint8 claimedRace, uint32 liveAccountId)
    {
        CosmicIPF14Core6PositiveRndbot::Publication proof;
        IndividualProgression::F14VerifiedActorPublication published;
        if (!sIndividualProgression->TryReadF14VerifiedActorPublication(
                targetGuid, published))
            return proof;

        // WHO's cached row has NO account field. For WHO, liveAccountId==0
        // means trust only IP's verified/current nonzero published account,
        // never invent a cached WHO account comparison.
        proof.verifiedSelectedRandomBot = true;
        proof.fullGuidMatches = published.fullGuid == targetGuid;
        proof.currentAccountProven = published.accountId != 0 &&
            (!liveAccountId || liveAccountId == published.accountId);
        proof.raceMatches = published.race == claimedRace;
        proof.nonzeroCurrentLoginEpoch = published.loginEpoch != 0;
        proof.publishedRace = published.race;
        return proof;
    }

public:
    // TRUE means HIDE for Core6. The original TRUE-means-ALLOW IP observer
    // and original FALSE-only Core6 adapter are NOT called by this class.
    bool OnF14ShouldSuppressPlayerForViewer(
        Player const* viewer, Player const* actor) override
    {
        CosmicIPF14Core6PositiveRndbot::Viewer viewerFacts;
        if (!actor || !IsConservativePositiveViewer(
                viewer, actor->GetGUID(), viewerFacts) ||
            (actor->getRace() != RACE_BLOODELF &&
             actor->getRace() != RACE_DRAENEI))
            return false;

        WorldSession const* actorSession = actor->GetSession();
        uint32 const accountId = actorSession ? actorSession->GetAccountId() : 0;
        auto const proof = ReadVerifiedPublication(
            actor->GetGUID(), actor->getRace(), accountId);
        return CosmicIPF14Core6PositiveRndbot::HideActor(
            viewerFacts, proof, actor->IsInWorld(),
            actorSession && actorSession->IsHeadless() && accountId != 0,
            !actor->IsGameMaster());
    }

    // WHO has a cached GUID/race only; never dereference the target Player
    // or query the mutable Playerbots manager on the WHO/session worker.
    bool OnF14ShouldExcludeWhoEntryForViewer(
        Player const* viewer, ObjectGuid const& targetGuid,
        uint32 cachedRace) override
    {
        CosmicIPF14Core6PositiveRndbot::Viewer viewerFacts;
        if (!IsConservativePositiveViewer(viewer, targetGuid, viewerFacts) ||
            (cachedRace != RACE_BLOODELF && cachedRace != RACE_DRAENEI))
            return false;
        auto const proof = ReadVerifiedPublication(
            targetGuid, static_cast<uint8>(cachedRace), 0);
        return CosmicIPF14Core6PositiveRndbot::ExcludeWho(viewerFacts, proof);
    }
};

#endif // COSMIC_IP_F14_CORE6_POSITIVE_RNDBOT_ADAPTER_H
