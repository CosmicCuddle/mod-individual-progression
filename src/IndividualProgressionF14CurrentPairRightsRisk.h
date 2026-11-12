/*
 * CosmicCuddle F14 current pair-rights RISK model.
 *
 * PURE / UNWIRED / FAIL-VISIBLE ONLY.
 *
 * This preserves the established negative evidence from historical IP
 * #99-#102 on the current lineage. It can say that a viewer/creature pair
 * MUST remain visible because a known interaction/right/uncertainty exists.
 *
 * A clean negative snapshot requires explicit evidence-complete proofs for
 * every represented veto family. Missing adapter evidence is itself a veto.
 *
 * Critically, NoKnownVeto_NotAuthority is NOT permission to conceal.
 * No Core pointers, DB, packets or mutation live in this value-only model.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_CURRENT_PAIR_RIGHTS_RISK_H
#define INDIVIDUAL_PROGRESSION_F14_CURRENT_PAIR_RIGHTS_RISK_H

#include <cstdint>

namespace IndividualProgressionF14CurrentPairRightsRisk
{
    enum class Result : std::uint8_t
    {
        InvalidOrUnavailablePair,
        ViewerNotOrdinaryEligibleHuman,
        CreatureNotLiveEligibleTarget,
        DirectViewerInteraction,
        PlayerOwnedUnitInteraction,
        CreatureOwnershipOrVehicleInterest,
        GroupOrLootInterest,
        AuraEvidenceUncertainOrPlayerCast,
        PendingSpellOrChannelInterest,
        ServiceOrEscortInteraction,
        ScriptedOrRewardInterest,
        ActiveQuestTemplateUncertain,
        ActiveQuestKillCreditInterest,
        QuestLootInterest,
        EvidenceIncomplete,
        NoKnownVeto_NotAuthority
    };

    struct Facts
    {
        // Pair/lifecycle facts. Missing presence/world/map/alive data already
        // fails visible independently of the completeness fence below.
        bool viewerPresent = false;
        bool creaturePresent = false;
        bool viewerInWorld = false;
        bool creatureInWorld = false;
        bool sameMapAndInstance = false;
        bool viewerIsGameMaster = false;
        bool creatureAlive = false;

        // A future adapter must explicitly prove each represented evidence
        // family complete. All default FALSE so an omitted family cannot turn
        // into a clean negative result.
        bool viewerEligibilityEvidenceComplete = false;
        bool creatureEligibilityEvidenceComplete = false;
        bool directInteractionEvidenceComplete = false;
        bool ownedUnitEvidenceComplete = false;
        bool creatureRoleEvidenceComplete = false;
        bool groupLootEvidenceComplete = false;
        bool auraEvidenceComplete = false;
        bool pendingSpellEvidenceComplete = false;
        bool serviceInteractionEvidenceComplete = false;
        bool scriptedRewardEvidenceComplete = false;
        bool questEvidenceComplete = false;

        // Direct viewer interaction. Threat/combat references cover the case
        // where the human is not the creature's current victim/target.
        bool viewerTargetsCreature = false;
        bool viewerAttacksCreature = false;
        bool creatureAttacksViewer = false;
        bool viewerOnCreatureThreatOrCombatReferences = false;

        // Loot/group rights.
        bool viewerIsLootRecipient = false;
        bool anyGroupLootRecipient = false;
        bool viewerIsGrouped = false;

        // Player-owned pet/charm/totem/guardian interaction and combat refs.
        bool ownedUnitTargetsOrAttacksCreature = false;
        bool creatureAttacksOwnedUnit = false;
        bool ownedUnitOnCreatureThreatOrCombatReferences = false;

        // The creature itself may be a player-owned summon/charm, or a
        // vehicle carrying a player. Either is a visibility veto.
        bool creatureIsPlayerOwnedOrSummoned = false;
        bool creatureIsVehicleOrHasPlayerPassenger = false;

        // Aura evidence. Bounded enumeration overflow, unresolved
        // application/base/caster data, player-cast auras, or a creature aura
        // still affecting the viewer are conservative visibility vetoes.
        bool auraEnumerationOverflow = false;
        bool auraEvidenceUnknown = false;
        bool anyPlayerCastAura = false;
        bool creatureCastAuraOnViewer = false;

        // Pending casts/missiles/channels cannot be safely severed by a
        // negative visibility decision.
        bool viewerOrOwnedUnitHasPendingSpellOrChannelOnCreature = false;

        // Gossip/vendor/trainer/flight/quest-giver/escort or comparable
        // active service interaction is a direct player right.
        bool viewerUsesCreatureServiceOrEscort = false;

        // Skinning, scripted rewards, encounter reach, or other scripted
        // interaction/reward state remains a visibility veto.
        bool skinningOrScriptedRewardInterest = false;
        bool encounterOrScriptedInteractionInterest = false;

        // Historical #101/#102 quest negative evidence.
        bool activeQuestTemplateMissing = false;
        bool activeQuestTargetsCreatureOrKillCredit = false;
        bool viewerHasQuestLootForCreature = false;
    };

    constexpr Result Evaluate(Facts const& f)
    {
        if (!f.viewerPresent || !f.creaturePresent ||
            !f.viewerInWorld || !f.creatureInWorld || !f.sameMapAndInstance)
            return Result::InvalidOrUnavailablePair;

        if (f.viewerIsGameMaster)
            return Result::ViewerNotOrdinaryEligibleHuman;

        if (!f.creatureAlive)
            return Result::CreatureNotLiveEligibleTarget;

        if (f.viewerTargetsCreature || f.viewerAttacksCreature ||
            f.creatureAttacksViewer ||
            f.viewerOnCreatureThreatOrCombatReferences)
            return Result::DirectViewerInteraction;

        if (f.ownedUnitTargetsOrAttacksCreature ||
            f.creatureAttacksOwnedUnit ||
            f.ownedUnitOnCreatureThreatOrCombatReferences)
            return Result::PlayerOwnedUnitInteraction;

        if (f.creatureIsPlayerOwnedOrSummoned ||
            f.creatureIsVehicleOrHasPlayerPassenger)
            return Result::CreatureOwnershipOrVehicleInterest;

        if (f.viewerIsGrouped || f.viewerIsLootRecipient ||
            f.anyGroupLootRecipient)
            return Result::GroupOrLootInterest;

        if (f.auraEnumerationOverflow || f.auraEvidenceUnknown ||
            f.anyPlayerCastAura || f.creatureCastAuraOnViewer)
            return Result::AuraEvidenceUncertainOrPlayerCast;

        if (f.viewerOrOwnedUnitHasPendingSpellOrChannelOnCreature)
            return Result::PendingSpellOrChannelInterest;

        if (f.viewerUsesCreatureServiceOrEscort)
            return Result::ServiceOrEscortInteraction;

        if (f.skinningOrScriptedRewardInterest ||
            f.encounterOrScriptedInteractionInterest)
            return Result::ScriptedOrRewardInterest;

        if (f.activeQuestTemplateMissing)
            return Result::ActiveQuestTemplateUncertain;

        if (f.activeQuestTargetsCreatureOrKillCredit)
            return Result::ActiveQuestKillCreditInterest;

        if (f.viewerHasQuestLootForCreature)
            return Result::QuestLootInterest;

        if (!f.viewerEligibilityEvidenceComplete ||
            !f.creatureEligibilityEvidenceComplete ||
            !f.directInteractionEvidenceComplete ||
            !f.ownedUnitEvidenceComplete ||
            !f.creatureRoleEvidenceComplete ||
            !f.groupLootEvidenceComplete ||
            !f.auraEvidenceComplete ||
            !f.pendingSpellEvidenceComplete ||
            !f.serviceInteractionEvidenceComplete ||
            !f.scriptedRewardEvidenceComplete ||
            !f.questEvidenceComplete)
            return Result::EvidenceIncomplete;

        return Result::NoKnownVeto_NotAuthority;
    }

    constexpr bool MustRemainVisible(Facts const& f)
    {
        return Evaluate(f) != Result::NoKnownVeto_NotAuthority;
    }

    // Deliberately impossible in this model. Even an evidence-complete clean
    // negative snapshot is not positive concealment authority.
    constexpr bool IsConcealmentAuthorized(Facts const&) { return false; }
}

#endif
