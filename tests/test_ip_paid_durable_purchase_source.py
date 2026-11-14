"""Source contracts: paid expansion durable purchase acknowledgement (API v2),
first-use receipt commissioning gate and the per-character (no account)
purchase path. Default OFF. Source-level only: not a native/DB/game test."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def code(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return re.sub(r"//[^\n]*", "", src)


def body(src, signature):
    start = src.index(signature)
    brace = src.index("{", start)
    depth = 0
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[start:i + 1]
    raise AssertionError(signature)


EXP = code(read("src/IndividualProgressionExpansion.cpp"))
HEADER = read("src/IndividualProgression.h")
POLICY = read("src/IndividualProgressionExpansionPolicy.h")
STATE = read("src/IndividualProgressionPaidPurchaseState.h")
CONF = read("conf/individualProgression.conf.dist")
CORE = read("src/IndividualProgression.cpp")
SIG_PURCHASE = "PurchaseResult IndividualProgression::PurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"
SIG_CAN = "PurchaseResult IndividualProgression::CanPurchaseExpansion(Player* player, uint8 eraValue, uint32 costCopper)"
SIG_COMPLETE = "void IndividualProgression::CompletePaidExpansionPurchase(WorldSession* session, uint32 guid, uint64 token, bool committed)"
SIG_LOAD = "void IndividualProgression::LoadPaidExpansionEntitlement(Player* player)"


class DefaultsAndApi(unittest.TestCase):
    def test_commission_gate_default_off(self):
        self.assertEqual(re.findall(r"(?m)^IndividualProgression\.PaidExpansion\.CommissionMissingReceipt\s*=\s*(\S+)\s*$", CONF), ["0"])
        self.assertEqual(re.findall(r"(?m)^IndividualProgression\.PaidExpansion\.Enable\s*=\s*(\S+)\s*$", CONF), ["0"])
        self.assertIn("bool paidExpansionCommissionMissingReceipt = false;", HEADER)
        self.assertIn('"IndividualProgression.PaidExpansion.CommissionMissingReceipt", false)', CORE)

    def test_api_v2_and_appended_results(self):
        self.assertIn("#define COSMIC_IP_PAID_EXPANSION_API_VERSION 2\n", HEADER)
        enum = code(body(POLICY, "enum class PurchaseResult : std::uint8_t"))
        names = re.findall(r"^\s*(\w+),?\s*$", enum, flags=re.M)
        self.assertEqual(names, ["Success", "FeatureInactive", "NotApplicable", "InvalidRequest", "AlreadyOwned",
                                 "NotEligible", "BlockedByProgressionLimit", "EntryUnavailable",
                                 "InsufficientFunds", "Submitted", "PurchasePending"])
        for decl in ("IndividualProgressionPaidPurchaseState::View GetPaidPurchaseState(Player* player, IndividualProgressionExpansionPolicy::Era era);",
                     "void CompletePaidExpansionPurchase(WorldSession* session, uint32 guid, uint64 token, bool committed);",
                     "void ForgetPaidExpansionPurchase(Player* player);"):
            self.assertIn(decl, HEADER)

    def test_state_header_is_pure(self):
        includes = re.findall(r'#include [<"]([^>"]+)[>"]', STATE)
        self.assertEqual(includes, ["IndividualProgressionReceiptPolicy.h", "cstdint"])
        for token in ("Player", "WorldSession", "Database", "time("):
            self.assertNotIn(token, code(STATE))


class PurchaseSequence(unittest.TestCase):
    P = code(body(EXP, SIG_PURCHASE))

    def test_can_refuses_pending_and_missing_session_before_policy(self):
        can = code(body(EXP, SIG_CAN))
        self.assertIn("!player->GetSession()", can.split("return PurchaseResult::InvalidRequest;", 1)[0])
        pending = can.index("return PurchaseResult::PurchasePending;")
        self.assertIn("!PurchaseState::MayStartPurchase(purchase->second)", can)
        self.assertLess(pending, can.index("return CheckPurchase("))

    def test_reserve_atomically_before_debit(self):
        p = self.P
        lock = p.index("std::unique_lock<std::shared_mutex> lock(_paidExpansionLock);\n        PurchaseState::Record& record = _paidExpansionPurchases[guid];")
        may = p.index("if (!PurchaseState::MayStartPurchase(record))\n            return PurchaseResult::PurchasePending;")
        token = p.index("token = ++_paidExpansionNextToken;")
        submit = p.index("record = PurchaseState::Submit(eraValue, token, reinterpret_cast<std::uintptr_t>(session));")
        debit = p.index("player->ModifyMoney(-static_cast<int32>(costCopper));")
        self.assertTrue(p.index("ReceiptPolicy::RecordPurchase(") < lock < may < token < submit < debit)
        self.assertEqual(p.count("ModifyMoney("), 1)

    def test_one_transaction_with_commit_callback(self):
        p = self.P
        debit = p.index("player->ModifyMoney(-static_cast<int32>(costCopper));")
        slots = p.index("player->UpdatePlayerSetting(PaidExpansionSettingSource, slot, stagedLedger.fields[slot]);")
        begin = p.index("CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();")
        save = p.index("player->SaveToDB(trans, false, false);")
        commit = p.index("session->AddTransactionCallback(CharacterDatabase.AsyncCommitTransaction(trans)).AfterComplete(")
        done = p.index("sIndividualProgression->CompletePaidExpansionPurchase(session, guid, token, committed);")
        ret = p.index("return PurchaseResult::Submitted;")
        self.assertTrue(debit < slots < begin < save < commit < done < ret)
        self.assertRegex(p, r"\[session, guid, token\]\(bool committed\)")
        for forbidden in ("SaveToDB(false", "CommitTransaction(trans);", "SetPaidExpansionFlag(", "_paidExpansionFlags",
                          "return PurchaseResult::Success;", "ForceUpdateProgressionState", "UpdateProgressionState(",
                          "RewardQuest(", "CharacterDatabase.Execute(", "CharacterDatabase.Query("):
            self.assertNotIn(forbidden, p, forbidden)

    def test_access_only_from_confirmed_callback_and_never_refund_or_retry(self):
        c = code(body(EXP, SIG_COMPLETE))
        complete = c.index("completion = PurchaseState::Complete(record->second, token, committed);")
        grant = c.index("if (completion == PurchaseState::Completion::GrantAccess)\n            _paidExpansionFlags[guid] |=")
        self.assertLess(complete, grant)
        self.assertIn("if (completion == PurchaseState::Completion::Ignored)\n        return;", c)
        self.assertRegex(c, r"if \(record == _paidExpansionPurchases\.end\(\)\)\s*return;")
        for forbidden in ("ModifyMoney", "UpdatePlayerSetting", "SaveToDB", "PurchaseExpansion", "CommitTransaction",
                          "Execute(", "Query(", "_paidExpansionLedgers", "SetPaidExpansionFlag"):
            self.assertNotIn(forbidden, c, forbidden)
        # The player is checked against the GUID before any message.
        self.assertIn("player->GetGUID().GetCounter() == guid", c)
        # The only places that set a purchase bit: the confirmed callback,
        # the strict DB load and the existing SetPaidExpansionFlag (grandfather).
        setters = [m.start() for m in re.finditer(r"bits \|=|_paidExpansionFlags\[guid\] \|=", EXP)]
        self.assertEqual(len(setters), 4)  # SetPaidExpansionFlag, 2x load, callback

    def test_refresh_only_after_confirmed_grant(self):
        # CS01 6885129881: phasing/adjustments refresh after the bit is set,
        # for the same in-world character, never before the async commit.
        c = code(body(EXP, SIG_COMPLETE))
        grant_bit = c.index("_paidExpansionFlags[guid] |=")
        confirmed = c.index("if (completion == PurchaseState::Completion::GrantAccess)\n    {")
        refresh = c.index("checkIPPhasing(player, player->GetAreaId());")
        adjust = c.index("CheckAdjustments(player);")
        unconfirmed_log = c.index("could not be confirmed")
        self.assertTrue(grant_bit < confirmed < adjust < refresh < unconfirmed_log)
        guarded = c[confirmed:refresh]
        self.assertIn("if (sameCharacter)", guarded)
        self.assertIn("if (player->IsInWorld())", guarded)
        self.assertEqual(c.count("checkIPPhasing("), 1)
        self.assertEqual(c.count("CheckAdjustments("), 1)
        for sig in (SIG_PURCHASE, SIG_CAN, SIG_LOAD):
            b = code(body(EXP, sig))
            self.assertNotIn("checkIPPhasing", b, sig)
            self.assertNotIn("CheckAdjustments", b, sig)

    def test_no_refund_anywhere(self):
        self.assertNotIn("ModifyMoney(static_cast<int32>(costCopper))", EXP)
        self.assertEqual(EXP.count("ModifyMoney("), 1)


class ReloadAndCommissioning(unittest.TestCase):
    L = code(body(EXP, SIG_LOAD))

    def test_off_forgets_purchases(self):
        off = self.L.split("if (!IsPaidExpansionActive())", 1)[1].split("return;", 1)[0]
        self.assertIn("_paidExpansionPurchases.erase(guid);", off)

    def test_pending_reload_keeps_cache_and_lost_callback_is_unconfirmed(self):
        l = self.L
        lost = l.index("if (PurchaseState::CallbackLost(purchase->second, reinterpret_cast<std::uintptr_t>(player->GetSession())))")
        mark = l.index("purchase->second.state = PurchaseState::State::Unconfirmed;")
        keep = l.index("if (!PurchaseState::ReloadMayReplaceCache(purchase->second))\n                return;")
        read_db = l.index("ReadStrictPaidLedger(guid)")
        self.assertTrue(lost < mark < keep < read_db)

    def test_commission_only_absent_row_and_store_saving(self):
        l = self.L
        self.assertIn("bool const commissionGate = paidExpansionCommissionMissingReceipt &&\n        StoreWritesAllowed(true, PaidExpansionStoreSaving());", l)
        self.assertIn("ReceiptPolicy::Decoded const ledger = PurchaseState::CommissionIfAbsent(stored, commissionGate);", l)
        write = l.split("if (ledger && !stored)", 1)[1].split("LOG_INFO", 1)[0]
        self.assertIn("for (uint32 slot = 0; slot < ReceiptPolicy::Slots; ++slot)", write)
        self.assertIn("player->UpdatePlayerSetting(PaidExpansionSettingSource, slot, ledger.ledger.fields[slot]);", write)
        self.assertEqual(l.count("UpdatePlayerSetting("), 1)
        helper = code(body(STATE, "inline IndividualProgressionReceiptPolicy::Decoded CommissionIfAbsent("))
        self.assertIn("if (!gateOn || decoded.status != R::Status::Missing)\n            return decoded;", helper)
        self.assertIn("commissioned.ledger.fields[R::Version] = R::Version1;", helper)
        self.assertEqual(helper.count("commissioned.ledger.fields["), 1)

    def test_logout_forgets_only_the_purchase_record(self):
        script = body(EXP, "class IndividualProgression_PaidExpansionPlayerScript")
        self.assertIn("PLAYERHOOK_ON_LOGOUT", script)
        self.assertIn("sIndividualProgression->ForgetPaidExpansionPurchase(player);", script)


class NoAccountBypass(unittest.TestCase):
    def test_purchase_and_entitlement_paths_are_per_character(self):
        for sig in (SIG_PURCHASE, SIG_CAN, SIG_COMPLETE,
                    "CharacterFacts IndividualProgression::GetExpansionFacts(Player* player)",
                    "bool IndividualProgression::HasExpansionEntitlement(Player* player, Era era)",
                    "uint8 IndividualProgression::GetEffectiveProgression(Player* player)",
                    "bool IndividualProgression::IsAutomaticGrantAllowed(Player* player, uint8 targetStage)",
                    "uint8 IndividualProgression::GetMaxServiceGrantableStage(Player* player)",
                    "IndividualProgressionPaidPurchaseState::View IndividualProgression::GetPaidPurchaseState(Player* player, Era era)"):
            b = code(body(EXP, sig))
            for token in ("GetAccount", "accountId", "GetAccountId", "character_queststatus_rewarded"):
                self.assertNotIn(token, b, (sig, token))
        self.assertNotIn("account", code(POLICY.split("constexpr PurchaseResult CheckPurchase(", 1)[1].split("\n    }\n", 1)[0]).lower())


if __name__ == "__main__":
    unittest.main()
