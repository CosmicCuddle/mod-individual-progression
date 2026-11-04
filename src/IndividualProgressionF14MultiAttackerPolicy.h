/*
 * CosmicCuddle IP F14 — multiple-verified-attacker lifecycle model.
 *
 * PURE, UNWIRED: no Core hook, map thread, Playerbots query, database,
 * WorldPacket, visible Unit reference or actual live side effect.
 *
 * The caller MUST validate each identity as a positively verified current
 * BE/Draenei RandomBot and supply a unique login/incarnation token (not
 * just a recyclable player GUID). This only tracks candidates; it never
 * authorises hiding: threat, tap, rights, observer safety and a same-epoch
 * immediate refresh still require separate independent proofs.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_MULTI_ATTACKER_POLICY_H
#define INDIVIDUAL_PROGRESSION_F14_MULTI_ATTACKER_POLICY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace IndividualProgressionF14MultiAttackerPolicy
{
    constexpr std::size_t MaxTrackedAttackers = 16;

    enum class Event : std::uint8_t
    {
        VerifiedAttackBegin,
        AttackEnd,
        ActorIncarnationExpired,
        CreatureDied,
        CreatureDespawned,
        CreatureRespawned,
        UnverifiedOrHumanParticipant,
        ThreatRightsOrIdentityChanged
    };

    struct State
    {
        // Must uniquely identify the living creature INSTANCE, not its spawn
        // DB ID/template or a reused GUID after death/respawn.
        std::uint64_t creatureInstanceToken = 0;
        std::uint32_t generation = 0;
        bool alive = false;
        bool completeCurrentParticipantProof = false;
        bool uncertain = false;
        // Each actor token must be unique across reconnect/relogin. Zero is
        // always invalid; no raw Unit* or Player* may enter this container.
        std::array<std::uint64_t, MaxTrackedAttackers> verifiedAttackers{};
    };

    struct Transition
    {
        State state{};
        bool invalidateObserverCreatureProof = true;
        bool requestImmediateVisibilityRefresh = true;
        bool failVisibleUntilReverified = true;
    };

    constexpr unsigned ActiveAttackers(State const& state)
    {
        unsigned n = 0;
        for (auto token : state.verifiedAttackers)
            if (token != 0)
                ++n;
        return n;
    }

    constexpr bool HasVerifiedAttackerCandidates(State const& state)
    {
        return state.creatureInstanceToken != 0 && state.generation != 0 &&
               state.alive && !state.uncertain &&
               state.completeCurrentParticipantProof && ActiveAttackers(state) != 0;
    }

    constexpr Transition OnEvent(State previous, Event event,
                                 std::uint64_t actorToken = 0)
    {
        Transition out{};
        out.state = previous;
        // Every event invalidates positive per-observer proof, even if the
        // roster looks unchanged. Only the map-thread owner may re-verify.
        out.state.completeCurrentParticipantProof = false;

        if (event == Event::CreatureRespawned)
        {
            // Caller must provide a NEW creature incarnation token. A reused
            // token, even on a fresh spawn of the same creature, fails closed.
            const bool newIdentity = actorToken != 0 &&
                actorToken != previous.creatureInstanceToken;
            out.state = {};
            out.state.creatureInstanceToken = newIdentity ? actorToken : 0;
            out.state.generation = newIdentity ? 1 : 0;
            out.state.alive = newIdentity;
            out.state.uncertain = !newIdentity;
            return out;
        }

        // Invalidated epoch is never allowed to wrap back to a stale proof.
        if (out.state.creatureInstanceToken == 0 ||
            out.state.generation == 0 ||
            out.state.generation == std::numeric_limits<std::uint32_t>::max())
        {
            out.state = {};
            out.state.uncertain = true;
            return out;
        }
        ++out.state.generation;

        if (event == Event::CreatureDied || event == Event::CreatureDespawned)
        {
            out.state.alive = false;
            out.state.verifiedAttackers = {};
            return out;
        }

        if (!out.state.alive)
        {
            out.state.uncertain = true;
            return out;
        }

        if (event == Event::UnverifiedOrHumanParticipant ||
            event == Event::ThreatRightsOrIdentityChanged)
        {
            // There may now be a human, Altbot, different owner or pending
            // threat/loot/quest claim. The whole snapshot must be refreshed.
            out.state.uncertain = true;
            return out;
        }

        if (actorToken == 0)
        {
            out.state.uncertain = true;
            return out;
        }

        if (event == Event::ActorIncarnationExpired)
        {
            // Login identity can expire BEFORE Core emits AttackStop, or the
            // stop may never arrive (logout, map removal, pet/DoT teardown).
            // Drop this incarnation but deny concealment even if another
            // verified bot remains: actor/threat/tap/rights must be rebuilt.
            // An unknown expiry still invalidates the complete snapshot.
            for (auto& known : out.state.verifiedAttackers)
                if (known == actorToken)
                    known = 0;
            out.state.uncertain = true;
            return out;
        }

        if (event == Event::VerifiedAttackBegin)
        {
            // Duplicate start must not double-count the attacker.
            for (auto known : out.state.verifiedAttackers)
                if (known == actorToken)
                    return out;

            for (auto& known : out.state.verifiedAttackers)
                if (known == 0)
                {
                    known = actorToken;
                    return out;
                }

            // Overflow cannot silently exclude a combatant: fail visible
            // until the exact complete participant list can be rebuilt.
            out.state.uncertain = true;
            return out;
        }

        if (event == Event::AttackEnd)
        {
            for (auto& known : out.state.verifiedAttackers)
                if (known == actorToken)
                {
                    known = 0;
                    return out;
                }

            // An unknown end means the event stream/identity is incomplete.
            out.state.uncertain = true;
            return out;
        }

        out.state.uncertain = true;
        return out;
    }

    // Recover after an unknown/human combatant has LEFT only by replacing the
    // WHOLE roster using a fresh, complete, map-thread actor/rights snapshot.
    // An old snapshot/generation must never resurrect an earlier hide decision.
    // The returned state still lacks positive observer/rights/refresh proof.
    constexpr Transition ReplaceWithFullyVerifiedRoster(
        State previous, std::uint64_t creatureInstance,
        std::uint32_t expectedGeneration,
        std::array<std::uint64_t, MaxTrackedAttackers> const& roster,
        std::size_t rosterCount, bool allActorsAndRightsVerified)
    {
        Transition out{};
        out.state = previous;
        out.state.completeCurrentParticipantProof = false;

        if (!previous.alive || creatureInstance == 0 ||
            creatureInstance != previous.creatureInstanceToken ||
            expectedGeneration == 0 ||
            expectedGeneration != previous.generation ||
            previous.generation == std::numeric_limits<std::uint32_t>::max() ||
            rosterCount > MaxTrackedAttackers || !allActorsAndRightsVerified)
        {
            out.state.uncertain = true;
            return out;
        }

        State replacement{};
        replacement.creatureInstanceToken = creatureInstance;
        replacement.generation = expectedGeneration + 1;
        replacement.alive = true;
        for (std::size_t i = 0; i < rosterCount; ++i)
        {
            if (roster[i] == 0)
            {
                out.state.uncertain = true;
                return out;
            }

            for (std::size_t j = 0; j < i; ++j)
                if (roster[j] == roster[i])
                {
                    out.state.uncertain = true;
                    return out;
                }

            replacement.verifiedAttackers[i] = roster[i];
        }

        out.state = replacement;
        return out;
    }

    // Proof comes from a separate COMPLETE, same map-thread participant,
    // threat, rights, identity and eligibility snapshot. Setting it does not
    // approve concealment and cannot bypass uncertainty or an empty roster.
    constexpr State ReverifyCompleteParticipantFacts(State state,
                                                     std::uint64_t creatureInstance,
                                                     std::uint32_t generation,
                                                     bool allActorsAndRightsVerified)
    {
        state.completeCurrentParticipantProof = false;
        if (creatureInstance != 0 &&
            state.creatureInstanceToken == creatureInstance &&
            generation != 0 && state.generation == generation &&
            state.alive && !state.uncertain &&
            ActiveAttackers(state) > 0 && allActorsAndRightsVerified)
            state.completeCurrentParticipantProof = true;
        return state;
    }
}

#endif
