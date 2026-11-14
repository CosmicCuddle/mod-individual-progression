/*
 * CosmicCuddle — F14 independent HUMAN viewer login-epoch source (default OFF).
 *
 * A stamp binds one current in-world Player object + full player ObjectGuid to
 * a new nonzero server-lifetime epoch issued during PlayerScript::OnPlayerLogin.
 * PlayerScript::OnPlayerLogout invalidates that object's stamp. No account ID,
 * credential, session address or raw Core pointer is retained in this model.
 *
 * This supplies ONLY an epoch identity fact. It does NOT establish the Core
 * callback's owning map thread, map/instance, a creature's incarnation, any
 * group/quest/loot rights, a previous F14 hide, client receipt or permission
 * to conceal a creature. No positive F14 policy/ledger writes live here.
 */
#ifndef INDIVIDUAL_PROGRESSION_F14_HUMAN_VIEWER_EPOCH_H
#define INDIVIDUAL_PROGRESSION_F14_HUMAN_VIEWER_EPOCH_H

#include <atomic>
#include <cstdint>
#include <limits>

namespace IndividualProgressionF14HumanViewerEpoch
{
    class Issuer final
    {
    public:
        explicit Issuer(std::uint64_t initial = 0) : _last(initial) { }

        // No wrap, no reuse: after UINT64_MAX the issuer fails closed.
        std::uint64_t Issue()
        {
            std::uint64_t current = _last.load(std::memory_order_relaxed);
            for (;;)
            {
                if (current == std::numeric_limits<std::uint64_t>::max())
                    return 0;
                if (_last.compare_exchange_weak(current, current + 1,
                        std::memory_order_relaxed, std::memory_order_relaxed))
                    return current + 1;
            }
        }

        std::uint64_t LastIssued() const
        {
            return _last.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<std::uint64_t> _last;
    };

    struct Stamp
    {
        std::uint64_t playerGuidRaw = 0;
        std::uint64_t loginEpoch = 0;

        void Clear()
        {
            playerGuidRaw = 0;
            loginEpoch = 0;
        }

        // Issued only from a real PlayerScript current human login callback.
        // All missing/stale/zero facts fail; no fallback to a cached GUID.
        void Bind(std::uint64_t fullPlayerGuidRaw, std::uint64_t issuedEpoch)
        {
            Clear();
            if (fullPlayerGuidRaw && issuedEpoch)
            {
                playerGuidRaw = fullPlayerGuidRaw;
                loginEpoch = issuedEpoch;
            }
        }

        bool Matches(std::uint64_t currentFullPlayerGuidRaw,
                     std::uint64_t currentObservedLoginEpoch) const
        {
            return currentFullPlayerGuidRaw && currentObservedLoginEpoch &&
                   playerGuidRaw == currentFullPlayerGuidRaw &&
                   loginEpoch == currentObservedLoginEpoch;
        }
    };

    constexpr bool AuthorizesCreatureConcealment() { return false; }
    constexpr bool HasProvenClientRestoration() { return false; }
    constexpr bool HasProvenOwningMapThread() { return false; }
}
#endif
