/*
 * CosmicCuddle IP F14 — event-driven visibility invalidation policy.
 * PURE / UNWIRED: no Core hook, unit mutation, packets or gameplay side effects.
 * Never allow concealment from cached combat data until an explicit, verified
 * same-generation and same-creature observer refresh has completed.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_REFRESH_POLICY_H
#define INDIVIDUAL_PROGRESSION_F14_REFRESH_POLICY_H

#include <cstdint>
#include <limits>

namespace IndividualProgressionF14RefreshPolicy
{
    enum class Event : std::uint8_t
    {
        AttackBegan,
        AttackStopped,
        AttackTargetChanged,
        CreatureDied,
        CreatureDespawned,
        CreatureRespawned,
        ThreatOrRightsChanged,
        ObserverEraOrIdentityChanged,
        BotLoginOrLogout,
        MapOrTeleportChanged,
        ConfigReloaded
    };

    struct State
    {
        std::uint64_t creatureIdentityToken = 0;
        std::uint32_t combatGeneration = 0;
        bool creatureAlive = false;
        bool eligibleHiddenAttackerActive = false;
        bool verifiedFactsCurrent = false;
        bool observerRefreshCompleted = false;
    };

    struct Transition
    {
        State state{};
        bool invalidatePairCache = false;
        bool requestImmediateVisibilityRefresh = false;
        bool forceVisibleDuringRefresh = false;
    };

    // Event intake must occur on the owning map/session thread. Every change
    // invalidates the old pairing; no cached stale attack may hide a corpse.
    // If generation overflows, abandon the identity until a fresh spawn token.
    constexpr Transition OnEvent(State previous, Event event,
                                 std::uint64_t newSpawnIdentity = 0)
    {
        Transition out{};
        out.state = previous;
        out.invalidatePairCache = true;
        out.requestImmediateVisibilityRefresh = true;
        out.forceVisibleDuringRefresh = true;
        out.state.verifiedFactsCurrent = false;
        out.state.observerRefreshCompleted = false;

        if (event == Event::CreatureRespawned)
        {
            // Never recycle the dead creature's cached observer facts.
            out.state.creatureIdentityToken =
                newSpawnIdentity != 0 && newSpawnIdentity != previous.creatureIdentityToken
                    ? newSpawnIdentity : 0;
            out.state.combatGeneration = out.state.creatureIdentityToken ? 1 : 0;
            out.state.creatureAlive = out.state.creatureIdentityToken != 0;
            out.state.eligibleHiddenAttackerActive = false;
            return out;
        }

        if (out.state.combatGeneration == std::numeric_limits<std::uint32_t>::max())
        {
            out.state.creatureIdentityToken = 0;
            out.state.combatGeneration = 0;
        }
        else if (out.state.combatGeneration != 0)
        {
            ++out.state.combatGeneration;
        }

        switch (event)
        {
            case Event::CreatureDied:
            case Event::CreatureDespawned:
                out.state.creatureAlive = false;
                out.state.eligibleHiddenAttackerActive = false;
                break;
            case Event::AttackStopped:
            case Event::AttackTargetChanged:
                out.state.eligibleHiddenAttackerActive = false;
                break;
            case Event::AttackBegan:
                // Still only an unverified attack candidate; no auto-hide.
                out.state.eligibleHiddenAttackerActive = true;
                break;
            case Event::ThreatOrRightsChanged:
            case Event::ObserverEraOrIdentityChanged:
            case Event::BotLoginOrLogout:
            case Event::MapOrTeleportChanged:
            case Event::ConfigReloaded:
                // Fail visible until a fresh safety proof is committed.
                break;
            case Event::CreatureRespawned:
                break; // handled above
        }
        return out;
    }

    // A positive full safety proof is not sufficient on its own. The caller
    // must still complete an immediate client visibility refresh separately.
    // This function never schedules or executes that refresh.
    constexpr State CommitVerifiedFacts(State state, std::uint64_t identity,
                                        std::uint32_t generation,
                                        bool completeAllParticipantsAndRightsProof)
    {
        // Every attempted recomputation revokes an earlier positive result.
        // Incomplete or stale evidence must never reuse previous approval.
        state.verifiedFactsCurrent = false;
        state.observerRefreshCompleted = false;
        if (identity != 0 && generation != 0 &&
            state.creatureIdentityToken == identity &&
            state.combatGeneration == generation &&
            state.creatureAlive && state.eligibleHiddenAttackerActive &&
            completeAllParticipantsAndRightsProof)
        {
            state.verifiedFactsCurrent = true;
        }
        return state;
    }

    constexpr State AcknowledgeObserverRefresh(State state, std::uint64_t identity,
                                               std::uint32_t generation)
    {
        // An out-of-order acknowledgement is not a new visibility proof.
        state.observerRefreshCompleted = false;
        if (state.verifiedFactsCurrent && state.creatureAlive &&
            state.eligibleHiddenAttackerActive && identity != 0 &&
            identity == state.creatureIdentityToken &&
            generation != 0 && generation == state.combatGeneration)
        {
            state.observerRefreshCompleted = true;
        }
        return state;
    }

    constexpr bool MayUseConcealmentProof(State const& state,
                                         std::uint64_t observedIdentity,
                                         std::uint32_t observedGeneration)
    {
        return state.creatureIdentityToken != 0 &&
               state.creatureIdentityToken == observedIdentity &&
               state.combatGeneration != 0 &&
               state.combatGeneration == observedGeneration &&
               state.creatureAlive && state.eligibleHiddenAttackerActive &&
               state.verifiedFactsCurrent && state.observerRefreshCompleted;
    }
}

#endif
