// ISOLATED PROTOTYPE (revision 2) of design C rev2
// (docs/CORE_PER_CHARACTER_WRITE_ORDERING_DESIGN_2026-11-15.md): a per-character
// keyed dispatcher layered ABOVE Core's real DatabaseWorkerPool
// (CharacterDatabase.AsyncCommitTransaction / AsyncQuery). Test-only: not compiled
// into the module or the worldserver, and not a Core change.
//
// Revision 2 answers CS01 review 6894208993:
//   H1 exact operation identity + monotonic fencing:
//     - the marker row (guid, seq, fence, owner) only changes by compare-and-set:
//       a write sets seq = n, fence = F and the harness trigger refuses anything
//       but (same fence, same owner, seq + 1); a CLAIM sets fence + 1 with a new
//       owner token and the same seq. A write of an older fence therefore fails.
//     - every keyed write inserts (guid, seq, fence, op token) into a write log;
//       a purchase also inserts (purchase_id, guid, seq, fence, op token) into
//       the journal. Committed means: the log row for (guid, n) carries THIS
//       op's fence and token. A higher marker alone proves nothing.
//     - reconciliation first CLAIMS the key (an UPDATE that waits for any
//       in-flight transaction on the marker row and then kills the old fence),
//       then reads the log/journal. After the claim no transaction of the old
//       fence can ever commit, so the read is final.
//     - a duplicate purchase id is DuplicatePurchase only when the journal row
//       belongs to the SAME character; another character's row is IdConflict.
//     - if another claimant moved the fence, the key is Fenced: writes are
//       refused until a new session re-claims it.
//   H2 callbacks first: an op's waiters are called BEFORE the key advances, so
//     a queued lazy builder sees the state the callback restored. Builder
//     failures are delivered from Process(), never inside a Submit call.
//   H3 bounded coalescing: caps are checked first (global waiter cap, per-op
//     waiter cap, per-key depth, global depth); only saves of the SAME session
//     epoch coalesce; purchases never coalesce in either direction.
//
// Threading: every public call and every callback runs on ONE owner thread (the
// harness main thread standing in for the world thread). Builders run at
// DISPATCH time on that thread. In Core the state machine would live inside
// DatabaseWorkerPool under its mutex; that move is the Core lane's (CS01).
#pragma once

#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Field.h"
#include "AsyncCallbackProcessor.h"
#include "QueryCallback.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <random>
#include <unistd.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace KeyedProto
{
    using Clock = std::chrono::steady_clock;

    enum class Admit { Queued, Coalesced, StaleEpoch, NoSession, Backpressure, ShuttingDown, PurchaseInFlight, Fenced, Frozen };
    enum class Outcome { Committed, NotCommitted, DuplicatePurchase, IdConflict, BuildFailed, Unresolved };
    enum class KeyState { Idle, Claiming, Running, Uncertain, Delivering };

    inline char const* Name(Admit a)
    {
        switch (a)
        {
            case Admit::Queued: return "Queued";
            case Admit::Coalesced: return "Coalesced";
            case Admit::StaleEpoch: return "StaleEpoch";
            case Admit::NoSession: return "NoSession";
            case Admit::Backpressure: return "Backpressure";
            case Admit::ShuttingDown: return "ShuttingDown";
            case Admit::PurchaseInFlight: return "PurchaseInFlight";
            case Admit::Fenced: return "Fenced";
            case Admit::Frozen: return "Frozen";
        }
        return "?";
    }

    inline char const* Name(Outcome o)
    {
        switch (o)
        {
            case Outcome::Committed: return "Committed";
            case Outcome::NotCommitted: return "NotCommitted";
            case Outcome::DuplicatePurchase: return "DuplicatePurchase";
            case Outcome::IdConflict: return "IdConflict";
            case Outcome::BuildFailed: return "BuildFailed";
            case Outcome::Unresolved: return "Unresolved";
        }
        return "?";
    }

    using Builder = std::function<void(CharacterDatabaseTransaction&)>;
    using Done = std::function<void(Outcome)>;
    using ReadDone = std::function<void(QueryResult)>; // only called when delivered

    struct Limits
    {
        std::size_t maxPendingPerKey = 4;   // queued (not running) ops per key
        std::size_t maxTotalPending = 256;  // queued ops over all keys
        std::size_t maxWaitersPerOp = 16;   // callbacks merged into one op
        std::size_t maxTotalWaiters = 1024; // callbacks held over all keys
        uint32_t retryMs = 50;              // between failed claim / verify steps
        uint32_t stallMs = 2000;            // claiming this long -> onStalled (login refused)
    };

    // What the dispatcher knows about an uncertain write (test hooks only).
    struct InFlight
    {
        uint32_t guid = 0;
        uint64_t seq = 0;
        uint64_t fence = 0;
        std::string op;
        std::string purchaseId;
    };

    struct Hooks
    {
        // Test only: report a COMMITTED write to the dispatcher as "false".
        std::function<bool(uint32_t guid, uint64_t seq)> loseAck;
        // Test only: runs once when reconciliation of an uncertain write starts.
        std::function<void(InFlight const&)> beforeReconcile;
        // A claim (init or reconciliation) took longer than Limits::stallMs.
        std::function<void(uint32_t guid)> onStalled;
    };

    struct Stats
    {
        uint64_t writesDispatched = 0, readsDispatched = 0, reconciliations = 0, claimAttempts = 0, coalesced = 0,
                 backpressure = 0, staleEpoch = 0, cancelled = 0, buildFailures = 0, fencedOut = 0,
                 idConflicts = 0, maxQueuedPerKey = 0, maxWaitersSeen = 0, frozen = 0, callbackExceptions = 0;
    };

    class Dispatcher
    {
    public:
        // Harness tables: marker harness_write_seq(guid PK, seq, fence, owner) with
        // the CAS trigger, harness_write_log(guid, seq PK, fence, op) and
        // harness_purchase_journal(purchase_id PK, guid, seq, fence, op).
        Dispatcher(Limits limits, Hooks hooks)
            : _limits(limits), _hooks(std::move(hooks)),
              _nonce(MakeNonce())
        {
        }

        // Process identity for op/claim tokens: wall clock + pid + 64 random bits.
        // (Prototype only: production identity needs its own review.)
        static std::string MakeNonce()
        {
            std::random_device rd;
            uint64_t const r = (uint64_t(rd()) << 32) ^ rd();
            return Acore::StringFormat("{:x}.{}.{:x}", std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(), static_cast<long>(::getpid()), r);
        }

        // Explicit authoritative reconciliation of a Frozen key (never a blind
        // unfreeze). Single-flight per key; the async proof read is bound to the
        // freeze GENERATION it started under, and is ignored if the key was
        // re-frozen, abandoned or shut down meanwhile. `done` is called exactly
        // once (contained: a throwing completion re-freezes the key).
        //  - callback/admin freeze: released on the caller's confirmation alone;
        //  - current-proof freeze (log mine, journal mismatch): released only when
        //    the write log AND journal for that op are consistent (repaired);
        //  - earlier-duplicate freeze (journal row without its own write-log
        //    proof): released only when that EARLIER journal row is proven by its
        //    own write-log row, or an audit voided it. The current, uncommitted
        //    attempt is never "proven" (nothing is fabricated for it).
        void ResolveFrozen(uint32_t guid, bool callerStateRecovered, std::function<void(bool)> done)
        {
            auto it = _keys.find(guid);
            if (it == _keys.end() || !it->second.frozen || !callerStateRecovered || it->second.resolveToken || _shuttingDown)
            {
                if (it != _keys.end())
                    Complete(it->second, done, false);
                else if (done)
                    try { done(false); } catch (...) { ++_stats.callbackExceptions; }
                return;
            }
            Key& k = it->second;
            if (k.freezeKind == FreezeKind::Callback)
            {
                Unfreeze(guid, k);
                Complete(k, done, true);
                return;
            }
            uint64_t const token = ++_resolveCounter;
            uint64_t const gen = k.freezeGen;
            FreezeKind const kind = k.freezeKind;
            FrozenOp const fo = k.frozenOp;
            k.resolveToken = token;
            k.resolveDone = std::move(done);
            std::string const sql = kind == FreezeKind::CurrentProof
                ? Acore::StringFormat("SELECT l.fence, l.op, j.guid, j.seq, j.fence, j.op FROM harness_write_log l "
                    "LEFT JOIN harness_purchase_journal j ON j.purchase_id = '{}' WHERE l.guid = {} AND l.seq = {}",
                    fo.purchaseId, guid, fo.seq)
                : Acore::StringFormat("SELECT COUNT(*), MAX(j.guid), MAX(j.seq), MAX(j.fence), MAX(j.op), MAX(l.fence), MAX(l.op) "
                    "FROM harness_purchase_journal j LEFT JOIN harness_write_log l ON l.guid = j.guid AND l.seq = j.seq "
                    "WHERE j.purchase_id = '{}'", fo.purchaseId);
            _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(sql)).WithCallback(
                [this, guid, token, gen, kind, fo](QueryResult r)
                {
                    auto kit = _keys.find(guid);
                    if (kit == _keys.end())
                        return;
                    Key& key = kit->second;
                    if (key.abandoned || key.resolveToken != token)
                        return; // settled at shutdown, or superseded: late result ignored
                    std::function<void(bool)> cb = std::move(key.resolveDone);
                    key.resolveDone = nullptr;
                    key.resolveToken = 0;
                    bool ok = false;
                    if (r && kind == FreezeKind::CurrentProof)
                    {
                        Field* f = r->Fetch();
                        ok = f[0].Get<uint64_t>() == fo.fence && f[1].Get<std::string>() == fo.op && !f[2].IsNull()
                            && f[2].Get<uint32_t>() == guid && f[3].Get<uint64_t>() == fo.seq
                            && f[4].Get<uint64_t>() == fo.fence && f[5].Get<std::string>() == fo.op;
                    }
                    else if (r && kind == FreezeKind::EarlierDuplicate)
                    {
                        Field* f = r->Fetch();
                        bool const voided = f[0].Get<uint64_t>() == 0;
                        bool const earlierProven = !voided && !f[5].IsNull() && f[1].Get<uint32_t>() == guid
                            && f[5].Get<uint64_t>() == f[3].Get<uint64_t>() && f[6].Get<std::string>() == f[4].Get<std::string>();
                        ok = voided || earlierProven;
                    }
                    // Re-check the exact freeze this resolve started for.
                    ok = ok && key.frozen && key.freezeGen == gen && key.freezeKind == kind;
                    if (ok)
                        Unfreeze(guid, key);
                    Complete(key, cb, ok);
                });
        }

        // Administrative / audit freeze (also used by tests to re-freeze mid-resolve).
        void FreezeForAudit(uint32_t guid)
        {
            auto it = _keys.find(guid);
            if (it != _keys.end())
                Freeze(it->second);
        }

        bool Frozen(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it != _keys.end() && it->second.frozen;
        }

        Dispatcher(Dispatcher const&) = delete;
        Dispatcher& operator=(Dispatcher const&) = delete;

        // Login admission: a new lease epoch (0 = refused: shutting down or over a
        // global cap). A key this process does not own (first session, or Fenced)
        // queues a claim before anything else.
        uint64_t BeginSession(uint32_t guid)
        {
            if (_shuttingDown || TotalPending() >= _limits.maxTotalPending || _totalWaiters >= _limits.maxTotalWaiters)
                return 0;
            auto existing = _keys.find(guid);
            if (existing != _keys.end() && existing->second.frozen)
                return 0; // login refused until explicit reconciliation
            Key& k = _keys[guid];
            k.leaseEpoch = ++_epochCounter;
            k.leaseOpen = true;
            if (!k.owned && !k.claimQueued)
            {
                Op claim;
                claim.kind = OpKind::Claim;
                claim.ticket = ++_ticketCounter;
                k.queue.push_back(std::move(claim));
                k.claimQueued = true;
                k.fenced = false; // the queued claim decides ownership again
                Pump(guid, k);
            }
            return k.leaseEpoch;
        }

        void EndSession(uint32_t guid, uint64_t epoch)
        {
            auto it = _keys.find(guid);
            if (it != _keys.end() && it->second.leaseEpoch == epoch)
                it->second.leaseOpen = false;
        }

        Admit SubmitSave(uint32_t guid, uint64_t epoch, Builder builder, Done done)
        {
            return SubmitWrite(guid, epoch, std::string(), std::move(builder), std::move(done));
        }

        Admit SubmitPurchase(uint32_t guid, uint64_t epoch, std::string purchaseId, Builder builder, Done done)
        {
            return SubmitWrite(guid, epoch, std::move(purchaseId), std::move(builder), std::move(done));
        }

        Admit SubmitRead(uint32_t guid, uint64_t epoch, std::string sql, ReadDone done, uint64_t* ticket = nullptr)
        {
            Admit const a = CheckAdmission(guid, epoch, false);
            if (a != Admit::Queued)
                return a;
            Key& k = _keys[guid];
            if (k.queue.size() >= _limits.maxPendingPerKey || TotalPending() >= _limits.maxTotalPending)
                return Refuse();
            Op op;
            op.kind = OpKind::Read;
            op.ticket = ++_ticketCounter;
            op.epoch = epoch;
            op.sql = std::move(sql);
            op.readDone = std::move(done);
            if (ticket)
                *ticket = op.ticket;
            k.queue.push_back(std::move(op));
            NoteDepth(k);
            Pump(guid, k);
            return Admit::Queued;
        }

        // Session gone: a queued read is removed; a running read's result is
        // discarded. Either way the key advances. Writes are never cancelled.
        bool CancelRead(uint32_t guid, uint64_t ticket)
        {
            auto it = _keys.find(guid);
            if (it == _keys.end())
                return false;
            Key& k = it->second;
            for (auto q = k.queue.begin(); q != k.queue.end(); ++q)
            {
                if (q->ticket == ticket && q->kind == OpKind::Read)
                {
                    k.queue.erase(q);
                    ++_stats.cancelled;
                    Pump(guid, k);
                    return true;
                }
            }
            if (k.running && k.current.ticket == ticket && k.current.kind == OpKind::Read)
            {
                k.current.discard = true;
                ++_stats.cancelled;
                return true;
            }
            return false;
        }

        // Owner-thread pump: completes DB callbacks, runs due retry steps,
        // delivers deferred results. Keys are visited by id, so a callback that
        // adds a key (BeginSession) cannot invalidate the walk.
        void Process()
        {
            _txCallbacks.ProcessReadyCallbacks();
            _queryCallbacks.ProcessReadyCallbacks();
            std::vector<uint32_t> ids;
            ids.reserve(_keys.size());
            for (auto const& [guid, k] : _keys)
                ids.push_back(guid);
            Clock::time_point const now = Clock::now();
            for (uint32_t guid : ids)
            {
                Key& k = _keys.at(guid);
                if (k.abandoned)
                    continue;
                if (k.deferredOutcome)
                {
                    Outcome const o = *k.deferredOutcome;
                    k.deferredOutcome.reset();
                    DeliverAndAdvance(guid, k, o, o);
                    continue;
                }
                if (k.step && now >= k.stepNotBefore)
                {
                    std::function<void()> step = std::move(k.step);
                    k.step = nullptr;
                    step();
                }
                if (k.state == KeyState::Claiming && !k.stallReported
                    && now - k.claimSince >= std::chrono::milliseconds(_limits.stallMs))
                {
                    k.stallReported = true;
                    if (_hooks.onStalled)
                        _hooks.onStalled(guid);
                }
            }
        }

        bool LoginBlocked(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it != _keys.end() && it->second.stallReported;
        }

        // Stop admission, then drain. Keys still claiming at the deadline (and
        // the ops queued behind them) report Unresolved; the next process
        // reconciles them from the marker, log and journal.
        bool Shutdown(uint32_t drainMs)
        {
            _shuttingDown = true;
            Clock::time_point const deadline = Clock::now() + std::chrono::milliseconds(drainMs);
            while (!Idle() && Clock::now() < deadline)
            {
                Process();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if (Idle())
                return true;
            for (auto& [guid, k] : _keys)
            {
                k.abandoned = true; // late DB callbacks are ignored from here on
                if (k.resolveToken)
                {
                    std::function<void(bool)> cb = std::move(k.resolveDone);
                    k.resolveDone = nullptr;
                    k.resolveToken = 0;
                    Complete(k, cb, false); // settled exactly once
                }
                if (k.running)
                    for (auto& w : TakeWaiters(k.current))
                        if (w.done)
                            Invoke(k, [&] { w.done(Outcome::Unresolved); });
                for (Op& op : k.queue)
                    for (auto& w : TakeWaiters(op))
                        if (w.done)
                            Invoke(k, [&] { w.done(Outcome::Unresolved); });
                k.queue.clear();
                k.running = false;
                k.current = Op();
            }
            return false;
        }

        bool Idle() const
        {
            for (auto const& [guid, k] : _keys)
                if (!k.abandoned && (k.running || !k.queue.empty() || k.state != KeyState::Idle || k.deferredOutcome
                    || k.resolveToken))
                    return false;
            return true;
        }

        KeyState StateOf(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it == _keys.end() ? KeyState::Idle : it->second.state;
        }

        uint64_t LastSeq(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it == _keys.end() ? 0 : it->second.lastSeq;
        }

        uint64_t Fence(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it == _keys.end() ? 0 : it->second.fence;
        }

        bool Owned(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it != _keys.end() && it->second.owned;
        }

        std::size_t Queued(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it == _keys.end() ? 0 : it->second.queue.size();
        }

        std::size_t TotalWaiters() const { return _totalWaiters; }
        Stats const& GetStats() const { return _stats; }

    private:
        enum class OpKind { Claim, Write, Read };

        struct Waiter
        {
            bool purchase = false;
            Done done;
        };

        struct Op
        {
            OpKind kind = OpKind::Write;
            uint64_t ticket = 0;
            uint64_t epoch = 0;
            Builder builder;               // newest whole-state builder (same-epoch saves only)
            std::string purchaseId;        // a purchase op never merges with anything
            std::vector<Waiter> waiters;
            std::string sql;               // Read
            ReadDone readDone;             // Read
            bool discard = false;          // Read cancelled while running
            uint64_t seq = 0;              // Write: assigned at dispatch
            uint64_t fence = 0;            // Write: fence at dispatch
            std::string opToken;           // Write: unique op identity
            bool dispatched = false;
        };

        enum class FreezeKind { None, Callback, CurrentProof, EarlierDuplicate };

        struct FrozenOp
        {
            uint64_t seq = 0;
            uint64_t fence = 0;
            std::string op;
            std::string purchaseId;
        };

        struct Key
        {
            std::deque<Op> queue;
            Op current;
            bool running = false;
            KeyState state = KeyState::Idle;
            uint64_t leaseEpoch = 0;
            bool leaseOpen = false;
            bool owned = false;            // this process holds the marker fence
            bool fenced = false;           // another claimant took the fence: refuse writes
            bool frozen = false;           // inconsistent proof / callback failure: nothing runs until ResolveFrozen
            FreezeKind freezeKind = FreezeKind::None;
            uint64_t freezeGen = 0;        // bumped by EVERY freeze (a resolve is bound to one generation)
            FrozenOp frozenOp;
            uint64_t resolveToken = 0;     // single-flight ResolveFrozen in progress
            std::function<void(bool)> resolveDone;
            bool claimQueued = false;
            uint64_t fence = 0;
            uint64_t lastSeq = 0;
            std::function<void()> step;    // a delayed retry step
            Clock::time_point stepNotBefore{};
            Clock::time_point claimSince{};
            bool stallReported = false;
            bool abandoned = false;
            std::optional<Outcome> deferredOutcome; // delivered from Process()
        };

        Admit Refuse()
        {
            ++_stats.backpressure;
            return Admit::Backpressure;
        }

        Admit CheckAdmission(uint32_t guid, uint64_t epoch, bool write)
        {
            if (_shuttingDown)
                return Admit::ShuttingDown;
            auto it = _keys.find(guid);
            if (it == _keys.end() || it->second.leaseEpoch == 0)
                return Admit::NoSession;
            if (it->second.leaseEpoch != epoch)
            {
                ++_stats.staleEpoch;
                return Admit::StaleEpoch;
            }
            if (!it->second.leaseOpen)
                return Admit::NoSession;
            if (it->second.frozen)
                return Admit::Frozen;
            if (write && it->second.fenced)
                return Admit::Fenced;
            return Admit::Queued;
        }

        bool HasPurchase(Key const& k) const
        {
            if (k.running && !k.current.purchaseId.empty())
                return true;
            for (Op const& op : k.queue)
                if (!op.purchaseId.empty())
                    return true;
            return false;
        }

        Admit SubmitWrite(uint32_t guid, uint64_t epoch, std::string purchaseId, Builder builder, Done done)
        {
            Admit const a = CheckAdmission(guid, epoch, true);
            if (a != Admit::Queued)
                return a;
            Key& k = _keys[guid];
            bool const purchase = !purchaseId.empty();
            if (purchase && HasPurchase(k))
                return Admit::PurchaseInFlight;
            // Caps first: every admitted callback is held until delivered.
            if (_totalWaiters >= _limits.maxTotalWaiters)
                return Refuse();
            // Coalesce only a save into a pending save of the SAME session epoch.
            if (!purchase && !k.queue.empty())
            {
                Op& tail = k.queue.back();
                if (tail.kind == OpKind::Write && !tail.dispatched && tail.purchaseId.empty() && tail.epoch == epoch)
                {
                    if (tail.waiters.size() >= _limits.maxWaitersPerOp)
                        return Refuse();
                    tail.builder = std::move(builder);
                    AddWaiter(tail, Waiter{false, std::move(done)});
                    ++_stats.coalesced;
                    return Admit::Coalesced;
                }
            }
            if (k.queue.size() >= _limits.maxPendingPerKey || TotalPending() >= _limits.maxTotalPending)
                return Refuse();
            Op op;
            op.kind = OpKind::Write;
            op.ticket = ++_ticketCounter;
            op.epoch = epoch;
            op.builder = std::move(builder);
            op.purchaseId = std::move(purchaseId);
            AddWaiter(op, Waiter{purchase, std::move(done)});
            k.queue.push_back(std::move(op));
            NoteDepth(k);
            Pump(guid, k);
            return Admit::Queued;
        }

        void AddWaiter(Op& op, Waiter w)
        {
            op.waiters.push_back(std::move(w));
            ++_totalWaiters;
            if (op.waiters.size() > _stats.maxWaitersSeen)
                _stats.maxWaitersSeen = op.waiters.size();
        }

        std::vector<Waiter> TakeWaiters(Op& op)
        {
            std::vector<Waiter> w = std::move(op.waiters);
            op.waiters.clear();
            _totalWaiters -= w.size();
            return w;
        }

        std::size_t TotalPending() const
        {
            std::size_t n = 0;
            for (auto const& [guid, k] : _keys)
                n += k.queue.size();
            return n;
        }

        void NoteDepth(Key const& k)
        {
            if (k.queue.size() > _stats.maxQueuedPerKey)
                _stats.maxQueuedPerKey = k.queue.size();
        }

        // Start the head op if the key is free (the per-key FIFO guarantee).
        void Pump(uint32_t guid, Key& k)
        {
            while (!k.running && k.state == KeyState::Idle && !k.queue.empty() && !k.abandoned && !k.frozen)
            {
                k.current = std::move(k.queue.front());
                k.queue.pop_front();
                k.running = true;
                switch (k.current.kind)
                {
                    case OpKind::Claim: StartClaim(guid, k, false); break;
                    case OpKind::Read: DispatchRead(guid, k); break;
                    case OpKind::Write: DispatchWrite(guid, k); break;
                }
            }
        }

        // The ONLY way an op ends: its waiters are called first (state
        // Delivering: anything they submit queues behind), then the key advances.
        void DeliverAndAdvance(uint32_t guid, Key& k, Outcome forSaves, Outcome forPurchase)
        {
            k.state = KeyState::Delivering;
            std::vector<Waiter> waiters = TakeWaiters(k.current);
            for (auto& w : waiters)
                if (w.done)
                    Invoke(k, [&] { w.done(w.purchase ? forPurchase : forSaves); });
            k.running = false;
            k.current = Op();
            k.state = KeyState::Idle;
            k.stallReported = false;
            Pump(guid, k);
        }

        // A throwing callback leaves the caller's state unknown: the key is
        // frozen (fail closed) but still leaves Delivering, and the remaining
        // waiters are still called.
        template <class Fn>
        void Invoke(Key& k, Fn&& fn)
        {
            try
            {
                fn();
            }
            catch (...)
            {
                ++_stats.callbackExceptions;
                Freeze(k);
            }
        }

        // Every freeze bumps the generation; a proof kind is never downgraded.
        void Freeze(Key& k)
        {
            if (!k.frozen)
                ++_stats.frozen;
            k.frozen = true;
            ++k.freezeGen;
            if (k.freezeKind == FreezeKind::None)
                k.freezeKind = FreezeKind::Callback;
        }

        void FreezeForProof(Key& k, Op const& op, FreezeKind kind)
        {
            Freeze(k);
            if (k.freezeKind == FreezeKind::Callback || k.freezeKind == FreezeKind::None
                || (kind == FreezeKind::EarlierDuplicate && k.freezeKind == FreezeKind::CurrentProof))
            {
                k.freezeKind = kind;
                k.frozenOp = FrozenOp{op.seq, op.fence, op.opToken, op.purchaseId};
            }
        }

        void Unfreeze(uint32_t guid, Key& k)
        {
            k.frozen = false;
            k.freezeKind = FreezeKind::None;
            k.frozenOp = FrozenOp();
            Pump(guid, k);
        }

        // A resolve completion is contained like any caller callback.
        void Complete(Key& k, std::function<void(bool)> const& done, bool ok)
        {
            if (done)
                Invoke(k, [&] { done(ok); });
        }

        void Schedule(Key& k, std::function<void()> step)
        {
            k.step = std::move(step);
            k.stepNotBefore = Clock::now() + std::chrono::milliseconds(_limits.retryMs);
        }

        // ---- claim: take the marker fence (init) or kill the old fence (reconcile)
        void StartClaim(uint32_t guid, Key& k, bool reconcile)
        {
            k.state = KeyState::Claiming;
            k.claimSince = Clock::now();
            if (reconcile)
            {
                ClaimUpdate(guid, k.current.fence, true);
                return;
            }
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            trans->Append(Acore::StringFormat(
                "INSERT IGNORE INTO harness_write_seq (guid, seq, fence, owner) VALUES ({}, 0, 0, '')", guid));
            _txCallbacks.AddCallback(CharacterDatabase.AsyncCommitTransaction(trans)).AfterComplete(
                [this, guid](bool) { ClaimObserve(guid); });
        }

        void ClaimObserve(uint32_t guid)
        {
            if (_keys[guid].abandoned)
                return;
            _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat(
                "SELECT fence FROM harness_write_seq WHERE guid = {}", guid))).WithCallback(
                [this, guid](QueryResult r)
                {
                    Key& key = _keys[guid];
                    if (key.abandoned)
                        return;
                    if (!r)
                    {
                        Schedule(key, [this, guid] { ClaimObserve(guid); });
                        return;
                    }
                    ClaimUpdate(guid, r->Fetch()[0].Get<uint64_t>(), false);
                });
        }

        // The UPDATE takes the row lock: it waits for any in-flight transaction on
        // the marker, and once it commits no transaction of `expected` can commit.
        void ClaimUpdate(uint32_t guid, uint64_t expected, bool reconcile)
        {
            ++_stats.claimAttempts;
            std::string const token = Acore::StringFormat("{}:c{}", _nonce, ++_claimCounter);
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            trans->Append(Acore::StringFormat(
                "UPDATE harness_write_seq SET fence = fence + 1, owner = '{}' WHERE guid = {} AND fence = {}",
                token, guid, expected));
            _txCallbacks.AddCallback(CharacterDatabase.AsyncCommitTransaction(trans)).AfterComplete(
                [this, guid, expected, token, reconcile](bool) { ClaimVerify(guid, expected, token, reconcile); });
        }

        // Whatever the claim reported, this read decides. The log and journal
        // columns are only used for a reconciliation (n > 0).
        void ClaimVerify(uint32_t guid, uint64_t expected, std::string token, bool reconcile)
        {
            Key& k = _keys[guid];
            if (k.abandoned)
                return;
            uint64_t const n = reconcile ? k.current.seq : 0;
            std::string const pid = reconcile ? k.current.purchaseId : std::string();
            std::string const sql = Acore::StringFormat(
                "SELECT m.seq, m.fence, m.owner, l.fence, l.op, j.guid, j.seq, j.op, j.fence, p.fence, p.op FROM harness_write_seq m "
                "LEFT JOIN harness_write_log l ON l.guid = m.guid AND l.seq = {} "
                "LEFT JOIN harness_purchase_journal j ON j.purchase_id = '{}' "
                "LEFT JOIN harness_write_log p ON p.guid = j.guid AND p.seq = j.seq WHERE m.guid = {}", n, pid, guid);
            _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(sql)).WithCallback(
                [this, guid, expected, token, reconcile](QueryResult r)
                {
                    Key& key = _keys[guid];
                    if (key.abandoned)
                        return;
                    if (!r)
                    {
                        Schedule(key, [this, guid, expected, token, reconcile] { ClaimVerify(guid, expected, token, reconcile); });
                        return;
                    }
                    Field* f = r->Fetch();
                    uint64_t const seq = f[0].Get<uint64_t>();
                    uint64_t const fence = f[1].Get<uint64_t>();
                    std::string const owner = f[2].Get<std::string>();
                    if (owner != token && fence == expected)
                    {
                        // The claim did not apply (lock wait timeout, lost, ...): retry it.
                        Schedule(key, [this, guid, expected, reconcile] { ClaimUpdate(guid, expected, reconcile); });
                        return;
                    }
                    bool const mine = owner == token;
                    if (!reconcile)
                    {
                        if (!mine)
                        {
                            // Another claimant moved the fence first: claim again from what is there now.
                            Schedule(key, [this, guid] { ClaimObserve(guid); });
                            return;
                        }
                        key.owned = true;
                        key.fenced = false;
                        key.fence = fence;
                        key.lastSeq = seq;
                        key.claimQueued = false;
                        DeliverAndAdvance(guid, key, Outcome::Committed, Outcome::Committed);
                        return;
                    }
                    // Reconciliation: the old fence is dead (we or another claimant moved
                    // it), so the write log and journal are final for this op.
                    Op const& op = key.current;
                    bool const logMine = !f[3].IsNull() && f[3].Get<uint64_t>() == op.fence
                        && f[4].Get<std::string>() == op.opToken;
                    Outcome forPurchase = Outcome::NotCommitted;
                    if (logMine)
                    {
                        bool const journalMine = op.purchaseId.empty() || (!f[5].IsNull() && f[5].Get<uint32_t>() == guid
                            && f[6].Get<uint64_t>() == op.seq && f[7].Get<std::string>() == op.opToken
                            && f[8].Get<uint64_t>() == op.fence);
                        forPurchase = journalMine ? Outcome::Committed : Outcome::Unresolved;
                        if (!journalMine)
                            FreezeForProof(key, op, FreezeKind::CurrentProof); // inconsistent proof
                    }
                    else if (!op.purchaseId.empty() && !f[5].IsNull())
                    {
                        if (f[5].Get<uint32_t>() == guid)
                        {
                            // Only an exactly proven earlier charge is a duplicate: the
                            // journal row's own write-log row (same seq, fence, op).
                            bool const earlierProven = !f[9].IsNull() && f[9].Get<uint64_t>() == f[8].Get<uint64_t>()
                                && f[10].Get<std::string>() == f[7].Get<std::string>();
                            if (earlierProven)
                                forPurchase = Outcome::DuplicatePurchase;
                            else
                            {
                                forPurchase = Outcome::Unresolved;
                                FreezeForProof(key, op, FreezeKind::EarlierDuplicate);
                            }
                        }
                        else
                        {
                            forPurchase = Outcome::IdConflict; // never attribute another character's charge
                            ++_stats.idConflicts;
                        }
                    }
                    Outcome const forSaves = logMine ? Outcome::Committed : Outcome::NotCommitted;
                    if (mine)
                    {
                        key.fence = fence;
                        key.lastSeq = seq;
                    }
                    else
                    {
                        key.owned = false;
                        key.fenced = true; // fail closed until a new session re-claims
                        ++_stats.fencedOut;
                    }
                    DeliverAndAdvance(guid, key, forSaves, forPurchase);
                });
        }

        void DispatchRead(uint32_t guid, Key& k)
        {
            k.state = KeyState::Running;
            ++_stats.readsDispatched;
            _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(k.current.sql)).WithCallback(
                [this, guid](QueryResult r)
                {
                    Key& key = _keys[guid];
                    if (key.abandoned)
                        return;
                    key.state = KeyState::Delivering;
                    if (!key.current.discard && key.current.readDone)
                        Invoke(key, [&] { key.current.readDone(r); });
                    DeliverAndAdvance(guid, key, Outcome::Committed, Outcome::Committed);
                });
        }

        void DispatchWrite(uint32_t guid, Key& k)
        {
            Op& op = k.current;
            k.state = KeyState::Running;
            if (!k.owned)
            {
                // Fenced while queued: never write without the fence.
                k.deferredOutcome = Outcome::NotCommitted;
                return;
            }
            op.dispatched = true;
            op.seq = k.lastSeq + 1;
            op.fence = k.fence;
            op.opToken = Acore::StringFormat("{}:w{}", _nonce, op.ticket);
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            // Marker FIRST (row lock for the whole transaction); the CAS trigger
            // refuses anything but (same fence, same owner, seq + 1).
            trans->Append(Acore::StringFormat("UPDATE harness_write_seq SET seq = {}, fence = {} WHERE guid = {}",
                op.seq, op.fence, guid));
            trans->Append(Acore::StringFormat(
                "INSERT INTO harness_write_log (guid, seq, fence, op) VALUES ({}, {}, {}, '{}')",
                guid, op.seq, op.fence, op.opToken));
            if (!op.purchaseId.empty())
                trans->Append(Acore::StringFormat("INSERT INTO harness_purchase_journal (purchase_id, guid, seq, fence, op) "
                    "VALUES ('{}', {}, {}, {}, '{}')", op.purchaseId, guid, op.seq, op.fence, op.opToken));
            try
            {
                if (op.builder)
                    op.builder(trans);
            }
            catch (...)
            {
                // Nothing was sent. Delivered from Process() (never inside a
                // Submit call); the key advances after delivery.
                ++_stats.buildFailures;
                k.deferredOutcome = Outcome::BuildFailed;
                return;
            }
            ++_stats.writesDispatched;
            uint64_t const seq = op.seq;
            _txCallbacks.AddCallback(CharacterDatabase.AsyncCommitTransaction(trans)).AfterComplete(
                [this, guid, seq](bool committed)
                {
                    Key& key = _keys[guid];
                    if (key.abandoned)
                        return;
                    if (committed && _hooks.loseAck && _hooks.loseAck(guid, seq))
                        committed = false;
                    if (committed)
                    {
                        key.lastSeq = seq;
                        DeliverAndAdvance(guid, key, Outcome::Committed, Outcome::Committed);
                        return;
                    }
                    key.state = KeyState::Uncertain;
                    ++_stats.reconciliations;
                    if (_hooks.beforeReconcile)
                        _hooks.beforeReconcile(InFlight{guid, key.current.seq, key.current.fence, key.current.opToken,
                            key.current.purchaseId});
                    StartClaim(guid, key, true);
                });
        }

        Limits _limits;
        Hooks _hooks;
        std::string _nonce;
        std::unordered_map<uint32_t, Key> _keys;
        AsyncCallbackProcessor<TransactionCallback> _txCallbacks;
        AsyncCallbackProcessor<QueryCallback> _queryCallbacks;
        uint64_t _epochCounter = 0, _ticketCounter = 0, _claimCounter = 0, _resolveCounter = 0;
        std::size_t _totalWaiters = 0;
        bool _shuttingDown = false;
        Stats _stats;
    };
}
