// ISOLATED PROTOTYPE of design C rev2 (docs/CORE_PER_CHARACTER_WRITE_ORDERING_DESIGN_2026-11-15.md):
// a per-character keyed dispatcher, layered ABOVE Core's real DatabaseWorkerPool
// (CharacterDatabase.AsyncCommitTransaction / AsyncQuery). Test-only. It is NOT a
// Core change and is not compiled into the module or the worldserver.
//
// What it proves (on a throwaway MySQL, see keyed_ordering_harness.cpp):
//   - per-key FIFO: an op for a character starts only after every earlier op for
//     that character FINISHED (committed, failed, reconciled or cancelled);
//     independent characters run in parallel across all async workers;
//   - durable transaction id: every keyed write carries a per-character sequence
//     number written as its FIRST statement (marker row, row lock held for the
//     whole transaction); a purchase also inserts its unique purchase id into a
//     journal inside the same transaction;
//   - ambiguous outcome: any "false" from Core (definite failure, connection lost
//     during COMMIT, deadlock retries exhausted) puts the key into Uncertain; the
//     key does not advance until a LOCKING read of the marker row (FOR SHARE, which
//     waits for any still-running server-side transaction) proves the outcome;
//     no automatic replay and no grant without proof;
//   - duplicate purchase: a replayed purchase id fails on the journal's primary
//     key, and reconciliation reports DuplicatePurchase (DB charged once);
//   - restart: a new process re-reads the marker (locking) before admitting
//     writes, so sequence numbers continue, and the journal answers whether a
//     purchase left Unresolved at shutdown committed;
//   - admission lease: BeginSession issues a new session epoch; submits from an
//     older epoch are refused (ops already queued by the older session still run,
//     in order, BEFORE the new session's login read);
//   - bounded queues: whole-state saves coalesce into the pending (not yet
//     dispatched) write of the same key; a per-key depth cap and a global op cap
//     refuse with Backpressure (the caller keeps memory dirty and retries; nothing
//     already admitted is ever dropped);
//   - cancellation (session gone) and an exception while building a transaction
//     both still advance the key;
//   - shutdown stops admission and drains every key; a key still Uncertain at the
//     drain deadline reports Unresolved (the next process reconciles it).
//
// Threading: every public call and every callback runs on ONE owner thread (the
// harness main thread, standing in for the world thread). Builders run at
// DISPATCH time on that thread, so a coalesced save writes the newest memory.
// In Core the same state machine would live inside DatabaseWorkerPool under its
// mutex; that move is the Core lane's (CS01) and is not attempted here.
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
#include <thread>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace KeyedProto
{
    using Clock = std::chrono::steady_clock;

    enum class Admit { Queued, Coalesced, StaleEpoch, NoSession, Backpressure, ShuttingDown, PurchaseInFlight };
    enum class Outcome { Committed, NotCommitted, DuplicatePurchase, BuildFailed, Unresolved };
    enum class KeyState { Idle, Initializing, Running, Uncertain, Reconciling };

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
        std::size_t maxPendingPerKey = 4;  // queued (not running) ops per key
        std::size_t maxTotalPending = 256; // queued ops over all keys (global cap)
        uint32_t reconcileRetryMs = 50;    // between failed reconciliation reads
        uint32_t stallMs = 2000;           // Uncertain this long -> onStalled (login refused)
    };

    struct Hooks
    {
        // Test only: report a COMMITTED write to the dispatcher as "false"
        // (models a connection lost during COMMIT after the server committed).
        std::function<bool(uint32_t guid, uint64_t seq)> loseAck;
        // Test only: runs right before each reconciliation read is queued.
        std::function<void(uint32_t guid)> beforeReconcile;
        // A key stayed Uncertain past Limits::stallMs (fail closed; still retrying).
        std::function<void(uint32_t guid)> onStalled;
    };

    struct Stats
    {
        uint64_t writesDispatched = 0, readsDispatched = 0, reconcileReads = 0, coalesced = 0,
                 backpressure = 0, staleEpoch = 0, cancelled = 0, buildFailures = 0, maxQueuedPerKey = 0;
    };

    class Dispatcher
    {
    public:
        // Tables (throwaway harness DB): marker(guid PK, seq), journal(purchase_id PK, guid, seq).
        Dispatcher(Limits limits, Hooks hooks, std::string markerTable = "harness_write_seq",
            std::string journalTable = "harness_purchase_journal")
            : _limits(limits), _hooks(std::move(hooks)), _marker(std::move(markerTable)), _journal(std::move(journalTable)) { }

        Dispatcher(Dispatcher const&) = delete;
        Dispatcher& operator=(Dispatcher const&) = delete;

        // Login admission: a new lease epoch (0 = refused: shutting down or over the global cap).
        // The first session of a key in this process queues the marker initialisation.
        uint64_t BeginSession(uint32_t guid)
        {
            if (_shuttingDown || TotalPending() >= _limits.maxTotalPending)
                return 0;
            Key& k = _keys[guid];
            k.leaseEpoch = ++_epochCounter;
            k.leaseOpen = true;
            if (!k.seqKnown && !k.initQueued)
            {
                Op init;
                init.kind = OpKind::Init;
                init.ticket = ++_ticketCounter;
                k.queue.push_back(std::move(init));
                k.initQueued = true;
                Pump(guid, k);
            }
            return k.leaseEpoch;
        }

        // Logout: the lease closes. Ops the session already queued still run in order.
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

        // A read ordered behind every earlier op of the key (the login holder).
        Admit SubmitRead(uint32_t guid, uint64_t epoch, std::string sql, ReadDone done, uint64_t* ticket = nullptr)
        {
            Admit const a = CheckAdmission(guid, epoch);
            if (a != Admit::Queued)
                return a;
            Key& k = _keys[guid];
            if (k.queue.size() >= _limits.maxPendingPerKey || TotalPending() >= _limits.maxTotalPending)
            {
                ++_stats.backpressure;
                return Admit::Backpressure;
            }
            Op op;
            op.kind = OpKind::Read;
            op.ticket = ++_ticketCounter;
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

        // Owner-thread pump: completes DB callbacks, drives Uncertain keys.
        void Process()
        {
            std::vector<std::function<void()>> deferred;
            deferred.swap(_deferred);
            for (auto& fn : deferred)
                fn();
            _txCallbacks.ProcessReadyCallbacks();
            _queryCallbacks.ProcessReadyCallbacks();
            Clock::time_point const now = Clock::now();
            for (auto& [guid, k] : _keys)
            {
                if (k.state == KeyState::Uncertain && now >= k.reconcileNotBefore)
                    StartReconcile(guid, k);
                if ((k.state == KeyState::Uncertain || k.state == KeyState::Reconciling) && !k.stallReported
                    && now - k.uncertainSince >= std::chrono::milliseconds(_limits.stallMs))
                {
                    k.stallReported = true;
                    if (_hooks.onStalled)
                        _hooks.onStalled(guid);
                }
            }
        }

        // Login fence helper: a new login for this key must be refused now.
        bool LoginBlocked(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it != _keys.end() && it->second.stallReported;
        }

        // Stop admission, then drain. Keys still Uncertain/Reconciling at the
        // deadline (and the ops queued behind them) report Unresolved; the next
        // process reconciles them from the marker and journal (restart path).
        // Returns true when everything drained.
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
                if (k.running)
                {
                    for (auto& w : k.current.waiters)
                        if (w.done)
                            w.done(Outcome::Unresolved);
                    k.current.waiters.clear();
                    k.current.readDone = nullptr;
                }
                for (Op& op : k.queue)
                    for (auto& w : op.waiters)
                        if (w.done)
                            w.done(Outcome::Unresolved);
                k.queue.clear();
                k.abandoned = true; // late DB callbacks are ignored
            }
            return false;
        }

        bool Idle() const
        {
            if (!_deferred.empty())
                return false;
            for (auto const& [guid, k] : _keys)
                if (k.running || !k.queue.empty() || k.state != KeyState::Idle)
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

        std::size_t Queued(uint32_t guid) const
        {
            auto it = _keys.find(guid);
            return it == _keys.end() ? 0 : it->second.queue.size();
        }

        Stats const& GetStats() const { return _stats; }

    private:
        enum class OpKind { Init, Write, Read };

        struct Waiter
        {
            std::string purchaseId; // empty for a save
            Done done;
        };

        struct Op
        {
            OpKind kind = OpKind::Write;
            uint64_t ticket = 0;
            Builder builder;               // newest whole-state builder (coalescing keeps the last)
            std::string purchaseId;        // at most one purchase per op
            std::vector<Waiter> waiters;
            std::string sql;               // Read
            ReadDone readDone;             // Read
            bool discard = false;          // Read cancelled while running
            uint64_t seq = 0;              // Write: assigned at dispatch
            bool dispatched = false;
        };

        struct Key
        {
            std::deque<Op> queue;
            Op current;
            bool running = false;
            KeyState state = KeyState::Idle;
            uint64_t leaseEpoch = 0;
            bool leaseOpen = false;
            bool seqKnown = false;
            bool initQueued = false;
            uint64_t lastSeq = 0;
            Clock::time_point uncertainSince{};
            Clock::time_point reconcileNotBefore{};
            bool stallReported = false;
            bool abandoned = false;
        };

        Admit CheckAdmission(uint32_t guid, uint64_t epoch)
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
            Admit const a = CheckAdmission(guid, epoch);
            if (a != Admit::Queued)
                return a;
            Key& k = _keys[guid];
            if (!purchaseId.empty() && HasPurchase(k))
                return Admit::PurchaseInFlight;
            // Coalesce into the pending tail write: it is not dispatched yet and
            // its builder is lazy, so the merged op writes the newest whole state.
            if (!k.queue.empty() && k.queue.back().kind == OpKind::Write && !k.queue.back().dispatched)
            {
                Op& tail = k.queue.back();
                tail.builder = std::move(builder);
                if (!purchaseId.empty())
                    tail.purchaseId = purchaseId;
                tail.waiters.push_back(Waiter{std::move(purchaseId), std::move(done)});
                ++_stats.coalesced;
                return Admit::Coalesced;
            }
            if (k.queue.size() >= _limits.maxPendingPerKey || TotalPending() >= _limits.maxTotalPending)
            {
                ++_stats.backpressure;
                return Admit::Backpressure;
            }
            Op op;
            op.kind = OpKind::Write;
            op.ticket = ++_ticketCounter;
            op.builder = std::move(builder);
            op.purchaseId = purchaseId;
            op.waiters.push_back(Waiter{std::move(purchaseId), std::move(done)});
            k.queue.push_back(std::move(op));
            NoteDepth(k);
            Pump(guid, k);
            return Admit::Queued;
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
            while (!k.running && k.state == KeyState::Idle && !k.queue.empty() && !k.abandoned)
            {
                k.current = std::move(k.queue.front());
                k.queue.pop_front();
                k.running = true;
                switch (k.current.kind)
                {
                    case OpKind::Init: DispatchInit(guid, k); break;
                    case OpKind::Read: DispatchRead(guid, k); break;
                    case OpKind::Write: DispatchWrite(guid, k); break;
                }
            }
        }

        // RAII-style release: every path that ends an op goes through here.
        void Finish(uint32_t guid, Key& k)
        {
            k.running = false;
            k.current = Op();
            k.state = KeyState::Idle;
            k.stallReported = false;
            Pump(guid, k);
        }

        void DispatchInit(uint32_t guid, Key& k)
        {
            k.state = KeyState::Initializing;
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            trans->Append(Acore::StringFormat("INSERT IGNORE INTO {} (guid, seq) VALUES ({}, 0)", _marker, guid));
            _txCallbacks.AddCallback(CharacterDatabase.AsyncCommitTransaction(trans)).AfterComplete(
                [this, guid](bool)
                {
                    // Whatever the insert reported, the locking read below is the truth.
                    InitRead(guid);
                });
        }

        void InitRead(uint32_t guid)
        {
            _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(Acore::StringFormat(
                "SELECT seq FROM {} WHERE guid = {} FOR SHARE", _marker, guid))).WithCallback(
                [this, guid](QueryResult r)
                {
                    Key& k = _keys[guid];
                    if (k.abandoned)
                        return;
                    if (!r)
                    {
                        // Error or missing row (indistinguishable): retry; writes stay blocked.
                        DispatchInit(guid, k);
                        return;
                    }
                    k.lastSeq = r->Fetch()[0].Get<uint64_t>();
                    k.seqKnown = true;
                    k.initQueued = false;
                    Finish(guid, k);
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
                    ReadDone done = std::move(key.current.readDone);
                    bool const discard = key.current.discard;
                    Finish(guid, key);
                    if (!discard && done)
                        done(r);
                });
        }

        void DispatchWrite(uint32_t guid, Key& k)
        {
            Op& op = k.current;
            op.dispatched = true;
            op.seq = k.lastSeq + 1;
            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            // Marker FIRST: the row lock is held for the whole transaction, so a
            // reconciliation FOR SHARE read waits for a still-running commit.
            trans->Append(Acore::StringFormat(
                "INSERT INTO {} (guid, seq) VALUES ({}, {}) ON DUPLICATE KEY UPDATE seq = VALUES(seq)",
                _marker, guid, op.seq));
            if (!op.purchaseId.empty())
                trans->Append(Acore::StringFormat("INSERT INTO {} (purchase_id, guid, seq) VALUES ('{}', {}, {})",
                    _journal, op.purchaseId, guid, op.seq));
            try
            {
                if (op.builder)
                    op.builder(trans);
            }
            catch (...)
            {
                // Nothing was sent; memory stays dirty for the next save. The key advances.
                // Callbacks are deferred to the next Process(): a build can run
                // inside a Submit call, which must never re-enter caller code.
                ++_stats.buildFailures;
                for (auto& w : op.waiters)
                    if (w.done)
                        _deferred.push_back([done = std::move(w.done)] { done(Outcome::BuildFailed); });
                Finish(guid, k);
                return;
            }
            k.state = KeyState::Running;
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
                        Resolve(guid, key, true, false);
                        return;
                    }
                    key.state = KeyState::Uncertain;
                    key.uncertainSince = Clock::now();
                    key.reconcileNotBefore = key.uncertainSince;
                    StartReconcile(guid, key);
                });
        }

        void StartReconcile(uint32_t guid, Key& k)
        {
            if (k.state != KeyState::Uncertain)
                return;
            k.state = KeyState::Reconciling;
            ++_stats.reconcileReads;
            if (_hooks.beforeReconcile)
                _hooks.beforeReconcile(guid);
            std::string const& pid = k.current.purchaseId;
            // The marker read is LOCKING (waits for any in-flight transaction on
            // the row, then reads the latest committed version). The journal is a
            // plain read: it only decides DuplicatePurchase when the marker
            // already proves THIS transaction did not commit.
            std::string const sql = pid.empty()
                ? Acore::StringFormat("SELECT seq, NULL FROM {} WHERE guid = {} FOR SHARE", _marker, guid)
                : Acore::StringFormat("SELECT w.seq, (SELECT j.seq FROM {} j WHERE j.purchase_id = '{}') "
                    "FROM {} w WHERE w.guid = {} FOR SHARE OF w", _journal, pid, _marker, guid);
            _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(sql)).WithCallback(
                [this, guid](QueryResult r)
                {
                    Key& key = _keys[guid];
                    if (key.abandoned)
                        return;
                    if (!r)
                    {
                        // Lock wait timeout / connection error: stay Uncertain, retry later.
                        key.state = KeyState::Uncertain;
                        key.reconcileNotBefore = Clock::now() + std::chrono::milliseconds(_limits.reconcileRetryMs);
                        return;
                    }
                    Field* f = r->Fetch();
                    uint64_t const dbSeq = f[0].Get<uint64_t>();
                    bool const journalHasId = !f[1].IsNull();
                    uint64_t const seq = key.current.seq;
                    bool const committed = dbSeq >= seq;
                    key.lastSeq = committed ? seq : dbSeq;
                    Resolve(guid, key, committed, !committed && journalHasId);
                });
        }

        void Resolve(uint32_t guid, Key& k, bool committed, bool duplicate)
        {
            std::vector<Waiter> waiters = std::move(k.current.waiters);
            Finish(guid, k);
            for (auto& w : waiters)
            {
                if (!w.done)
                    continue;
                if (committed)
                    w.done(Outcome::Committed);
                else if (duplicate && !w.purchaseId.empty())
                    w.done(Outcome::DuplicatePurchase);
                else
                    w.done(Outcome::NotCommitted);
            }
        }

        Limits _limits;
        Hooks _hooks;
        std::string _marker, _journal;
        std::unordered_map<uint32_t, Key> _keys;
        AsyncCallbackProcessor<TransactionCallback> _txCallbacks;
        AsyncCallbackProcessor<QueryCallback> _queryCallbacks;
        uint64_t _epochCounter = 0, _ticketCounter = 0;
        bool _shuttingDown = false;
        std::vector<std::function<void()>> _deferred;
        Stats _stats;
    };
}
