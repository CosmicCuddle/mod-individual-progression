/*
 * Disposable C++17/20 test of the strict RAW purchase receipt ledger.
 * NO Worldserver, account, real money or database is accessed.
 */
#include "../src/IndividualProgressionReceiptPolicy.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace IndividualProgressionReceiptPolicy;

int main()
{
    unsigned checks = 0;
    // Never use assert(): NDEBUG would silently skip every check expression,
    // falsely printing PASS while making no policy calls at all.
    auto check = [&](bool value)
    {
        if (!value)
        {
            std::fputs("FAIL: strict receipt policy check\n", stderr);
            std::exit(EXIT_FAILURE);
        }
        ++checks;
    };

    std::string const clean = "0 0 1 1 0 0 0 0 ";
    Decoded value = DecodeRaw(clean, true);
    check(value.status == Status::Valid);
    check(static_cast<bool>(value));
    check(!HasPaidReceipt(value.ledger, Era::Tbc));
    check(!HasPaidReceipt(value.ledger, Era::Wrath));
    check(Serialize(value.ledger) == clean);
    check(DecodeRaw("0 0 1 1 0 0 0 0", true).status == Status::Valid);

    check(DecodeRaw(clean, false).status == Status::Missing);
    check(DecodeRaw("", false).status == Status::Missing);
    check(DecodeRaw("", true).status == Status::WrongSlotCount);
    for (char const* raw : {
        "x 0 1", "-1 0 1", "+1 0 1", "0 x 1", "0 1a 1",
        "0 4294967296 1", "0 1 1 1 0 0 0 0 42",
        "0 0 1 1 0 0 0", "0  0 1 1 0 0 0 0",
        " 0 0 1 1 0 0 0 0", "0 0 1 1 0 0 0 0  ",
        "0\t0 1 1 0 0 0 0", "0 0 1 1 0 0 0 -1",
        "0 0 1 1 0 0 0 4294967296"
    })
        check(DecodeRaw(raw, true).status != Status::Valid);

    check(DecodeRaw("0 0 1 0 0 0 0 0 ", true).status == Status::UnknownVersion);
    check(DecodeRaw("0 0 1 2 0 0 0 0 ", true).status == Status::UnknownVersion);
    check(DecodeRaw("1 0 1 1 0 0 0 0 ", true).status == Status::Inconsistent);
    check(DecodeRaw("1 0 1 1 1 0 0 0 ", true).status == Status::Inconsistent);
    check(DecodeRaw("0 1 1 1 0 0 0 10 ", true).status == Status::Inconsistent);
    check(DecodeRaw("3 0 1 1 0 0 0 0 ", true).status == Status::Inconsistent);
    check(DecodeRaw("0 0 2 1 0 0 0 0 ", true).status == Status::Inconsistent);
    check(DecodeRaw("0 0 1 1 10 0 0 0 ", true).status == Status::Inconsistent);

    Ledger ledger = value.ledger;
    check(!RecordPurchase(ledger, Era::Tbc, 0, 1793620000));
    check(!RecordPurchase(ledger, Era::Tbc, 50000, 0));
    check(!RecordPurchase(ledger, Era::Tbc, MaxChargedCopper + 1u, 1793620000));
    check(Serialize(ledger) == clean);
    check(RecordPurchase(ledger, Era::Tbc, 50000, 1793620000));
    check(HasPaidReceipt(ledger, Era::Tbc));
    check(!HasPaidReceipt(ledger, Era::Wrath));
    check(!RecordPurchase(ledger, Era::Tbc, 50000, 1793620001));
    check(ledger.fields[TbcCopper] == 50000 && ledger.fields[TbcEpoch] == 1793620000);

    check(RecordPurchase(ledger, Era::Wrath, 70000, 1793620010));
    check(HasPaidReceipt(ledger, Era::Wrath));
    check(ledger.fields[WrathCopper] == 70000 && ledger.fields[WrathEpoch] == 1793620010);
    check(DecodeRaw(Serialize(ledger), true).status == Status::Valid);

    Ledger copy = ledger;
    copy.fields[TbcCopper] = 0;
    check(!ValidLedger(copy));
    check(!HasPaidReceipt(copy, Era::Tbc));
    check(!RecordPurchase(copy, Era::Wrath, 70000, 1793620010));

    // Grok L2: ValidLedger is also called directly on cached/staged ledgers,
    // so it must independently reject invalid v1 versions (not just DecodeRaw).
    Ledger invalidVersion = value.ledger;
    invalidVersion.fields[Version] = 0;
    check(!ValidLedger(invalidVersion));
    check(!RecordPurchase(invalidVersion, Era::Tbc, 50000, 1793620010));

    Ledger inherited = value.ledger;
    inherited.fields[WrathFlag] = FlagGrandfathered;
    check(ValidLedger(inherited));
    check(!HasPaidReceipt(inherited, Era::Wrath));
    check(!RecordPurchase(inherited, Era::Wrath, 50000, 1793620020));
    check(DecodeRaw(Serialize(inherited), true).status == Status::Valid);

    std::cout << "RECEIPT POLICY CHECKS " << checks << " passed\n";
}
