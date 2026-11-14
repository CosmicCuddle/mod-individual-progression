/*
 * CosmicCuddle F14 — source-tagged current-viewer/creature identity facts.
 *
 * Model only: a positive F14-specific Core SUPPRESSION DECISION callback may
 * provide both WorldObjects while their identities are readable. This DOES
 * NOT say the target is still linked to that viewer, that the client has
 * received any packet, or that the Core caller was on the owning map thread.
 *
 * No positive hide listener is registered. No protocol or ledger is changed.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_SUPPRESSION_DECISION_IDENTITY_H
#define INDIVIDUAL_PROGRESSION_F14_SUPPRESSION_DECISION_IDENTITY_H

#include <cstdint>

namespace IndividualProgressionF14SuppressionDecisionIdentity
{
    enum class Result : std::uint8_t
    {
        NotSourceAttributedF14Decision,
        MissingVerifiedHumanViewer,
        MissingLiveCreatureIdentity,
        MismatchedMapOrInstance,
        StaleOrUnknownRegistryIncarnation,
        CurrentSourceDecisionIdentitySnapshotOnly
    };

    struct Facts
    {
        bool explicitlySourceAttributedF14Decision = false;
        bool currentViewerIsHumanNormalAccount = false;
        std::uint64_t viewerFullGuid = 0;
        std::uint64_t viewerLoginEpoch = 0;
        bool viewerInWorld = false;

        bool creatureIsWorldNpc = false;
        bool creatureInWorld = false;
        std::uint64_t creatureFullGuid = 0;
        std::uint64_t creatureIncarnationStamp = 0;

        bool sameLiveCoreMapPointer = false;
        std::uint32_t viewerMapId = 0;
        std::uint32_t viewerInstanceId = 0;
        std::uint32_t creatureMapId = 0;
        std::uint32_t creatureInstanceId = 0;

        bool registryNotPoisonedAndMatched = false;
        std::uint64_t registryCreatureFullGuid = 0;
        std::uint64_t registryIncarnationStamp = 0;
        std::uint32_t registryMapId = 0;
        std::uint32_t registryInstanceId = 0;
    };

    constexpr Result Validate(Facts const& f)
    {
        if (!f.explicitlySourceAttributedF14Decision)
            return Result::NotSourceAttributedF14Decision;

        if (!f.viewerInWorld || !f.currentViewerIsHumanNormalAccount ||
            !f.viewerFullGuid || !f.viewerLoginEpoch)
            return Result::MissingVerifiedHumanViewer;

        if (!f.creatureIsWorldNpc || !f.creatureInWorld ||
            !f.creatureFullGuid || !f.creatureIncarnationStamp)
            return Result::MissingLiveCreatureIdentity;

        if (!f.sameLiveCoreMapPointer ||
            f.viewerMapId != f.creatureMapId ||
            f.viewerInstanceId != f.creatureInstanceId ||
            f.creatureMapId != f.registryMapId ||
            f.creatureInstanceId != f.registryInstanceId)
            return Result::MismatchedMapOrInstance;

        if (!f.registryNotPoisonedAndMatched ||
            f.registryCreatureFullGuid != f.creatureFullGuid ||
            f.registryIncarnationStamp != f.creatureIncarnationStamp)
            return Result::StaleOrUnknownRegistryIncarnation;

        return Result::CurrentSourceDecisionIdentitySnapshotOnly;
    }

    // No independent Core thread assertion and no client-object ACK exists
    // here; even a current source snapshot cannot drive protocol or hiding.
    constexpr bool ProvesOwningMapThread() { return false; }
    constexpr bool ProvesClientVisibilityOrRestoration() { return false; }
    constexpr bool AuthorizesConcealment() { return false; }
    constexpr bool AuthorizesPriorHiddenLedgerMutation() { return false; }
}
#endif
