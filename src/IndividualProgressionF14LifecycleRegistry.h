/*
 * CosmicCuddle IP F14 — verified-RandomBot attack/corpse lifecycle registry.
 *
 * PURE bookkeeping. The caller converts every Core callback pointer into
 * plain value tokens BEFORE calling in, and holds one mutex around every
 * call. This header never stores Unit*, Player*, Creature* or Map*, never
 * queries Playerbots, never sends packets, never writes stages, SQL or
 * settings, and NEVER authorises concealment. No roster built here ever
 * carries the pure model's participant proof: HasVerifiedAttackerCandidates()
 * and IsConcealmentCandidate() stay false for every entry. Core threat /
 * combat / tap snapshots are recorded only as NON-AUTHORITATIVE
 * ConservativeEvidence: they can add uncertainty, never remove it, and are
 * not a complete census of participants or rights (CS01 acceptance rule).
 *
 * Token rules (no recycling, so no "expired" ledgers are needed):
 *  - actor token  = the nonzero, monotonic, never-wrapping per-login epoch
 *                   issued by IndividualProgression for a positively
 *                   verified headless BE/Draenei RandomBot. 0 = unverified
 *                   (human, Altbot, GM, excluded, pet/minion, creature,
 *                   unknown or expired). An unverified participant never
 *                   gets an entry; it only makes an existing entry uncertain.
 *  - creature token = issued here, monotonic, never reused. A new token is
 *                   issued whenever an entry is created; corpse, respawn
 *                   and removal from the world retire it. The caller stamps
 *                   it on the live Creature object, so a recycled GUID or
 *                   instance id or a new Creature object can never inherit
 *                   an old roster.
 *
 * Every overflow (creatures, attackers, tokens) or impossible state poisons
 * the WHOLE registry: all mutators become no-ops and every query answers
 * "not a candidate" (fail VISIBLE) until Reset().
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_LIFECYCLE_REGISTRY_H
#define INDIVIDUAL_PROGRESSION_F14_LIFECYCLE_REGISTRY_H

#include "IndividualProgressionF14CombatVisibilityPolicy.h"
#include "IndividualProgressionF14MultiAttackerPolicy.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>

namespace IndividualProgressionF14LifecycleRegistry
{
    namespace Policy = IndividualProgressionF14MultiAttackerPolicy;
    namespace Visibility = IndividualProgressionF14CombatVisibilityPolicy;

    constexpr std::size_t DefaultMaxCreatures = 4096;
    // Distinct participants a complete snapshot may list. More => incomplete.
    constexpr std::size_t MaxSnapshotParticipants = 32;
    // Total entries a snapshot may VISIT across the threat list, PvE combat
    // refs and attacker set, duplicates included. This bounds the time the
    // global lifecycle mutex is held, independent of unique-token count.
    constexpr std::size_t MaxSnapshotVisits = 64;
    // Distinct verified OWNER epochs a snapshot may record for minions.
    constexpr std::size_t MaxSnapshotMinionOwners = 16;

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
        std::uint64_t removedRetired = 0;
        std::uint64_t staleKeyReplaced = 0;
        std::uint64_t poisonedCalls = 0;
        std::uint64_t leftCombatRetired = 0;
        std::uint64_t evidenceInvalidations = 0;
        std::uint64_t assistInvalidations = 0;
        std::uint64_t snapshotsApplied = 0;
        std::uint64_t snapshotsOnlyVerifiedBots = 0;
        std::uint64_t snapshotsOnlyVerifiedBotsOrOwnedMinions = 0;
        std::uint64_t minionOwnerExpiries = 0;
    };

    // Creature-side facts sampled synchronously on the owning map thread by
    // the caller, already converted to value tokens (0 = unverified: human,
    // Altbot, GM, pet/minion/totem, creature or unknown). Every flag defaults
    // to the fail-visible value.
    struct ParticipantSnapshot
    {
        // Every list below was enumerated in full, within its bound, and no
        // unsupported reference kind (e.g. PvP combat refs) was present.
        bool complete = false;
        bool plainUnscriptedNonEliteNpcScope = false;
        bool alive = false;
        bool inCombat = false;
        // Loot recipient is nobody or a verified bot, and no loot group.
        bool tapHolderNoneOrVerifiedBot = false;

        // Units currently attacking the creature (its attacker set).
        std::array<std::uint64_t, Policy::MaxTrackedAttackers + 1> attackers{};
        std::size_t attackerCount = 0;
        // Every threat-list victim (offline included) and PvE combat ref.
        std::array<std::uint64_t, MaxSnapshotParticipants + 1> participants{};
        std::size_t participantCount = 0;
        // Stage 3: SEPARATE minion-owner evidence. Each entry is the current
        // verified login epoch of the OWNER of a pet/guardian/totem/summon
        // whose whole value-only lineage matched. A minion is never a
        // participant, attacker or roster member and never an eligible bot.
        std::array<std::uint64_t, MaxSnapshotMinionOwners + 1> minionOwners{};
        std::size_t minionOwnerCount = 0;
        std::size_t minionVisits = 0;
    };

    // Stage 3: value-only lineage of a pet/guardian/totem/summon, copied
    // from Core GUID fields (raw values, no pointers). Defaults fail.
    struct MinionLineage
    {
        bool minionType = false;          // pet, guardian, totem or summon
        bool vehicleOrPossessed = true;
        bool ownerIsPlayer = false;
        std::uint64_t owner = 0;          // GetOwnerGUID raw
        std::uint64_t charmer = 0;        // GetCharmerGUID raw (0 = none)
        std::uint64_t creator = 0;        // GetCreatorGUID raw (0 = none)
        bool summon = false;
        bool summonDataAvailable = false; // ToTempSummon() succeeded
        std::uint64_t summoner = 0;       // GetSummonerGUID raw (0 = none)
    };

    // True only when every GUID in the lineage agrees with ONE player owner.
    // Any charmer/creator/summoner mismatch, missing owner, non-player
    // owner, possession, vehicle or missing summon data => false.
    inline bool LineageMatchesSingleOwner(MinionLineage const& l)
    {
        if (!l.minionType || l.vehicleOrPossessed || !l.ownerIsPlayer || l.owner == 0)
            return false;
        if (l.charmer != 0 && l.charmer != l.owner)
            return false;
        if (l.creator != 0 && l.creator != l.owner)
            return false;
        if (l.summon && (!l.summonDataAvailable || (l.summoner != 0 && l.summoner != l.owner)))
            return false;
        return true;
    }

    // Minion-owner evidence token: the owner's CURRENT verified login epoch
    // (0 if the owner is human/Altbot/GM/unverified/disconnected), and only
    // if the lineage matched. Never a first-party bot token.
    inline std::uint64_t MinionOwnerEvidence(MinionLineage const& l, std::uint64_t ownerCurrentEpoch)
    {
        return LineageMatchesSingleOwner(l) ? ownerCurrentEpoch : 0;
    }

    // Pure, bounded snapshot builder. The runtime feeds it one token per
    // visited entry and MUST `break` its loop as soon as a call returns
    // false. It stops (and marks the snapshot incomplete) on: the total
    // visit budget, an unverified/unknown participant, participant or
    // attacker overflow, or an explicit Fail() (API unavailable, PvP ref).
    // An incomplete snapshot always fails visible in the registry.
    class SnapshotCollector
    {
    public:
        SnapshotCollector() { _snap.complete = true; }

        void Fail()
        {
            _snap.complete = false;
            _stopped = true;
        }

        bool Stopped() const { return _stopped; }
        std::size_t Visits() const { return _visits; }

        // A threat-list victim or PvE combat ref (0 = unverified/unknown).
        bool AddParticipant(std::uint64_t token)
        {
            if (_stopped)
                return false;
            if (++_visits > MaxSnapshotVisits)
            {
                Fail();
                return false;
            }
            if (token == 0)
            {
                Append(_snap.participants, _snap.participantCount, 0);
                Fail();
                return false;
            }
            for (std::size_t i = 0; i < _snap.participantCount; ++i)
                if (_snap.participants[i] == token)
                    return true; // duplicate: counted as a visit only
            if (!Append(_snap.participants, _snap.participantCount, token))
            {
                Fail();
                return false;
            }
            return true;
        }

        // A unit currently attacking the creature: participant AND attacker.
        bool AddAttacker(std::uint64_t token)
        {
            if (!AddParticipant(token))
                return false;
            if (!Append(_snap.attackers, _snap.attackerCount, token))
            {
                Fail();
                return false;
            }
            return true;
        }

        // A pet/guardian/totem/summon whose lineage matched a verified owner
        // (ownerEpoch != 0). ownerEpoch 0 = unknown lineage: stop, visible.
        // Spends the same visit budget BEFORE de-duplication.
        bool AddMinion(std::uint64_t ownerEpoch)
        {
            if (_stopped)
                return false;
            if (++_visits > MaxSnapshotVisits)
            {
                Fail();
                return false;
            }
            if (ownerEpoch == 0)
            {
                Append(_snap.participants, _snap.participantCount, 0);
                Fail();
                return false;
            }
            ++_snap.minionVisits;
            for (std::size_t i = 0; i < _snap.minionOwnerCount; ++i)
                if (_snap.minionOwners[i] == ownerEpoch)
                    return true;
            if (!Append(_snap.minionOwners, _snap.minionOwnerCount, ownerEpoch))
            {
                Fail();
                return false;
            }
            return true;
        }

        // Copies the enumerated lists out; scalar facts are set by the caller.
        ParticipantSnapshot Take() const { return _snap; }

    private:
        template <std::size_t N>
        static bool Append(std::array<std::uint64_t, N>& arr, std::size_t& count, std::uint64_t token)
        {
            // Keep the final slot free so `count` can never exceed the bound
            // silently; a full array means overflow.
            if (count + 1 >= N)
                return false;
            arr[count++] = token;
            return true;
        }

        ParticipantSnapshot _snap{};
        std::size_t _visits = 0;
        bool _stopped = false;
    };

    // Last snapshot result stored per entry. Valid only for the policy
    // generation it was taken at; any later event makes it stale.
    struct SnapshotFacts
    {
        bool valid = false;
        std::uint32_t generation = 0;
        bool enumerated = false;
        bool plainUnscriptedNonEliteNpcScope = false;
        bool alive = false;
        bool inCombat = false;
        bool tapHolderNoneOrVerifiedBot = false;
        bool observedOnlyVerifiedBots = false;
        bool observedOnlyVerifiedBotsOrOwnedMinions = false;
        // Owner epochs the minion evidence depended on (for owner expiry).
        std::array<std::uint64_t, MaxSnapshotMinionOwners> minionOwners{};
        std::size_t minionOwnerCount = 0;
    };

    // NON-AUTHORITATIVE bookkeeping evidence. It is NOT a census of every
    // participant, NOT proof of tap/loot/quest/kill/skin rights, NOT proof
    // that no delayed damage/heal/credit exists, and must NEVER be consumed
    // for visibility. Every flag is false unless observed at the current
    // generation. A future, separate complete rights audit is mandatory.
    struct ConservativeEvidence
    {
        std::uint64_t creatureToken = 0;
        std::uint32_t generation = 0;
        bool current = false;
        bool rosterUncertain = true;
        unsigned verifiedAttackers = 0;
        bool enumeratedWithinBounds = false;
        bool plainUnscriptedNonEliteNpcScope = false;
        bool alive = false;
        bool inCombat = false;
        bool threatCombatAndAttackersObservedOnlyVerifiedBots = false;
        // Stage 3, separate and weaker: only verified bots and/or minions
        // whose value-only lineage matched a CURRENT verified owner epoch.
        // Minion presence still makes the roster uncertain.
        bool observedOnlyVerifiedBotsOrVerifiedOwnerMinions = false;
        unsigned verifiedOwnerMinionOwners = 0;
        bool tapHolderObservedNoneOrVerifiedBot = false;
    };

    struct Entry
    {
        // Registry key and creature stamp. Kept separately because the
        // policy zeroes state.creatureInstanceToken on generation exhaustion.
        std::uint64_t token = 0;
        CreatureKey key{};
        Policy::State state{};
        SnapshotFacts facts{};
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

        // Read-only. Always false: no roster built here ever carries the
        // pure model's participant/rights proof (event rosters and
        // snapshots alike); false whenever poisoned or unknown.
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

        // The Creature object is leaving its map (despawn, grid/instance
        // unload, RemoveFromMap). Always retire, before the object can be
        // destroyed or its GUID reused. Idempotent with corpse/respawn.
        void OnCreatureRemovedFromWorld(CreatureKey const& key, std::uint64_t stamp)
        {
            if (Guard())
                return;
            Entry* entry = Find(stamp, key);
            if (!entry)
                return;
            ++_counters.removedRetired;
            Erase(*entry);
        }

        // The tracked creature left combat or entered evade: this combat
        // incarnation is over. Retire regardless of roster or uncertainty;
        // a later attack starts a fresh entry with no proof.
        bool OnCreatureLeftCombat(CreatureKey const& key, std::uint64_t stamp)
        {
            if (Guard())
                return false;
            Entry* entry = Find(stamp, key);
            if (!entry)
                return false;
            ++_counters.leftCombatRetired;
            Erase(*entry);
            return true;
        }

        // Damage, periodic tick or heal involving the tracked creature, from
        // `sourceToken` (0 = unverified). A rostered verified source changes
        // nothing. Anything else is a participant the roster may not know:
        // fail visible until a later complete snapshot proves otherwise.
        // NEVER take a snapshot in the same event: the source's threat/combat
        // reference may not exist yet. Returns true if state changed.
        bool OnParticipantEvidence(CreatureKey const& key, std::uint64_t stamp, std::uint64_t sourceToken)
        {
            if (Guard())
                return false;
            Entry* entry = Find(stamp, key);
            if (!entry)
                return false;
            if (sourceToken != 0 && Rostered(entry->state, sourceToken))
                return false;
            ++_counters.evidenceInvalidations;
            Apply(*entry, sourceToken == 0 ? Policy::Event::UnverifiedOrHumanParticipant
                                           : Policy::Event::ThreatRightsOrIdentityChanged, sourceToken);
            return true;
        }

        // A heal/HoT on a rostered bot gives the healer threat on every
        // creature that bot is fighting, without any creature-side event.
        // Mark each such creature uncertain unless the healer is that same
        // verified bot or is already rostered there. Returns affected count.
        std::size_t OnAssistOfActor(std::uint64_t receiverToken, std::uint64_t helperToken)
        {
            if (Guard() || receiverToken == 0 || receiverToken == helperToken)
                return 0;
            std::size_t affected = 0;
            for (auto& pair : _entries)
            {
                if (!Rostered(pair.second.state, receiverToken))
                    continue;
                if (helperToken != 0 && Rostered(pair.second.state, helperToken))
                    continue;
                Apply(pair.second, helperToken == 0 ? Policy::Event::UnverifiedOrHumanParticipant
                                                    : Policy::Event::ThreatRightsOrIdentityChanged, helperToken);
                ++affected;
            }
            _counters.assistInvalidations += affected;
            return affected;
        }

        // True when the entry exists and has no current snapshot.
        bool NeedsSnapshot(CreatureKey const& key, std::uint64_t stamp)
        {
            if (Guard())
                return false;
            Entry* entry = Find(stamp, key);
            return entry && (!entry->facts.valid || entry->facts.generation != entry->state.generation);
        }

        // Record a bounded map-thread snapshot as NON-AUTHORITATIVE evidence.
        // It can only ADD uncertainty (any unverified participant, overflow,
        // unenumerable list or dead creature); it never clears uncertainty,
        // never rebuilds the roster and never sets the pure participant proof
        // (no ReplaceWithFullyVerifiedRoster / ReverifyCompleteParticipantFacts:
        // Core threat/combat/tap APIs are not a complete census of rights).
        // Never creates an entry.
        void ApplyParticipantSnapshot(CreatureKey const& key, std::uint64_t stamp, ParticipantSnapshot const& snap)
        {
            if (Guard())
                return;
            Entry* entry = Find(stamp, key);
            if (!entry)
                return;
            ++_counters.snapshotsApplied;

            bool const enumerated = snap.complete &&
                                    snap.attackerCount <= Policy::MaxTrackedAttackers &&
                                    snap.participantCount <= MaxSnapshotParticipants &&
                                    snap.minionOwnerCount <= MaxSnapshotMinionOwners;
            // No unknown (token 0) anywhere; minion owners are all nonzero.
            bool noUnknown = enumerated;
            for (std::size_t i = 0; noUnknown && i < snap.participantCount; ++i)
                if (snap.participants[i] == 0)
                    noUnknown = false;
            for (std::size_t i = 0; noUnknown && i < snap.attackerCount; ++i)
                if (snap.attackers[i] == 0)
                    noUnknown = false;
            for (std::size_t i = 0; noUnknown && i < snap.minionOwnerCount; ++i)
                if (snap.minionOwners[i] == 0)
                    noUnknown = false;
            bool const anyMinion = snap.minionVisits != 0 || snap.minionOwnerCount != 0;
            // Zero observed participants for a tracked creature is not
            // evidence of anything: treat it as unknown (fail visible).
            // First-party: ONLY verified bots, no minions at all.
            bool const onlyVerified = noUnknown && snap.participantCount > 0 && !anyMinion;
            // Weaker, separate: verified bots and/or verified-owner minions.
            bool const onlyVerifiedOrOwned = noUnknown && (snap.participantCount + snap.minionOwnerCount) > 0;

            // Unchanged conservative rule: anything but first-party verified
            // bots (minions included) adds uncertainty.
            if (!onlyVerified || !snap.alive)
                Apply(*entry, Policy::Event::UnverifiedOrHumanParticipant, 0);

            SnapshotFacts& facts = entry->facts;
            facts = {};
            facts.valid = true;
            facts.generation = entry->state.generation;
            facts.enumerated = enumerated;
            facts.plainUnscriptedNonEliteNpcScope = snap.plainUnscriptedNonEliteNpcScope;
            facts.alive = snap.alive;
            facts.inCombat = snap.inCombat;
            facts.tapHolderNoneOrVerifiedBot = snap.tapHolderNoneOrVerifiedBot;
            facts.observedOnlyVerifiedBots = onlyVerified && snap.alive;
            facts.observedOnlyVerifiedBotsOrOwnedMinions = onlyVerifiedOrOwned && snap.alive;
            if (enumerated)
                for (std::size_t i = 0; i < snap.minionOwnerCount && i < facts.minionOwners.size(); ++i)
                    facts.minionOwners[facts.minionOwnerCount++] = snap.minionOwners[i];
            if (facts.observedOnlyVerifiedBots)
                ++_counters.snapshotsOnlyVerifiedBots;
            if (facts.observedOnlyVerifiedBotsOrOwnedMinions)
                ++_counters.snapshotsOnlyVerifiedBotsOrOwnedMinions;
        }

        // Non-authoritative evidence for diagnostics/tests only.
        bool GetConservativeEvidence(std::uint64_t stamp, ConservativeEvidence& out) const
        {
            out = {};
            if (IsPoisoned() || stamp == 0)
                return false;
            auto const it = _entries.find(stamp);
            if (it == _entries.end())
                return false;
            Entry const& e = it->second;
            SnapshotFacts const& f = e.facts;
            bool const current = f.valid && f.generation != 0 && f.generation == e.state.generation;
            out.creatureToken = e.token;
            out.generation = e.state.generation;
            out.current = current;
            out.rosterUncertain = e.state.uncertain;
            out.verifiedAttackers = Policy::ActiveAttackers(e.state);
            out.enumeratedWithinBounds = current && f.enumerated;
            out.plainUnscriptedNonEliteNpcScope = current && f.plainUnscriptedNonEliteNpcScope;
            out.alive = current && f.alive && e.state.alive;
            out.inCombat = current && f.inCombat;
            out.threatCombatAndAttackersObservedOnlyVerifiedBots = current && f.observedOnlyVerifiedBots;
            out.observedOnlyVerifiedBotsOrVerifiedOwnerMinions = current && f.observedOnlyVerifiedBotsOrOwnedMinions;
            out.verifiedOwnerMinionOwners = current ? static_cast<unsigned>(f.minionOwnerCount) : 0u;
            out.tapHolderObservedNoneOrVerifiedBot = current && f.tapHolderNoneOrVerifiedBot;
            return true;
        }

        // The pure visibility model's creature half. Only identity, revision
        // and plain observations are filled; EVERY authoritative proof field
        // (complete, eligible attacker, every participant verified, no real
        // player rights, nothing pending) stays FALSE, because this stage
        // has no complete participant/rights census. It can never conceal.
        bool BuildCreatureSnapshot(std::uint64_t stamp, Visibility::CreatureSnapshot& out) const
        {
            out = {};
            ConservativeEvidence ev;
            if (!GetConservativeEvidence(stamp, ev))
                return false;
            out.creatureIdentityToken = ev.creatureToken;
            out.generation = ev.generation;
            out.current = ev.current;
            // Hostility is NOT verified by this stage: the pure model's
            // "...HostileNpc" fact stays false. Only the type-scope exclusion
            // (no elite/boss/script/NPC flag/...) is kept, as evidence.
            out.plainUnscriptedNonEliteHostileNpc = false;
            out.alive = ev.alive;
            out.inCombat = ev.inCombat;
            out.complete = false;
            out.eligibleHiddenRandomBotCurrentlyAttackingThisCreature = false;
            out.everyParticipantVerifiedTbcRaceRandomBotOrItsMinion = false;
            out.noRealPlayerCombatThreatTapOrRights = false;
            out.noPendingThreatOrCombatantChanges = false;
            return true;
        }

        // Logout / relogin / identity loss of a login epoch. Never trusts an
        // AttackStop to arrive. Returns the number of affected creatures.
        std::size_t ExpireActor(std::uint64_t actorToken,
                                std::function<void(CreatureKey const&, std::uint64_t)> onAffected = {})
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
                if (contained)
                {
                    // COPY ONLY key+incarnation before invalidating roster.
                    // Consumer is restricted to value-only queue writes under
                    // LifecycleLock; it may NOT touch any live map objects.
                    if (onAffected)
                        onAffected(pair.second.key, pair.second.token);
                    Apply(pair.second, Policy::Event::ActorIncarnationExpired, actorToken);
                    ++affected;
                    continue;
                }
                // Stage 3: minion evidence that depended on this OWNER epoch
                // becomes stale and uncertain; it can never outlive logout.
                SnapshotFacts const& f = pair.second.facts;
                bool owned = false;
                for (std::size_t i = 0; i < f.minionOwnerCount; ++i)
                    if (f.minionOwners[i] == actorToken)
                        owned = true;
                if (!owned)
                    continue;
                if (onAffected)
                    onAffected(pair.second.key, pair.second.token);
                Apply(pair.second, Policy::Event::ThreatRightsOrIdentityChanged, actorToken);
                ++_counters.minionOwnerExpiries;
                ++affected;
            }
            _counters.expiryFanout += affected;
            return affected;
        }

    private:
        static bool Rostered(Policy::State const& state, std::uint64_t actor)
        {
            if (actor == 0)
                return false;
            for (auto known : state.verifiedAttackers)
                if (known == actor)
                    return true;
            return false;
        }

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
