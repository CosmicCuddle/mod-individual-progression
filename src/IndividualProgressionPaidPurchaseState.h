/*
 * CosmicCuddle Individual Progression — paid expansion purchase durability
 * (pure state machine; the paid feature itself stays default OFF).
 *
 * Problem (Coordination #18/#61): Core's ordinary Player::SaveToDB queues a
 * fire-and-forget commit, so "purchased" could be shown before anything was
 * durable, and a failed commit leaves the debit and the staged receipt in
 * memory where a later ordinary save may still persist them.
 *
 * Rule implemented by IndividualProgressionExpansion.cpp:
 *  - Submit: validate first; debit and stage the canonical receipt in Player
 *    memory (one SaveToDB transaction always carries gold AND every settings
 *    source together); commit that transaction with a completion callback.
 *    The entitlement bit is NOT set yet.
 *  - Callback true for the same token: Confirmed, the bit is set (access).
 *  - Callback false: Unconfirmed. Never refund, retry, charge again or reset
 *    the receipt: a later ordinary save writes gold and receipt together, and
 *    the next login reads DB truth with the strict decoder.
 *  - While Pending/Unconfirmed: another purchase of any era is refused, and a
 *    teleport/reconnect reload must not replace the staged cache.
 *  - A callback lost with its session (reconnect) turns Pending into
 *    Unconfirmed; logout forgets the record (the next login reads the DB).
 *
 * No Player, DB, clock or Core dependency: native-testable.
 */

#ifndef INDIVIDUAL_PROGRESSION_PAID_PURCHASE_STATE_H
#define INDIVIDUAL_PROGRESSION_PAID_PURCHASE_STATE_H

#include "IndividualProgressionReceiptPolicy.h"

#include <cstdint>

namespace IndividualProgressionPaidPurchaseState
{
    enum class State : std::uint8_t
    {
        None,        // no purchase in flight for this character this session
        Pending,     // submitted; commit result not known yet
        Confirmed,   // this purchase's own transaction committed
        Unconfirmed  // commit failed or its callback was lost: DB decides at next login
    };

    // What a vendor may show for one era (GetPaidPurchaseState).
    enum class View : std::uint8_t
    {
        None,        // not owned, nothing in flight
        Pending,     // do NOT say "purchased"
        Unconfirmed, // do NOT say "purchased"; do NOT refund or retry
        Active       // entitlement bit active (durable purchase, or stored/grandfathered at load)
    };

    struct Record
    {
        State state = State::None;
        std::uint8_t era = 0;          // IndividualProgressionExpansionPolicy::Era value
        std::uint64_t token = 0;       // unique per submission
        std::uintptr_t session = 0;    // identity of the session holding the callback
    };

    constexpr bool InFlight(State s)
    {
        return s == State::Pending || s == State::Unconfirmed;
    }

    // Duplicate click / second era while anything is unresolved: refuse
    // before any validation that could debit.
    constexpr bool MayStartPurchase(Record const& r)
    {
        return !InFlight(r.state);
    }

    // A login-thread reload (teleport, reconnect) may replace the cached
    // receipt and bits only when no purchase is unresolved: the DB row may
    // not contain the staged receipt yet.
    constexpr bool ReloadMayReplaceCache(Record const& r)
    {
        return !InFlight(r.state);
    }

    constexpr Record Submit(std::uint8_t era, std::uint64_t token, std::uintptr_t session)
    {
        return Record{State::Pending, era, token, session};
    }

    enum class Completion : std::uint8_t
    {
        Ignored,      // stale token or not pending: no change
        GrantAccess,  // set the entitlement bit now
        Unconfirmed   // tell the player; never refund/retry
    };

    constexpr Completion Complete(Record& r, std::uint64_t token, bool committed)
    {
        if (r.state != State::Pending || r.token != token || token == 0)
            return Completion::Ignored;
        r.state = committed ? State::Confirmed : State::Unconfirmed;
        return committed ? Completion::GrantAccess : Completion::Unconfirmed;
    }

    // The reload hook also runs on reconnect. A Pending purchase whose
    // callback lived on another session can never be acknowledged now.
    constexpr bool CallbackLost(Record const& r, std::uintptr_t currentSession)
    {
        return r.state == State::Pending && r.session != currentSession;
    }

    // First-use commissioning (release gate, default OFF). Only a truly
    // ABSENT row (Status::Missing) becomes the canonical empty v1 receipt.
    // Malformed, wrong-length, unknown-version and inconsistent rows are
    // returned unchanged (still invalid): never commissioned or overwritten.
    inline IndividualProgressionReceiptPolicy::Decoded CommissionIfAbsent(
        IndividualProgressionReceiptPolicy::Decoded const& decoded, bool gateOn)
    {
        namespace R = IndividualProgressionReceiptPolicy;
        if (!gateOn || decoded.status != R::Status::Missing)
            return decoded;
        R::Decoded commissioned;
        commissioned.status = R::Status::Valid;
        commissioned.ledger.fields[R::Version] = R::Version1;
        return commissioned;
    }

    constexpr View ViewFor(Record const& r, std::uint8_t era, bool entitlementBit)
    {
        if (InFlight(r.state) && r.era == era)
            return r.state == State::Pending ? View::Pending : View::Unconfirmed;
        return entitlementBit ? View::Active : View::None;
    }
}

#endif
