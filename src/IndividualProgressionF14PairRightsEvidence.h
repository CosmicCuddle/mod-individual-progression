/*
 * CosmicCuddle F14 pair-rights EVIDENCE mapping (pure, value-only).
 *
 * A Core-facing collector fills `Raw` from the current viewer and creature.
 * `ToFacts` alone decides every IndividualProgressionF14CurrentPairRightsRisk
 * EvidenceComplete bit. A bit is TRUE only when the represented family was
 * actually and exhaustively scanned within a bound; every default is the
 * failing value. Positive vetoes are always carried through, so an observed
 * interaction still wins over incompleteness.
 *
 * On Core25 (395b68ee) four families can never be proven complete and are
 * hard FALSE here: pending spells (in-flight delayed spells are not
 * enumerable), service/escort (external scripts), scripted/reward
 * interaction (no reverse script index) and quests (script/spell kill credit
 * and loot routes beyond the scanned templates). So a runtime snapshot can
 * never reach NoKnownVeto_NotAuthority, and even that result is never
 * concealment authority.
 *
 * No Core pointers, DB, packets, Playerbots or mutation live here.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_PAIR_RIGHTS_EVIDENCE_H
#define INDIVIDUAL_PROGRESSION_F14_PAIR_RIGHTS_EVIDENCE_H

#include "IndividualProgressionF14CurrentPairRightsRisk.h"
#include <cstdint>

namespace IndividualProgressionF14PairRightsEvidence
{
    namespace Risk = IndividualProgressionF14CurrentPairRightsRisk;

    constexpr std::uint32_t MaxAurasPerUnit = 64;

    // One bounded applied-aura scan. Casters are classified from the aura's
    // own caster GUID only; no other object is looked up.
    struct AuraScan
    {
        bool scanned = false;
        bool overflow = false;
        std::uint32_t total = 0;
        std::uint32_t castBySelf = 0;            // caster == the scanned unit
        std::uint32_t castByTargetCreature = 0;  // viewer scan: caster == the creature
        std::uint32_t castByPlayerOrPet = 0;
        std::uint32_t castByUnresolved = 0;      // empty, other creature, GO, dynobj...
    };

    struct Raw
    {
        // Existing observer gates: exact active pass identity, exact live
        // creature incarnation, same Map*, verified RandomBot attackers > 0.
        bool observerGatesPassed = false;

        // Pair/lifecycle.
        bool viewerPresent = false;
        bool creaturePresent = false;
        bool viewerInWorld = false;
        bool creatureInWorld = false;
        bool sameMapAndInstance = false;
        bool creatureAlive = false;

        // Viewer eligibility.
        bool eligibilityScanned = false;
        bool viewerIsGameMaster = true;
        bool viewerCharmed = true;
        bool viewerSeerIsSelf = false;

        // Direct interaction (viewer/creature own state only).
        bool directScanned = false;
        bool viewerTargetIsCreature = false;
        bool viewerVictimIsCreature = false;
        bool creatureVictimIsViewer = false;
        bool creatureThreatenedByViewer = false;
        bool creatureInCombatWithViewer = false;
        bool viewerInCombatWithCreature = false;

        // Player-owned units: PRESENCE only, never dereferenced.
        bool ownedScanned = false;
        bool anyControlledUnit = true;
        bool anySummonSlot = true;
        bool anyCharmMinionPetOrCritterGuid = true;
        bool anyViewerVehicleRelation = true;

        // Creature role (creature's own fields only).
        bool roleScanned = false;
        bool creatureHasOwnerCharmerOrCreator = false;
        bool creatureControlledByPlayer = false;
        bool creatureIsSummonPetTotemOrGuardian = false;
        bool creatureHasVehicleRelation = false;

        // Group / loot (own fields; no member iteration).
        bool groupLootScanned = false;
        bool viewerInGroup = false;
        bool lootRecipientIsViewer = false;
        bool lootRecipientGroupSet = false;

        // Auras on the creature and on the viewer.
        AuraScan creatureAuras{};
        AuraScan viewerAuras{};

        // Pending spells: any current spell slot in use (target ignored).
        bool viewerHasAnyCurrentSpell = false;

        // Service capability / active vendor (always incomplete).
        bool creatureHasNpcFlagsOrService = false;
        bool viewerCurrentVendorIsCreatureEntry = false;

        // Scripted / encounter vetoes (always incomplete).
        bool creatureHasScriptOrAI = false;
        bool creatureOnInstanceableMap = false;
        bool creatureInFormation = false;
        bool creatureIsBoss = false;

        // Quest vetoes (always incomplete).
        bool questSlotTemplateMissing = false;
        bool questTargetsCreatureOrKillCredit = false;
        bool questLootForViewer = false;
    };

    constexpr bool AuraScanConsistent(AuraScan const& s)
    {
        return s.total <= MaxAurasPerUnit &&
            s.castBySelf + s.castByTargetCreature + s.castByPlayerOrPet + s.castByUnresolved == s.total;
    }

    constexpr Risk::Facts ToFacts(Raw const& r)
    {
        Risk::Facts f{};

        f.viewerPresent = r.viewerPresent;
        f.creaturePresent = r.creaturePresent;
        f.viewerInWorld = r.viewerInWorld;
        f.creatureInWorld = r.creatureInWorld;
        f.sameMapAndInstance = r.sameMapAndInstance;
        f.viewerIsGameMaster = r.viewerIsGameMaster;
        f.creatureAlive = r.creatureAlive;

        f.viewerEligibilityEvidenceComplete = r.observerGatesPassed && r.eligibilityScanned &&
            !r.viewerCharmed && r.viewerSeerIsSelf;
        f.creatureEligibilityEvidenceComplete = r.observerGatesPassed && r.creatureAlive;

        f.viewerTargetsCreature = r.viewerTargetIsCreature;
        f.viewerAttacksCreature = r.viewerVictimIsCreature;
        f.creatureAttacksViewer = r.creatureVictimIsViewer;
        f.viewerOnCreatureThreatOrCombatReferences = r.creatureThreatenedByViewer ||
            r.creatureInCombatWithViewer || r.viewerInCombatWithCreature;
        f.directInteractionEvidenceComplete = r.directScanned;

        // Owned units are never dereferenced: any presence is uncertainty.
        f.ownedUnitEvidenceComplete = r.ownedScanned && !r.anyControlledUnit &&
            !r.anySummonSlot && !r.anyCharmMinionPetOrCritterGuid && !r.anyViewerVehicleRelation;

        f.creatureIsPlayerOwnedOrSummoned = r.creatureHasOwnerCharmerOrCreator ||
            r.creatureControlledByPlayer || r.creatureIsSummonPetTotemOrGuardian;
        f.creatureIsVehicleOrHasPlayerPassenger = r.creatureHasVehicleRelation;
        f.creatureRoleEvidenceComplete = r.roleScanned;

        f.viewerIsGrouped = r.viewerInGroup;
        f.viewerIsLootRecipient = r.lootRecipientIsViewer;
        f.anyGroupLootRecipient = r.lootRecipientGroupSet;
        f.groupLootEvidenceComplete = r.groupLootScanned;

        bool const creatureAurasOk = r.creatureAuras.scanned && AuraScanConsistent(r.creatureAuras);
        bool const viewerAurasOk = r.viewerAuras.scanned && AuraScanConsistent(r.viewerAuras);
        f.auraEnumerationOverflow = r.creatureAuras.overflow || r.viewerAuras.overflow;
        f.auraEvidenceUnknown =
            (r.creatureAuras.scanned && !AuraScanConsistent(r.creatureAuras)) ||
            (r.viewerAuras.scanned && !AuraScanConsistent(r.viewerAuras)) ||
            r.creatureAuras.castByUnresolved != 0 || r.creatureAuras.castByTargetCreature != 0 ||
            r.viewerAuras.castByUnresolved != 0;
        f.anyPlayerCastAura = r.creatureAuras.castByPlayerOrPet != 0;
        f.creatureCastAuraOnViewer = r.viewerAuras.castByTargetCreature != 0;
        f.auraEvidenceComplete = creatureAurasOk && viewerAurasOk &&
            !f.auraEnumerationOverflow && !f.auraEvidenceUnknown;

        f.viewerOrOwnedUnitHasPendingSpellOrChannelOnCreature = r.viewerHasAnyCurrentSpell;
        f.pendingSpellEvidenceComplete = false;     // in-flight delayed spells not enumerable

        f.viewerUsesCreatureServiceOrEscort = r.creatureHasNpcFlagsOrService ||
            r.viewerCurrentVendorIsCreatureEntry;
        f.serviceInteractionEvidenceComplete = false; // external scripted escort/service

        f.encounterOrScriptedInteractionInterest = r.creatureHasScriptOrAI ||
            r.creatureOnInstanceableMap || r.creatureInFormation || r.creatureIsBoss;
        f.skinningOrScriptedRewardInterest = false;   // creature alive; no reward fact claimed
        f.scriptedRewardEvidenceComplete = false;     // no reverse script-reference index

        f.activeQuestTemplateMissing = r.questSlotTemplateMissing;
        f.activeQuestTargetsCreatureOrKillCredit = r.questTargetsCreatureOrKillCredit;
        f.viewerHasQuestLootForCreature = r.questLootForViewer;
        f.questEvidenceComplete = false;              // script/spell credit, other loot routes

        return f;
    }

    constexpr Risk::Result Classify(Raw const& r) { return Risk::Evaluate(ToFacts(r)); }

    // The runtime can never produce a clean terminal on Core25.
    constexpr bool CanReachCleanTerminalOnCore25() { return false; }
    constexpr bool IsConcealmentAuthorized(Raw const&) { return false; }

    constexpr char const* ResultName(Risk::Result r)
    {
        switch (r)
        {
            case Risk::Result::InvalidOrUnavailablePair: return "InvalidOrUnavailablePair";
            case Risk::Result::ViewerNotOrdinaryEligibleHuman: return "ViewerNotOrdinaryEligibleHuman";
            case Risk::Result::CreatureNotLiveEligibleTarget: return "CreatureNotLiveEligibleTarget";
            case Risk::Result::DirectViewerInteraction: return "DirectViewerInteraction";
            case Risk::Result::PlayerOwnedUnitInteraction: return "PlayerOwnedUnitInteraction";
            case Risk::Result::CreatureOwnershipOrVehicleInterest: return "CreatureOwnershipOrVehicleInterest";
            case Risk::Result::GroupOrLootInterest: return "GroupOrLootInterest";
            case Risk::Result::AuraEvidenceUncertainOrPlayerCast: return "AuraEvidenceUncertainOrPlayerCast";
            case Risk::Result::PendingSpellOrChannelInterest: return "PendingSpellOrChannelInterest";
            case Risk::Result::ServiceOrEscortInteraction: return "ServiceOrEscortInteraction";
            case Risk::Result::ScriptedOrRewardInterest: return "ScriptedOrRewardInterest";
            case Risk::Result::ActiveQuestTemplateUncertain: return "ActiveQuestTemplateUncertain";
            case Risk::Result::ActiveQuestKillCreditInterest: return "ActiveQuestKillCreditInterest";
            case Risk::Result::QuestLootInterest: return "QuestLootInterest";
            case Risk::Result::EvidenceIncomplete: return "EvidenceIncomplete";
            case Risk::Result::NoKnownVeto_NotAuthority: return "NoKnownVeto_NotAuthority";
        }
        return "Unknown";
    }

    constexpr std::uint32_t ResultCount =
        static_cast<std::uint32_t>(Risk::Result::NoKnownVeto_NotAuthority) + 1;
}

#endif
