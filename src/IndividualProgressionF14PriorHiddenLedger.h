/*
 * CosmicCuddle F14: experimental per-viewer prior-F14-hide RESTORATION ledger.
 * PURE, NOT WIRED. Never proves a player, bot, rights, suppression, or client ACK.
 * A caller MUST supply positive F14-only suppression evidence; an ordinary
 * server link removal is NOT that evidence. Store values only, no Core pointers.
 * Over-capacity retains all outstanding entries and freezes future suppression;
 * it does NOT itself send restoration or protect a live client.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_PRIOR_HIDDEN_LEDGER_H
#define INDIVIDUAL_PROGRESSION_F14_PRIOR_HIDDEN_LEDGER_H

#include "IndividualProgressionF14LifecycleRegistry.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace IndividualProgressionF14PriorHiddenLedger
{
    namespace Registry = IndividualProgressionF14LifecycleRegistry;

    struct ViewerKey
    {
        std::uint64_t guidRaw = 0;
        std::uint64_t loginEpoch = 0;

        bool IsValid() const { return guidRaw != 0 && loginEpoch != 0; }
        bool operator==(ViewerKey const& other) const
        {
            return guidRaw == other.guidRaw && loginEpoch == other.loginEpoch;
        }
    };

    struct SuppressedPair
    {
        ViewerKey viewer{};
        Registry::CreatureKey creature{};
        std::uint64_t incarnation = 0;
        bool restorePending = false;
    };

    template<std::size_t Capacity = Registry::DefaultMaxCreatures>
    class Ledger
    {
        static_assert(Capacity > 0, "F14 viewer ledger capacity must be positive");

    public:
        std::size_t Size() const { return _size; }
        bool IsPoisoned() const { return _poisoned; }
        bool IsFrozen() const { return _frozen; }
        std::size_t PendingRestores() const
        {
            std::size_t count = 0;
            for (std::size_t i = 0; i < _size; ++i)
                if (_pairs[i].restorePending)
                    ++count;
            return count;
        }

        // Never allow new suppression while ANY previous hidden client link
        // requires restoration. This function grants no concealment authority.
        bool MayConsiderNewConcealment() const
        {
            return !_poisoned && !_frozen && PendingRestores() == 0 && _size < Capacity;
        }

        // Must be called ONLY after a separate, positive F14-specific
        // suppression dispatch was confirmed by an approved Core seam.
        // The boolean is an explicit caller-supplied proof, not inferred here.
        // Generic link loss/WHO/phase/cache misses MUST pass false.
        bool RecordConfirmedF14Suppression(ViewerKey viewer,
                                           Registry::CreatureKey creature,
                                           std::uint64_t incarnation,
                                           bool positivelyProvenF14Suppression)
        {
            if (!positivelyProvenF14Suppression || !viewer.IsValid() ||
                !creature.IsValid() || incarnation == 0 ||
                _poisoned || _frozen || PendingRestores() != 0)
                return false;

            for (std::size_t i = 0; i < _size; ++i)
                if (_pairs[i].viewer == viewer && _pairs[i].creature == creature)
                {
                    if (_pairs[i].incarnation == incarnation && !_pairs[i].restorePending)
                        return true; // exact duplicate of the same proven F14 suppression
                    RequestGlobalRestore(); // recycled incarnation: never overwrite old link
                    return false;
                }

            if (_size == Capacity)
            {
                // NO EVICTION. Every old hidden pair still needs restoration.
                _poisoned = true;
                RequestGlobalRestore();
                return false;
            }
            _pairs[_size++] = {viewer, creature, incarnation, false};
            return true;
        }

        // Stop, last bot logout, corpse, respawn, map unload and spawn reuse
        // must revoke ALL prior incarnations for this creature identity key.
        void RevokeCreature(Registry::CreatureKey creature)
        {
            if (!creature.IsValid())
                return;
            for (std::size_t i = 0; i < _size; ++i)
                if (_pairs[i].creature == creature)
                    _pairs[i].restorePending = true;
        }

        // Viewer logout, identity/era change: invalidate ALL login epochs
        // for the raw viewer GUID. A re-login cannot acknowledge the old one.
        void RevokeViewer(std::uint64_t viewerGuid)
        {
            if (!viewerGuid)
                return;
            for (std::size_t i = 0; i < _size; ++i)
                if (_pairs[i].viewer.guidRaw == viewerGuid)
                    _pairs[i].restorePending = true;
        }

        // Config disable, proof loss or overflow requires global restoration.
        // Unlike the logout-request queue, this preserves all old entries.
        void RequestGlobalRestore()
        {
            _frozen = true;
            for (std::size_t i = 0; i < _size; ++i)
                _pairs[i].restorePending = true;
        }

        bool NeedsRestore(ViewerKey viewer, Registry::CreatureKey creature) const
        {
            for (std::size_t i = 0; i < _size; ++i)
                if (_pairs[i].viewer == viewer && _pairs[i].creature == creature &&
                    _pairs[i].restorePending)
                    return true;
            return false;
        }

        // A server visibility pass / link transition is NOT a 3.3.5a client
        // receipt. No runtime caller may use this until independently verified
        // restoration-completion evidence and its thread are implemented.
        bool CompleteRestoreOnlyWithClientProof(ViewerKey viewer,
                                                Registry::CreatureKey creature,
                                                std::uint64_t originalIncarnation,
                                                bool separatelyVerifiedClientRestore)
        {
            if (!separatelyVerifiedClientRestore || !viewer.IsValid() ||
                !creature.IsValid() || originalIncarnation == 0)
                return false;
            for (std::size_t i = 0; i < _size; ++i)
                if (_pairs[i].viewer == viewer && _pairs[i].creature == creature &&
                    _pairs[i].incarnation == originalIncarnation &&
                    _pairs[i].restorePending)
                {
                    _pairs[i] = _pairs[_size - 1];
                    _pairs[--_size] = {};
                    return true;
                }
            return false;
        }

        // Not a feature toggle. Called only after ALL previous F14-hidden
        // client links were independently restored and the ledger is empty.
        bool ClearFrozenAfterGlobalProof(bool independentlyVerifiedAllClear)
        {
            if (!independentlyVerifiedAllClear || _size != 0)
                return false;
            _frozen = false;
            _poisoned = false;
            return true;
        }

    private:
        std::array<SuppressedPair, Capacity> _pairs{};
        std::size_t _size = 0;
        bool _poisoned = false;
        bool _frozen = false;
    };
}
#endif
