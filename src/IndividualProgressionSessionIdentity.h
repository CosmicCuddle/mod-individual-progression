/*
 * CosmicCuddle Individual Progression — session identity for paid purchase
 * commit callbacks (CS01 6903447094).
 *
 * A paid purchase's commit callback lives on the WorldSession that submitted
 * it; on reconnect that session is destroyed and its pending callbacks are
 * silently dropped (Core ~WorldSession). IP detects the loss by comparing the
 * submitting session's identity with the current one.
 *
 *  - Paired Core with `WorldSession::GetPB59SessionIncarnation()` (a
 *    process-local, never-recycled counter; 0 = exhausted): the EXACT
 *    incarnation is used. Zero is rejected (no purchase can start).
 *  - Any other Core (standalone IP build): falls back to the session
 *    address. Fail-closed, but a new session allocated at the freed address
 *    (ABA) would look identical: the purchase then stays Pending (no grant,
 *    no refund, no second charge) until logout forgets it. Documented
 *    tradeoff; IP alone has no hook that distinguishes worldport from
 *    reconnect.
 *
 * The member is detected by expression SFINAE, so this compiles against
 * every Core (the identity header and the accessor landed in different Core
 * commits, so a header check would not be sound).
 */

#ifndef INDIVIDUAL_PROGRESSION_SESSION_IDENTITY_H
#define INDIVIDUAL_PROGRESSION_SESSION_IDENTITY_H

#include "IndividualProgressionPaidPurchaseState.h"

#include <cstdint>
#include <type_traits>
#include <utility>

namespace IndividualProgressionSessionIdentity
{
    namespace PurchaseState = IndividualProgressionPaidPurchaseState;

    template <class S, class = void>
    struct HasIncarnation : std::false_type { };

    template <class S>
    struct HasIncarnation<S, std::void_t<decltype(static_cast<std::uint64_t>(std::declval<S const&>().GetPB59SessionIncarnation()))>>
        : std::true_type { };

    template <class S>
    PurchaseState::SessionKey KeyFor(S const* session)
    {
        if (!session)
            return {};
        if constexpr (HasIncarnation<S>::value)
            return {PurchaseState::SessionKind::Incarnation, static_cast<std::uint64_t>(session->GetPB59SessionIncarnation())};
        else
            return {PurchaseState::SessionKind::Address, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(session))};
    }
}

#endif
