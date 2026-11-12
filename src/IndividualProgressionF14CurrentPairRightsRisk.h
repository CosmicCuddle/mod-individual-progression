/*
 * CosmicCuddle F14 current pair-rights RISK model.
 *
 * PURE / UNWIRED / FAIL-VISIBLE ONLY.
 *
 * This preserves the established negative evidence from historical IP
 * #99-#102 on the current lineage. It can say that a viewer/creature pair
 * MUST remain visible because a known interaction/right/uncertainty exists.
 *
 * Critically, NoKnownVeto_NotAuthority is NOT permission to conceal. The
 * model cannot prove pending spells, complete group/shared reward rights,
 * all pets/totems/guardians, skinning/scripted rewards, encounter reach or
 * any other unrepresented right. No Core pointers, DB, packets or mutation.
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
        GroupOrLootInterest,
        AuraEvidenceUncertainOrPlayerCast,
        ActiveQuestTemplateUncertain,
        ActiveQuestKillCreditInterest,
        QuestLootInterest,
        NoKnownVeto_NotAuthority
    };

    struct Facts
    {
        // Pair/lifecycle facts.
        bool viewerPresent = false;
        bool creaturePresent = false;
        bool viewerInWorld = false;
        bool creatureInWorld = false;
        bool sameMapAndInstance = false;
        bool viewerIsGameMaster = false;
        bool creatureAlive = false;

        // Historical #99 direct interaction / loot veto family.
        bool viewerTargetsCreature = false;
        bool viewerAttacksCreature = false;
        bool creatureAttacksViewer = false;
        bool viewerIsLootRecipient = false;
        bool anyGroupLootRecipient = false;

        // Player-owned pet/charm/guardian interaction, conservatively folded
        // into value-only facts. A current Core adapter would have to prove
        // each source on the owning map; this model does not fetch pointers.
        bool ownedUnitTargetsOrAttacksCreature = false;
        bool creatureAttacksOwnedUnit = false;

        // Historical #100: until complete shared rights exist, any grouped
        // viewer is a veto. Aura enumeration is bounded at 64; overflow,
        // unknown application/base/caster, or any player-cast aura is a veto.
        bool viewerIsGrouped = false;
        bool auraEnumerationOverflow = false;
        bool auraEvidenceUnknown = false;
        bool anyPlayerCastAura = false;

        // Historical #101/#102: active incomplete NPC/kill-credit objectives
        // and real per-player quest loot are negative evidence. A missing
        // template for a nonzero active quest slot is also uncertainty.
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
            f.creatureAttacksViewer)
            return Result::DirectViewerInteraction;

        if (f.ownedUnitTargetsOrAttacksCreature ||
            f.creatureAttacksOwnedUnit)
            return Result::PlayerOwnedUnitInteraction;

        if (f.viewerIsGrouped || f.viewerIsLootRecipient ||
            f.anyGroupLootRecipient)
            return Result::GroupOrLootInterest;

        if (f.auraEnumerationOverflow || f.auraEvidenceUnknown ||
            f.anyPlayerCastAura)
            return Result::AuraEvidenceUncertainOrPlayerCast;

        if (f.activeQuestTemplateMissing)
            return Result::ActiveQuestTemplateUncertain;

        if (f.activeQuestTargetsCreatureOrKillCredit)
            return Result::ActiveQuestKillCreditInterest;

        if (f.viewerHasQuestLootForCreature)
            return Result::QuestLootInterest;

        return Result::NoKnownVeto_NotAuthority;
    }

    constexpr bool MustRemainVisible(Facts const& f)
    {
        return Evaluate(f) != Result::NoKnownVeto_NotAuthority;
    }

    // Deliberately impossible in this model. Even a clean negative snapshot
    // is incomplete evidence and cannot become a positive Core listener.
    constexpr bool IsConcealmentAuthorized(Facts const&) { return false; }
}

#endif
