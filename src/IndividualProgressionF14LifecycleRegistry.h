/*
 * CosmicCuddle IP F14 — verified-RandomBot attack/corpse lifecycle registry.
 *
 * PURE bookkeeping. The caller converts every Core callback pointer into
 * plain value tokens BEFORE calling in, and holds one mutex around every
 * call. This header never stores Unit*, Player*, Creature* or Map*, never
 * queries Playerbots, never sends packets, never writes stages, SQL or
 * settings, and NEVER authorises concealment: a fresh event-built roster
 * has no complete threat/tap/loot/quest-rights proof, so
 * HasVerifiedAttackerCandidates() stays false for every entry it creates.
 *
 * Token rules (no recycling, so no "expired" ledgers are needed):
 *  - actor token  = the nonzero, monotonic, never-wrapping per-login epoch
 *                   issued by IndividualProgression for a positively
 *                   verified headless BE/Draenei RandomBot. 0 = unverified
 *                   (human, Altbot, GM, excluded, pet/minion, creature,
 *                   unknown or expired). An unverified participant never
 *                   gets an entry; it only makes an existing entry uncertain.
 *  - creature token = issued here, monotonic, never reused. A new token is
 *                   issued whenever an entry is created; corpse and respawn
 *                   retire it. The caller stamps it on the live Creature
 *                   object, so a recycled GUID/instance id or a new Creature
 *                   object can never inherit an old roster.
 *
 * Every overflow (creatures, attackers, tokens) or impossible state poisons
 * the WHOLE registry: all mutators become no-ops and every query answers
 * "not a candidate" (fail VISIBLE) until Reset().
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_LIFECYCLE_REGISTRY_H
#define INDIVIDUAL_PROGRESSION_F14_LIFECYCLE_REGISTRY_H

#include "IndividualProgressionF14MultiAttackerPolicy.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>

namespace IndividualProgressionF14LifecycleRegistry
{
    namespace Policy = IndividualProgressionF14MultiAttackerPolicy;

    constexpr std::size_t DefaultMaxCreatures = 4096;

    // Identity of the live creature object as seen by the map thread.
    struct CreatureKey
    {
        std::uint32_t mapId = 0;
        std::uint32_t instanceId = 0;
        std::uint64_t guidRaw = 0;

        bool operator==(CreatureKey const& o) const
        {
            return mapId == o.mapId && instanceId == o.instanceId && guidRaw == o.guidRaw;
        }
        bool IsValid() const { return guidRaw != 0; }
    };

    struct CreatureKeyHash
    {
        std::size_t operator()(CreatureKey const& k) const
        {
            std::size_t h = std::hash<std::uint64_t>{}(k.guidRaw);
            h ^= std::hash<std::uint64_t>{}((std::uint64_t(k.mapId) << 32) | k.instanceId) +
                 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return h;
        }
    };

    enum class PoisonReason : std::uint8_t
    {
        None,
        CreatureCapacity,
        CreatureTokenExhausted,
        StampKeyMismatch,
        InvalidKey
    };

    struct Counters
    {
        std::uint64_t verifiedBegins = 0;
        std::uint64_t verifiedEnds = 0;
        std::uint64_t unverifiedMarks = 0;
        std::uint64_t ignoredWithoutEntry = 0;
        std::uint64_t expiryFanout = 0;
        std::uint64_t corpseRetired = 0;
        std::uint64_t respawnRetired = 0;
        std::uint64_t evadeRetired = 0;
        std::uint64_t staleKeyReplaced = 0;
        std::uint64_t poisonedCalls = 0;
    };

    struct Entry
    {
        // Registry key and creature stamp. Kept separately because the
        // policy zeroes state.creatureInstanceToken on generation exhaustion.
        std::uint64_t token = 0;
        CreatureKey key{};
        Policy::State state{};
    };

    // Result of a participant-begin call. The caller writes `stamp` onto the
    // live Creature object (0 = remove any stamp).
    struct BeginResult
    {
        std::uint64_t stamp = 0;
        bool created = false;
    };

    class Registry
    {
    public:
        explicit Registry(std::size_t maxCreatures = DefaultMaxCreatures)
            : _maxCreatures(maxCreatures == 0 ? 1 : maxCreatures) { }

        // Clears every roster and lifts poison. Tokens keep counting upward,
        // so stamps left on live creatures can never match a new entry.
        void Reset()
        {
            _entries.clear();
            _byKey.clear();
            _poison = PoisonReason::None;
            _counters = {};
        }

        bool IsPoisoned() const { return _poison != PoisonReason::None; }
        PoisonReason GetPoisonReason() const { return _poison; }
        std::size_t Size() const { return _entries.size(); }
        std::size_t Capacity() const { return _maxCreatures; }
        Counters const& GetCounters() const { return _counters; }

        // Read-only. Always false for event-built rosters (no complete
        // participant/rights proof); false whenever poisoned or unknown.
        bool IsConcealmentCandidate(std::uint64_t stamp) const
        {
            if (IsPoisoned() || stamp == 0)
                return false;
            auto const it = _entries.find(stamp);
            return it != _entries.end() && Policy::HasVerifiedAttackerCandidates(it->second.state);
        }

        // Read-only copy for tests/diagnostics. Returns false if unknown.
        bool Lookup(std::uint64_t stamp, Entry& out) const
        {
            if (IsPoisoned() || stamp == 0)
                return false;
            auto const it = _entries.find(stamp);
            if (it == _entries.end())
                return false;
            out = it->second;
            return true;
        }

        // A participant starts attacking a tracked-eligible world creature.
        // actorToken 0 = unverified participant. `others` are the tokens of
        // every OTHER current attacker of that creature (0 = unverified),
        // sampled synchronously by the caller. Only a VERIFIED begin may
        // create an entry; it then starts from that full attacker snapshot.
        BeginResult OnParticipantBegin(CreatureKey const& key, std::uint64_t stamp,
                                       std::uint64_t actorToken,
                                       std::uint64_t const* others, std::size_t otherCount)
        {
            BeginResult result{};
            result.stamp = stamp;
            if (Guard())
                return result;
            if (!key.IsValid())
            {
                Poison(PoisonReason::InvalidKey);
                result.stamp = 0;
                return result;
            }

            Entry* entry = Find(stamp, key);
            if (IsPoisoned())
            {
                result.stamp = 0;
                return result;
            }

            if (!entry)
            {
                if (actorToken == 0)
                {
                    // No verified bot involved yet: nothing to track.
                    ++_counters.ignoredWithoutEntry;
                    result.stamp = 0;
                    return result;
                }
                entry = Create(key);
                if (!entry)
                {
                    result.stamp = 0;
                    return result;
                }
                result.created = true;
                result.stamp = entry->state.creatureInstanceToken;

                // Seed from the creature's complete current attacker set.
                if (otherCount > Policy::MaxTrackedAttackers || (otherCount && !others))
                    Apply(*entry, Policy::Event::UnverifiedOrHumanParticipant, 0);
                else
                    for (std::size_t i = 0; i < otherCount; ++i)
                    {
                        if (others[i] == 0 || others[i] == actorToken)
                        {
                            if (others[i] == 0)
                            {
                                ++_counters.unverifiedMarks;
                                Apply(*entry, Policy::Event::UnverifiedOrHumanParticipant, 0);
                            }
                            continue;
                        }
                        Apply(*entry, Policy::Event::VerifiedAttackBegin, others[i]);
                    }
            }

            if (actorToken == 0)
            {
                ++_counters.unverifiedMarks;
                Apply(*entry, Policy::Event::UnverifiedOrHumanParticipant, 0);
            }
            else
            {
                ++_counters.verifiedBegins;
                Apply(*entry, Policy::Event::VerifiedAttackBegin, actorToken);
            }
            result.stamp = entry->token;
            return result;
        }

        // A participant stops attacking (or switches away from) a creature.
        // Never creates an entry.
        void OnParticipantEnd(CreatureKey const& key, std::uint64_t stamp, std::uint64_t actorToken)
        {
            if (Guard())
                return;
            Entry* entry = Find(stamp, key);
            if (!entry)
            {
                ++_counters.ignoredWithoutEntry;
                return;
            }
            if (actorToken == 0)
            {
                // A human/unknown leaving cannot restore certainty; only a
                // future complete map-thread roster rebuild could.
                ++_counters.unverifiedMarks;
                Apply(*entry, Policy::Event::UnverifiedOrHumanParticipant, 0);
                return;
            }
            ++_counters.verifiedEnds;
            Apply(*entry, Policy::Event::AttackEnd, actorToken);
        }

        // The tracked creature itself picks a new victim. A human, pet,
        // creature or unverified victim is a new participant.
        void OnCreatureTargetChange(CreatureKey const& key, std::uint64_t stamp, bool victimIsVerifiedBot)
        {
            if (Guard())
                return;
            Entry* entry = Find(stamp, key);
            if (!entry || victimIsVerifiedBot)
                return;
            ++_counters.unverifiedMarks;
            Apply(*entry, Policy::Event::UnverifiedOrHumanParticipant, 0);
        }

        // The tracked creature stopped attacking (evade, combat end, death
        // start). Retire the entry only once no verified attacker remains.
        // Returns true when the caller must clear the creature's stamp.
        bool OnCreatureStoppedAttacking(CreatureKey const& key, std::uint64_t stamp)
        {
            if (Guard())
                return false;
            Entry* entry = Find(stamp, key);
            if (!entry || Policy::ActiveAttackers(entry->state) != 0)
                return false;
            ++_counters.evadeRetired;
            Erase(*entry);
            return true;
        }

        // Corpse-ready or respawn: this incarnation is over. Always retire.
        void OnCreatureCorpseOrRespawn(CreatureKey const& key, std::uint64_t stamp, bool respawn)
        {
            if (Guard())
                return;
            Entry* entry = Find(stamp, key);
            if (!entry)
                return;
            if (respawn)
                ++_counters.respawnRetired;
            else
            {
                Apply(*entry, Policy::Event::CreatureDied, 0);
                ++_counters.corpseRetired;
            }
            Erase(*entry);
        }

        // Logout / relogin / identity loss of a login epoch. Never trusts an
        // AttackStop to arrive. Returns the number of affected creatures.
        std::size_t ExpireActor(std::uint64_t actorToken)
        {
            if (Guard() || actorToken == 0)
                return 0;
            std::size_t affected = 0;
            for (auto& pair : _entries)
            {
                bool contained = false;
                for (auto known : pair.second.state.verifiedAttackers)
                    if (known == actorToken)
                        contained = true;
                if (!contained)
                    continue;
                Apply(pair.second, Policy::Event::ActorIncarnationExpired, actorToken);
                ++affected;
            }
            _counters.expiryFanout += affected;
            return affected;
        }

    private:
        bool Guard()
        {
            if (!IsPoisoned())
                return false;
            ++_counters.poisonedCalls;
            return true;
        }

        void Poison(PoisonReason reason)
        {
            if (_poison == PoisonReason::None)
                _poison = reason;
            _entries.clear();
            _byKey.clear();
        }

        Entry* Find(std::uint64_t stamp, CreatureKey const& key)
        {
            if (stamp == 0)
                return nullptr;
            auto const it = _entries.find(stamp);
            if (it == _entries.end())
                return nullptr; // stale stamp from before Reset/retirement
            if (!(it->second.key == key))
            {
                // A stamp travels with one Creature object; a key change
                // means the caller or Core broke an invariant.
                Poison(PoisonReason::StampKeyMismatch);
                return nullptr;
            }
            return &it->second;
        }

        Entry* Create(CreatureKey const& key)
        {
            // Same GUID in the same map instance cannot be two live
            // creatures: an older entry is a stale incarnation whose
            // removal hook never fired. Drop it, never merge it.
            auto const old = _byKey.find(key);
            if (old != _byKey.end())
            {
                ++_counters.staleKeyReplaced;
                _entries.erase(old->second);
                _byKey.erase(old);
            }
            if (_entries.size() >= _maxCreatures)
            {
                Poison(PoisonReason::CreatureCapacity);
                return nullptr;
            }
            if (_nextToken == std::numeric_limits<std::uint64_t>::max())
            {
                Poison(PoisonReason::CreatureTokenExhausted);
                return nullptr;
            }
            std::uint64_t const token = ++_nextToken;
            Entry entry{};
            entry.token = token;
            entry.key = key;
            entry.state.creatureInstanceToken = token;
            entry.state.generation = 1;
            entry.state.alive = true;
            auto const inserted = _entries.emplace(token, entry);
            _byKey[key] = token;
            return &inserted.first->second;
        }

        void Apply(Entry& entry, Policy::Event event, std::uint64_t actorToken)
        {
            entry.state = Policy::OnEvent(entry.state, event, actorToken).state;
        }

        void Erase(Entry& entry)
        {
            // Copy first: erasing invalidates the reference.
            std::uint64_t const token = entry.token;
            CreatureKey const key = entry.key;
            _byKey.erase(key);
            _entries.erase(token);
        }

        std::size_t _maxCreatures;
        std::unordered_map<std::uint64_t, Entry> _entries;
        std::unordered_map<CreatureKey, std::uint64_t, CreatureKeyHash> _byKey;
        std::uint64_t _nextToken = 0;
        PoisonReason _poison = PoisonReason::None;
        Counters _counters{};
    };
}

#endif
