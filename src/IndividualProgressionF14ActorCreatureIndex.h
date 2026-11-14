/*
 * CosmicCuddle F14 — PURE/UNWIRED per-map actor -> creature invalidation model.
 *
 * This is NOT a live map-thread implementation and DOES NOT authorise hiding.
 * A future caller must prove complete actor/creature/rights snapshots on the
 * owning map thread, notify every affected client and handle async teardown.
 * No Unit*, packets, Playerbots queries, DB, phase, loot or quest mutations.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_ACTOR_CREATURE_INDEX_H
#define INDIVIDUAL_PROGRESSION_F14_ACTOR_CREATURE_INDEX_H

#include "IndividualProgressionF14MultiAttackerPolicy.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14ActorCreatureIndex
{
    namespace Policy = IndividualProgressionF14MultiAttackerPolicy;
    constexpr std::size_t MaxKnownCreatures = 32;
    constexpr std::size_t MaxExpiredLogins = 64;
    constexpr std::size_t MaxRetiredCreatures = 64;

    struct ExpiryFanout
    {
        // A nonempty list is ONLY a model of creatures needing an eventual
        // map-thread refresh. It does not enqueue or send any notifications.
        std::array<std::uint64_t, MaxKnownCreatures> affectedCreatureTokens{};
        std::size_t affectedCount = 0;
        bool requireGlobalObserverInvalidation = true;
        bool requireCompleteMapThreadRebuild = true;
        bool actorWasIndexed = false;
        bool registryPermanentlyDisabled = false;
    };

    class Index
    {
    public:
        bool CanConsiderCreature(std::uint64_t creatureToken) const
        {
            if (!_complete || _poisoned || creatureToken == 0)
                return false;
            for (Policy::State const& state : _creatures)
                if (state.creatureInstanceToken == creatureToken)
                    return Policy::HasVerifiedAttackerCandidates(state);
            return false;
        }

        bool IsComplete() const { return _complete && !_poisoned; }
        bool IsPoisoned() const { return _poisoned; }

        // A FULL owner-map-thread snapshot is mandatory. The boolean only
        // records the external caller's proof, which THIS MODEL CANNOT MAKE.
        // All previous candidate decisions are invalidated during rebuild.
        bool ReplaceFromCompleteSnapshot(
            std::array<Policy::State, MaxKnownCreatures> const& states,
            std::size_t count, bool fullMapThreadAuditProved)
        {
            _complete = false;
            _creatures = {};
            if (_poisoned || !fullMapThreadAuditProved ||
                count > MaxKnownCreatures)
                return false;

            for (std::size_t i = 0; i < count; ++i)
            {
                Policy::State const& candidate = states[i];
                if (!candidate.alive || !candidate.creatureInstanceToken ||
                    !candidate.generation || candidate.uncertain ||
                    WasCreatureRetired(candidate.creatureInstanceToken))
                    return false;

                for (std::size_t j = 0; j < i; ++j)
                    if (states[j].creatureInstanceToken == candidate.creatureInstanceToken)
                        return false;

                for (std::size_t j = 0; j < Policy::MaxTrackedAttackers; ++j)
                {
                    std::uint64_t actor = candidate.verifiedAttackers[j];
                    if (!actor)
                        continue;
                    if (WasLoginExpired(actor))
                        return false;
                    for (std::size_t k = 0; k < j; ++k)
                        if (candidate.verifiedAttackers[k] == actor)
                            return false;
                }
            }

            for (std::size_t i = 0; i < count; ++i)
                _creatures[i] = states[i];
            _complete = true;
            return true;
        }

        // Never trust a logout/identity-loss callback to emit AttackStop.
        // Invalidate ALL positive decisions before scanning known creatures.
        // Unknown actor, incomplete index and overflow cannot imply safety.
        ExpiryFanout ExpireLogin(std::uint64_t loginEpoch)
        {
            ExpiryFanout result{};
            _complete = false;
            if (!loginEpoch || !RememberExpiredLogin(loginEpoch))
            {
                _poisoned = true;
                result.registryPermanentlyDisabled = true;
                return result;
            }

            for (Policy::State& creature : _creatures)
            {
                if (!creature.creatureInstanceToken)
                    continue;

                bool contained = false;
                for (auto actor : creature.verifiedAttackers)
                    if (actor == loginEpoch)
                        contained = true;
                if (!contained)
                    continue;

                result.actorWasIndexed = true;
                result.affectedCreatureTokens[result.affectedCount++] =
                    creature.creatureInstanceToken;
                creature = Policy::OnEvent(
                    creature, Policy::Event::ActorIncarnationExpired,
                    loginEpoch).state;
            }
            return result;
        }

        // A removed creature incarnation is permanently retired for this
        // index lifetime. A later "complete" snapshot may NEVER reintroduce
        // its token, even if a DB spawn/GUID is reused. Capacity exhaustion
        // denies all future concealment rather than forgetting old identity.
        bool RemoveCreature(std::uint64_t creatureToken)
        {
            _complete = false;
            if (!creatureToken || !RememberRetiredCreature(creatureToken))
            {
                _poisoned = true;
                return false;
            }

            bool wasIndexed = false;
            for (Policy::State& creature : _creatures)
                if (creature.creatureInstanceToken == creatureToken)
                {
                    creature = {};
                    wasIndexed = true;
                }
            return wasIndexed;
        }

    private:
        bool WasLoginExpired(std::uint64_t token) const
        {
            for (auto expired : _expiredLogins)
                if (token != 0 && expired == token)
                    return true;
            return false;
        }

        bool WasCreatureRetired(std::uint64_t token) const
        {
            for (auto retired : _retiredCreatures)
                if (token != 0 && retired == token)
                    return true;
            return false;
        }

        bool RememberRetiredCreature(std::uint64_t token)
        {
            if (WasCreatureRetired(token))
                return true; // Repeated remove does not revive a creature.
            for (auto& retired : _retiredCreatures)
                if (retired == 0)
                {
                    retired = token;
                    return true;
                }
            return false; // No eviction: exhausted ledger poisons this index.
        }

        bool RememberExpiredLogin(std::uint64_t token)
        {
            if (WasLoginExpired(token))
                return true; // Repeated expiry must never resurrect a token.
            for (auto& expired : _expiredLogins)
                if (expired == 0)
                {
                    expired = token;
                    return true;
                }
            return false; // Exhaustion permanently disables concealment.
        }

        std::array<Policy::State, MaxKnownCreatures> _creatures{};
        std::array<std::uint64_t, MaxExpiredLogins> _expiredLogins{};
        std::array<std::uint64_t, MaxRetiredCreatures> _retiredCreatures{};
        bool _complete = false;
        bool _poisoned = false;
    };
}
#endif
