// Native model of the paid expansion purchase durability rule (API v2).
// Uses the REAL pure headers (expansion policy, strict receipt policy,
// purchase state machine) and models Player memory plus the character DB:
// Core's SaveToDB snapshots money AND every settings source into ONE
// transaction, which then commits or fails as a whole.
// No AzerothCore, DB or server: this is a model, not a native/game test.
#include "IndividualProgressionExpansionPolicy.h"
#include "IndividualProgressionPaidPurchaseState.h"
#include "IndividualProgressionSessionIdentity.h"
#include "IndividualProgressionReceiptPolicy.h"

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace E = IndividualProgressionExpansionPolicy;
namespace R = IndividualProgressionReceiptPolicy;
namespace S = IndividualProgressionPaidPurchaseState;
namespace SI = IndividualProgressionSessionIdentity;

namespace
{
    constexpr S::SessionKey Key(std::uint64_t v) { return S::SessionKey{S::SessionKind::Incarnation, v}; }
    unsigned g_pass = 0;
    unsigned g_fail = 0;
#define CHECK(expr)                                                         \
    do                                                                      \
    {                                                                       \
        if (expr)                                                           \
            ++g_pass;                                                       \
        else                                                                \
        {                                                                   \
            ++g_fail;                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);     \
        }                                                                   \
    } while (0)

    constexpr std::uint8_t TBC = static_cast<std::uint8_t>(E::Era::Tbc);
    constexpr std::uint8_t WRATH = static_cast<std::uint8_t>(E::Era::Wrath);
    constexpr std::uint32_t Epoch = 1'800'000'000u;

    std::string EmptyV1()
    {
        R::Ledger l;
        l.fields[R::Version] = R::Version1;
        return R::Serialize(l);
    }

    struct Db
    {
        std::int64_t gold = 0;
        std::optional<std::string> row; // nullopt = no character_settings row
        std::uint8_t rawStage = 0;
    };

    struct Txn
    {
        std::int64_t gold;
        std::optional<std::string> row;
    };

    // One online character: Player memory + IP caches + the DB.
    struct Character
    {
        bool gated = true;           // normal human account
        bool commissionGate = false; // CommissionMissingReceipt
        Db db;
        // Player memory
        std::int64_t gold = 0;
        std::optional<std::string> settings; // in-memory ip_paid_expansion source
        std::uint8_t rawStage = 0;
        // IP caches
        std::optional<R::Ledger> cachedLedger;
        bool tbcBit = false;
        bool wrathBit = false;
        S::Record record;
        S::SessionKey session = Key(1);
        std::uint64_t nextToken = 0;
        std::vector<std::pair<std::uint64_t, Txn>> inflight; // purchase txns with callbacks
        std::int64_t debits = 0;

        E::CharacterFacts Facts() const
        {
            E::CharacterFacts f;
            f.featureActive = true;
            f.gated = gated;
            f.rawStage = rawStage;
            f.tbcPaid = tbcBit;
            f.wrathPaid = wrathBit;
            return f;
        }

        // OnPlayerSendInitialPacketsBeforeAddToMap at login: everything from the DB.
        void Login(std::uint64_t newSession)
        {
            session = Key(newSession);
            gold = db.gold;
            settings = db.row;
            rawStage = db.rawStage;
            record = S::Record{};          // logout forgot it
            inflight.clear();              // callbacks died with the old session
            Reload();
        }

        // The same hook on far teleport / reconnect (no logout in between).
        void Reload()
        {
            if (S::CallbackLost(record, session))
                record.state = S::State::Unconfirmed;
            if (!S::ReloadMayReplaceCache(record))
                return;
            R::Decoded const stored = db.row ? R::DecodeRaw(*db.row, true) : R::DecodeRaw({}, false);
            R::Decoded const ledger = S::CommissionIfAbsent(stored, commissionGate);
            if (ledger && !stored)
                settings = R::Serialize(ledger.ledger);
            tbcBit = wrathBit = false;
            if (ledger)
            {
                cachedLedger = ledger.ledger;
                if (rawStage >= E::VanillaFinalStage)
                {
                    tbcBit = ledger.ledger.fields[R::TbcFlag] != R::FlagNone;
                    wrathBit = ledger.ledger.fields[R::WrathFlag] != R::FlagNone;
                }
            }
            else
                cachedLedger.reset();
        }

        Txn Snapshot() const { return Txn{gold, settings}; }

        void Apply(Txn const& t)
        {
            db.gold = t.gold;
            if (t.row)
                db.row = t.row;
        }

        E::PurchaseResult Can(std::uint8_t era, std::uint32_t cost) const
        {
            if (!cachedLedger)
                return E::PurchaseResult::EntryUnavailable;
            if (!S::MayStartPurchase(record))
                return E::PurchaseResult::PurchasePending;
            return E::CheckPurchase(Facts(), era, gold >= cost, true);
        }

        // Mirrors IndividualProgression::PurchaseExpansion step by step.
        E::PurchaseResult Purchase(std::uint8_t era, std::uint32_t cost)
        {
            E::PurchaseResult const check = gated ? Can(era, cost) : E::CheckPurchase(Facts(), era, gold >= cost, true);
            if (check != E::PurchaseResult::Success)
                return check;
            R::Ledger staged = *cachedLedger;
            if (!R::RecordPurchase(staged, era == TBC ? R::Era::Tbc : R::Era::Wrath, cost, Epoch))
                return E::PurchaseResult::EntryUnavailable;
            if (!S::MayStartPurchase(record))
                return E::PurchaseResult::PurchasePending;
            std::uint64_t const token = ++nextToken;
            record = S::Submit(era, token, session);
            gold -= cost;
            ++debits;
            settings = R::Serialize(staged);
            cachedLedger = staged;
            inflight.emplace_back(token, Snapshot());
            return E::PurchaseResult::Submitted;
        }

        // The commit result of in-flight purchase txn i arrives.
        S::Completion Callback(std::size_t i, bool committed)
        {
            auto const [token, txn] = inflight.at(i);
            if (committed)
                Apply(txn);
            S::Completion const c = S::Complete(record, token, committed);
            if (c == S::Completion::GrantAccess)
                (record.era == TBC ? tbcBit : wrathBit) = true;
            return c;
        }

        void Autosave(bool committed)
        {
            if (committed)
                Apply(Snapshot());
        }

        S::View View(std::uint8_t era) const
        {
            return S::ViewFor(record, era, era == TBC ? tbcBit : wrathBit);
        }

        bool DbPaid(std::uint8_t era) const
        {
            if (!db.row)
                return false;
            R::Decoded const d = R::DecodeRaw(*db.row, true);
            return d && R::HasPaidReceipt(d.ledger, era == TBC ? R::Era::Tbc : R::Era::Wrath);
        }
    };

    Character Human(std::uint8_t raw, std::int64_t gold, std::optional<std::string> row = EmptyV1())
    {
        Character c;
        c.db.gold = gold;
        c.db.row = row;
        c.db.rawStage = raw;
        c.Login(1);
        return c;
    }

    void TestTbcHappyPath()
    {
        Character c = Human(7, 1000);
        CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::Submitted);
        CHECK(c.gold == 600 && c.debits == 1);              // exact debit, once
        CHECK(!E::HasTbc(c.Facts()));                         // no access before commit
        CHECK(!E::AutomaticGrantAllowed(c.Facts(), E::TbcEntryStage)); // 10259 cannot grant 8 yet
        CHECK(c.View(TBC) == S::View::Pending);
        CHECK(c.db.gold == 1000 && !c.DbPaid(TBC));           // nothing durable yet
        CHECK(c.Callback(0, true) == S::Completion::GrantAccess);
        CHECK(c.View(TBC) == S::View::Active);
        CHECK(E::HasTbc(c.Facts()) && c.rawStage == 7);       // purchase never writes stage 8
        CHECK(E::AutomaticGrantAllowed(c.Facts(), E::TbcEntryStage)); // 10259 hand-in may now grant 8
        CHECK(c.db.gold == 600 && c.DbPaid(TBC));
        CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::AlreadyOwned);
        CHECK(c.gold == 600 && c.debits == 1);
        // relog: DB truth, still owned
        c.Login(2);
        CHECK(c.tbcBit && c.Can(TBC, 400) == E::PurchaseResult::AlreadyOwned);
    }

    void TestWrathPath()
    {
        R::Ledger l;
        l.fields[R::Version] = R::Version1;
        R::RecordPurchase(l, R::Era::Tbc, 400, Epoch);
        Character c = Human(13, 2000, R::Serialize(l));
        CHECK(E::EffectiveStage(c.Facts()) == E::UnpaidWrathAccessCap);
        CHECK(c.Purchase(WRATH, 900) == E::PurchaseResult::Submitted);
        CHECK(E::EffectiveStage(c.Facts()) == E::UnpaidWrathAccessCap); // pending: still 12
        CHECK(c.Callback(0, true) == S::Completion::GrantAccess);
        CHECK(E::EffectiveStage(c.Facts()) == E::WrathAccessStage);
        CHECK(c.rawStage == 13);                               // never grants 14
        CHECK(c.db.gold == 1100 && c.DbPaid(WRATH) && c.DbPaid(TBC));
        // Wrath from Vanilla / unpaid TBC is never sellable.
        Character v = Human(7, 5000);
        CHECK(v.Purchase(WRATH, 900) == E::PurchaseResult::NotEligible);
        CHECK(v.debits == 0);
    }

    void TestInsufficientFundsAndIneligible()
    {
        Character c = Human(7, 399);
        CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::InsufficientFunds);
        CHECK(c.gold == 399 && c.debits == 0 && c.record.state == S::State::None);
        Character d = Human(6, 99999);
        CHECK(d.Purchase(TBC, 400) == E::PurchaseResult::NotEligible);
        CHECK(d.debits == 0);
    }

    void TestDuplicateClicks()
    {
        Character c = Human(7, 1000);
        CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::Submitted);
        for (int i = 0; i < 5; ++i)
            CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::PurchasePending);
        CHECK(c.Purchase(WRATH, 400) == E::PurchaseResult::PurchasePending);
        CHECK(c.gold == 600 && c.debits == 1 && c.inflight.size() == 1);
        c.Callback(0, true);
        CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::AlreadyOwned);
        CHECK(c.debits == 1);
    }

    void TestFirstSaveFailsLaterSaveSucceeds()
    {
        Character c = Human(7, 1000);
        CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::Submitted);
        CHECK(c.Callback(0, false) == S::Completion::Unconfirmed);
        CHECK(c.View(TBC) == S::View::Unconfirmed);
        CHECK(c.gold == 600);                          // never refunded
        CHECK(!c.tbcBit);                              // no access on failure
        CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::PurchasePending); // never charged again
        CHECK(c.debits == 1);
        c.Reload();                                    // teleport: cache kept
        CHECK(c.cachedLedger && c.cachedLedger->fields[R::TbcFlag] == R::FlagPaid);
        c.Autosave(true);                              // a later ordinary save commits
        CHECK(c.db.gold == 600 && c.DbPaid(TBC));      // gold and receipt TOGETHER
        c.Login(2);                                    // next login: DB truth
        CHECK(c.tbcBit && c.View(TBC) == S::View::Active && c.debits == 1);
    }

    void TestBothSavesFail()
    {
        Character c = Human(7, 1000);
        c.Purchase(TBC, 400);
        c.Callback(0, false);
        c.Autosave(false);
        c.Autosave(false);          // logout save fails too / crash
        CHECK(c.db.gold == 1000 && !c.DbPaid(TBC)); // nothing persisted, consistently
        c.Login(2);
        CHECK(c.gold == 1000 && !c.tbcBit);
        CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::Submitted); // may buy again (never charged in DB)
        c.Callback(0, true);
        CHECK(c.db.gold == 600 && c.DbPaid(TBC));    // charged exactly once in the DB
    }

    // Exhaustive: purchase txn + up to 3 later saves, every commit outcome
    // and every callback position. Persisted gold and receipt never diverge.
    void TestInvariantExhaustive()
    {
        for (unsigned mask = 0; mask < 16; ++mask)
            for (unsigned callbackAt = 0; callbackAt <= 3; ++callbackAt)
            {
                Character c = Human(7, 1000);
                CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::Submitted);
                for (unsigned step = 0; step <= 3; ++step)
                {
                    if (step == callbackAt)
                        c.Callback(0, mask & 1u);
                    if (step > 0)
                        c.Autosave(mask & (1u << step));
                    bool const paid = c.DbPaid(TBC);
                    CHECK(paid == (c.db.gold == 600));
                    CHECK(c.db.gold == 600 || c.db.gold == 1000);
                    CHECK(c.Purchase(TBC, 400) != E::PurchaseResult::Submitted);
                    CHECK(c.debits == 1);
                    // Access only after this purchase's own confirmed commit.
                    CHECK(c.tbcBit == (step >= callbackAt && (mask & 1u)));
                }
                c.Login(9);
                CHECK(c.tbcBit == c.DbPaid(TBC));
                CHECK(c.gold == c.db.gold);
            }
    }

    void TestRelogTeleportReconnect()
    {
        // Logout while pending: the record is forgotten; a late callback is ignored.
        {
            Character c = Human(7, 1000);
            c.Purchase(TBC, 400);
            Txn const t = c.inflight[0].second;
            c.Autosave(true);        // logout save commits gold+receipt
            c.Login(2);
            CHECK(c.tbcBit && c.db.gold == 600);
            S::Record r = c.record;
            CHECK(S::Complete(r, 1, true) == S::Completion::Ignored);
            (void)t;
        }
        // Teleport while pending: cache NOT replaced by the stale DB row.
        {
            Character c = Human(7, 1000);
            c.Purchase(TBC, 400);
            c.Reload();
            CHECK(c.cachedLedger->fields[R::TbcFlag] == R::FlagPaid);
            CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::PurchasePending);
            CHECK(c.Callback(0, true) == S::Completion::GrantAccess);
            CHECK(c.tbcBit);
        }
        // Reconnect (new session, same in-world player): callback lost.
        {
            Character c = Human(7, 1000);
            c.Purchase(TBC, 400);
            c.session = Key(77);
            c.Reload();
            CHECK(c.record.state == S::State::Unconfirmed);
            CHECK(c.View(TBC) == S::View::Unconfirmed && !c.tbcBit);
            CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::PurchasePending);
        }
        // Stale/duplicate tokens never change state.
        {
            S::Record r = S::Submit(TBC, 5, Key(1));
            CHECK(S::Complete(r, 4, true) == S::Completion::Ignored);
            CHECK(S::Complete(r, 0, true) == S::Completion::Ignored);
            CHECK(S::Complete(r, 5, true) == S::Completion::GrantAccess);
            CHECK(S::Complete(r, 5, false) == S::Completion::Ignored);
            CHECK(r.state == S::State::Confirmed);
            S::Record none;
            CHECK(S::Complete(none, 0, true) == S::Completion::Ignored);
        }
    }

    void TestReceiptRows()
    {
        // Missing row, gate OFF: cannot buy, nothing written.
        {
            Character c = Human(7, 1000, std::nullopt);
            CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::EntryUnavailable);
            CHECK(!c.settings && c.debits == 0);
        }
        // Missing row, gate ON: commissioned empty v1, then a normal purchase.
        {
            Character c;
            c.commissionGate = true;
            c.db = Db{1000, std::nullopt, 7};
            c.Login(1);
            CHECK(c.settings && *c.settings == EmptyV1());
            CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::Submitted);
            c.Callback(0, true);
            CHECK(c.DbPaid(TBC) && c.db.gold == 600);
        }
        // Malformed / legacy / inconsistent rows: never commissioned, never overwritten.
        std::vector<std::string> const bad = {
            "",                    // empty data
            "1 0 0 ",              // legacy 3-slot
            "1 0 0",               // legacy, no trailing space
            "0 0 0 1 0 0 0 0",     // non-canonical (no trailing space)
            "0 0 0 2 0 0 0 0 ",    // unknown version
            "1 0 0 1 0 0 0 0 ",    // paid without cost/time: inconsistent
            "0 0 0 1 0 0 0 0 0 ",  // nine slots
            "0 0 0 01 0 0 0 0 ",   // leading zero
            "x 0 0 1 0 0 0 0 ",    // junk
            "3 0 0 1 0 0 0 0 ",    // unknown flag
        };
        for (std::string const& row : bad)
            for (int gate = 0; gate < 2; ++gate)
            {
                Character c;
                c.commissionGate = gate;
                c.db = Db{1000, row, 7};
                c.Login(1);
                CHECK(!c.cachedLedger);
                CHECK(c.settings && *c.settings == row);       // untouched in memory
                CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::EntryUnavailable);
                CHECK(c.Purchase(WRATH, 400) == E::PurchaseResult::EntryUnavailable);
                CHECK(c.debits == 0 && c.gold == 1000);
                c.Autosave(true);
                CHECK(c.db.row && *c.db.row == row);           // never overwritten
                CHECK(!c.tbcBit && !c.wrathBit);               // unknown is not paid ...
                CHECK(c.Can(TBC, 400) != E::PurchaseResult::Success); // ... and not free
            }
        // Pure commissioning helper: only Missing is commissioned.
        for (R::Status st : {R::Status::Malformed, R::Status::WrongSlotCount, R::Status::UnknownVersion,
                             R::Status::Inconsistent})
        {
            R::Decoded d;
            d.status = st;
            CHECK(S::CommissionIfAbsent(d, true).status == st);
        }
        R::Decoded missing;
        CHECK(!S::CommissionIfAbsent(missing, false));
        R::Decoded const commissioned = S::CommissionIfAbsent(missing, true);
        CHECK(commissioned && R::Serialize(commissioned.ledger) == EmptyV1());
        CHECK(R::DecodeRaw(R::Serialize(commissioned.ledger), true));  // canonical
    }

    void TestNegativesBotsAltbotsExcluded()
    {
        // bot / Altbot / excluded / GM-excluded accounts: gated = false.
        for (std::uint8_t raw : {0, 7, 8, 13, 14})
            for (std::uint8_t era : {TBC, WRATH})
            {
                Character c = Human(raw, 99999);
                c.gated = false;
                CHECK(c.Purchase(era, 400) == E::PurchaseResult::NotApplicable);
                CHECK(c.debits == 0 && c.gold == 99999);
                CHECK(E::HasTbc(c.Facts()) && E::HasWrath(c.Facts()));   // established rules unchanged
                CHECK(E::EffectiveStage(c.Facts()) == raw);
            }
        // Humans are gated; unpaid Vanilla human cannot use TBC.
        Character h = Human(7, 1000);
        CHECK(!E::HasTbc(h.Facts()));
    }

    void TestNoAccountBypass()
    {
        // Facts are per character; another character at stage 13/14 on the
        // same account changes nothing for this one (no account input exists).
        for (std::uint8_t raw = 0; raw < 7; ++raw)
        {
            Character c = Human(raw, 99999);
            CHECK(c.Purchase(TBC, 400) == E::PurchaseResult::NotEligible);
            CHECK(c.Purchase(WRATH, 400) == E::PurchaseResult::NotEligible);
            CHECK(c.debits == 0);
        }
        Character tbcUnfinished = Human(12, 99999);
        tbcUnfinished.tbcBit = true;
        CHECK(tbcUnfinished.Purchase(WRATH, 400) == E::PurchaseResult::NotEligible);
    }

    void TestViews()
    {
        S::Record none;
        CHECK(S::ViewFor(none, TBC, false) == S::View::None);
        CHECK(S::ViewFor(none, TBC, true) == S::View::Active);
        S::Record p = S::Submit(TBC, 1, Key(1));
        CHECK(S::ViewFor(p, TBC, false) == S::View::Pending);
        CHECK(S::ViewFor(p, WRATH, false) == S::View::None);
        CHECK(S::ViewFor(p, WRATH, true) == S::View::Active);
        p.state = S::State::Unconfirmed;
        CHECK(S::ViewFor(p, TBC, false) == S::View::Unconfirmed);
        p.state = S::State::Confirmed;
        CHECK(S::ViewFor(p, TBC, true) == S::View::Active);
        CHECK(S::MayStartPurchase(p) && S::ReloadMayReplaceCache(p));
        CHECK(!S::CallbackLost(p, Key(2)));
        S::Record q = S::Submit(WRATH, 2, Key(3));
        CHECK(S::CallbackLost(q, Key(4)) && !S::CallbackLost(q, Key(3)));
        CHECK(static_cast<int>(E::PurchaseResult::InsufficientFunds) == 8);   // v1 numbering kept
        CHECK(static_cast<int>(E::PurchaseResult::Submitted) == 9);
        CHECK(static_cast<int>(E::PurchaseResult::PurchasePending) == 10);
    }
}

    // CS01 6903447094: exact session identity for the commit callback.
    struct CoreSession { std::uint64_t inc; std::uint64_t GetPB59SessionIncarnation() const noexcept { return inc; } };
    struct PlainSession { int unused = 0; };

    void TestSessionIdentity()
    {
        // Zero / None identity is invalid; a purchase may not start with it.
        CHECK(!S::Valid(S::SessionKey{}) && !S::Valid(Key(0)) && S::Valid(Key(1)));
        CHECK(!S::Valid(S::SessionKey{S::SessionKind::Address, 0}));
        // Kinds never match each other, and invalid never matches anything.
        CHECK(!S::SameSession(Key(5), S::SessionKey{S::SessionKind::Address, 5}));
        CHECK(!S::SameSession(Key(0), Key(0)) && S::SameSession(Key(7), Key(7)));
        // Paired Core: same ADDRESS, replacement incarnation -> the callback is lost.
        CoreSession a{41};
        S::SessionKey const ka = SI::KeyFor(&a);
        CHECK(ka.kind == S::SessionKind::Incarnation && ka.value == 41);
        S::Record r = S::Submit(TBC, 9, ka);
        a.inc = 42;                      // the freed object's slot reused by a NEW session
        CHECK(S::CallbackLost(r, SI::KeyFor(&a)));
        // ... while a worldport keeps the same incarnation -> not lost.
        a.inc = 41;
        CHECK(!S::CallbackLost(r, SI::KeyFor(&a)));
        // Exhausted incarnation (0) is invalid: no purchase can be submitted.
        CoreSession z{0};
        CHECK(!S::Valid(SI::KeyFor(&z)));
        // A stale callback after the loss never grants.
        r.state = S::State::Unconfirmed;
        CHECK(S::Complete(r, 9, true) == S::Completion::Ignored);
        // Standalone Core (no accessor): address fallback; the ABA case is NOT
        // distinguishable (documented tradeoff: fail-closed Pending until logout).
        PlainSession p;
        S::SessionKey const kp = SI::KeyFor(&p);
        CHECK(kp.kind == S::SessionKind::Address && kp.value != 0);
        S::Record rp = S::Submit(TBC, 10, kp);
        CHECK(!S::CallbackLost(rp, SI::KeyFor(&p)));
        PlainSession other;
        CHECK(S::CallbackLost(rp, SI::KeyFor(&other)));
        // Mixed kinds (e.g. a record from before an upgrade) count as lost.
        CHECK(S::CallbackLost(r.state == S::State::Pending ? r : S::Submit(TBC, 11, kp), ka));
        // No session at all.
        CHECK(!S::Valid(SI::KeyFor(static_cast<CoreSession const*>(nullptr))));
        static_assert(SI::HasIncarnation<CoreSession>::value && !SI::HasIncarnation<PlainSession>::value);
    }

int main()
{
    TestTbcHappyPath();
    TestWrathPath();
    TestInsufficientFundsAndIneligible();
    TestDuplicateClicks();
    TestFirstSaveFailsLaterSaveSucceeds();
    TestBothSavesFail();
    TestInvariantExhaustive();
    TestRelogTeleportReconnect();
    TestReceiptRows();
    TestNegativesBotsAltbotsExcluded();
    TestNoAccountBypass();
    TestViews();
    TestSessionIdentity();
    std::printf("PAID PURCHASE DURABILITY MODEL %u/%u passed\n", g_pass, g_pass + g_fail);
    return g_fail == 0 ? 0 : 1;
}
