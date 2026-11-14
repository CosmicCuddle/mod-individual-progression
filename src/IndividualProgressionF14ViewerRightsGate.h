/*
 * CosmicCuddle F14: conservative PURE, UNWIRED joint viewer/creature policy gate.
 *
 * This is a development-time composition of the existing combat visibility
 * model (#F14) and Claude's unchanged per-viewer prior-hidden protocol (#94).
 * It supplies NO facts itself. All external authority flags default FALSE;
 * no live source registers a positive Core concealment callback.
 *
 * A true MODEL result can only say that the supplied synthetic facts satisfy
 * both pure policies. It is NEVER permission to hide an object on a server.
 * Missing map ownership, identity provenance, complete human rights or a
 * proven creature incarnation MUST return false/fail visible.
 *
 * No raw Core pointers, Playerbots lookup, DB access, packets, client ACK,
 * ledger mutation or runtime hooks. No positive restoration completion path.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_VIEWER_RIGHTS_GATE_H
#define INDIVIDUAL_PROGRESSION_F14_VIEWER_RIGHTS_GATE_H

#include "IndividualProgressionF14CombatVisibilityPolicy.h"
#include "IndividualProgressionF14ViewerRestoreProtocol.h"

#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14ViewerRightsGate
{
    namespace Visibility = IndividualProgressionF14CombatVisibilityPolicy;
    namespace Restore = IndividualProgressionF14ViewerRestoreProtocol;

    enum class GateReason : std::uint8_t
    {
        InvalidPair,
        WrongOrUnverifiedMapThread,
        MissingActorIdentityOrIncarnationAuthority,
        MissingIndependentHumanRightsAuthority,
        CombatOrObserverPolicyDenied,
        PriorHiddenProtocolDenied,
        HypotheticalModelOnly
    };

    struct JointFacts
    {
        Restore::ViewerKey viewer{};
        Restore::CreatureKey creature{};
        std::uint64_t creatureIncarnation = 0;

        Visibility::CreatureSnapshot combat{};
        Visibility::ObserverSnapshot observer{};
        Restore::DecisionFacts decision{};

        // The C++ model cannot establish any of these facts itself.
        // A future audited Core/IP owning-map consumer would have to prove
        // them at the SAME visibility decision, not use a stale cache.
        bool onConfirmedOwningMapThread = false;
        bool exactVerifiedSelectedRandomBotIdentity = false;
        bool sameLiveCreatureIncarnationIndependentlyProven = false;
        bool independentFullHumanPairRightsProven = false;
    };

    // NEVER use this boolean result to activate visibility suppression.
    // Synthetic positive tests exist solely to check internal consistency.
    template<std::size_t Capacity>
    GateReason CheckHypotheticalSuppressionOnly(
        Restore::Protocol<Capacity> const& protocol, JointFacts const& facts)
    {
        if (!facts.viewer.IsValid() || !facts.creature.IsValid() ||
            facts.creatureIncarnation == 0)
            return GateReason::InvalidPair;

        if (!facts.onConfirmedOwningMapThread ||
            facts.decision.eventMapId != facts.creature.mapId ||
            facts.decision.eventInstanceId != facts.creature.instanceId)
            return GateReason::WrongOrUnverifiedMapThread;

        if (!facts.exactVerifiedSelectedRandomBotIdentity ||
            !facts.sameLiveCreatureIncarnationIndependentlyProven ||
            !facts.decision.actorPositivelyVerifiedRandomBot ||
            facts.decision.currentViewerEpoch != facts.viewer.loginEpoch ||
            facts.decision.currentCreatureIncarnation != facts.creatureIncarnation)
            return GateReason::MissingActorIdentityOrIncarnationAuthority;

        // The existing Core snapshot covers threat/combat/tap IN PART only.
        // Quest, group, pet, pending spells, assist and loot rights have no
        // verified complete source today. Never infer this flag from the
        // non-authoritative lifecycle snapshot.
        if (!facts.independentFullHumanPairRightsProven)
            return GateReason::MissingIndependentHumanRightsAuthority;

        if (!Visibility::ShouldConcealCreatureFromObserver(facts.combat, facts.observer))
            return GateReason::CombatOrObserverPolicyDenied;

        // Ignore a caller-supplied "true" rights flag if the independent
        // human rights authority or visibility policy failed. This flag
        // originates ONLY from the conservative checks above.
        Restore::DecisionFacts verified = facts.decision;
        verified.pairRightsProven = true;
        if (!protocol.MayAnswerPositiveSuppression(facts.viewer, facts.creature,
                                                   facts.creatureIncarnation, verified))
            return GateReason::PriorHiddenProtocolDenied;

        return GateReason::HypotheticalModelOnly;
    }

    // Actual active policy remains permanently FALSE in this header. A
    // future, separately approved runtime implementation cannot reuse the
    // hypothetical method as a positive OnF14ShouldSuppress... provider.
    constexpr bool IsLiveCreatureConcealmentAuthorized() { return false; }
}
#endif
