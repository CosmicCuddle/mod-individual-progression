// DRAFT / NO SERVER / NO DB — Core-size model for IP v1 receipt slot writes.
// The real receipt policy below is compiled unchanged; only Core's
// Player::UpdatePlayerSetting(index) vector-growth rule is modeled here.
// Source-contract tests independently pin the production PurchaseExpansion
// loop to ReceiptPolicy::Slots and the single SaveToDB call.
#include "../src/IndividualProgressionReceiptPolicy.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace Policy = IndividualProgressionReceiptPolicy;
namespace
{
    int checks = 0;
    int failures = 0;
    void Check(char const* name, bool passed)
    {
        ++checks;
        if (!passed)
        {
            ++failures;
            std::cerr << "FAIL " << name << '\n';
        }
    }

    struct CoreVectorModel
    {
        std::vector<std::uint32_t> fields;
        void UpdatePlayerSetting(std::size_t index, std::uint32_t value)
        {
            // Pinned Core 8daffd... PlayerSettings.cpp: missing vectors
            // have requiredSize=index+1 and later writes grow to index+1.
            if (fields.size() < index + 1)
                fields.resize(index + 1);
            fields[index] = value;
        }

        std::string Serialize() const
        {
            std::string result;
            for (std::uint32_t v : fields)
            {
                result += std::to_string(v);
                result += ' ';
            }
            return result;
        }
    };

    Policy::Ledger EmptyV1()
    {
        Policy::Ledger l;
        l.fields[Policy::Version] = Policy::Version1;
        return l;
    }

    void WriteAllEight(CoreVectorModel& core, Policy::Ledger const& paid)
    {
        for (std::size_t slot = 0; slot < Policy::Slots; ++slot)
            core.UpdatePlayerSetting(slot, paid.fields[slot]);
    }
}

int main()
{
    Policy::Ledger tbc = EmptyV1();
    Check("initial canonical empty ledger valid", Policy::ValidLedger(tbc));
    Check("first TBC purchase staged", Policy::RecordPurchase(tbc, Policy::Era::Tbc, 12345, 1700000000));
    Check("staged TBC ledger valid", Policy::ValidLedger(tbc));

    // Red reproduction: previous code wrote flag 0, version 3, cost 4,
    // epoch 5 only; Core then saved exactly six tokens on a missing row.
    CoreVectorModel old;
    old.UpdatePlayerSetting(Policy::TbcFlag, tbc.fields[Policy::TbcFlag]);
    old.UpdatePlayerSetting(Policy::Version, tbc.fields[Policy::Version]);
    old.UpdatePlayerSetting(Policy::TbcCopper, tbc.fields[Policy::TbcCopper]);
    old.UpdatePlayerSetting(Policy::TbcEpoch, tbc.fields[Policy::TbcEpoch]);
    Check("old TBC first write was six slots", old.fields.size() == 6);
    Check("old TBC first write rejected on login",
          Policy::DecodeRaw(old.Serialize(), true).status == Policy::Status::WrongSlotCount);

    // Green: production now mirrors this complete in-memory vector write
    // after a validated purchase, before one character SaveToDB.
    CoreVectorModel now;
    now.UpdatePlayerSetting(Policy::TbcFlag, Policy::FlagPaid);
    WriteAllEight(now, tbc);
    auto const valid = Policy::DecodeRaw(now.Serialize(), true);
    Check("TBC first write has exactly eight slots", now.fields.size() == Policy::Slots);
    Check("TBC first write has canonical RAW bytes", now.Serialize() == Policy::Serialize(tbc));
    Check("TBC entitlement survives strict login decoder", valid.status == Policy::Status::Valid);
    Check("TBC paid bit survives relog", valid && Policy::HasPaidReceipt(valid.ledger, Policy::Era::Tbc));
    Check("unused Wrath pair explicitly zero", now.fields[Policy::WrathCopper] == 0 &&
          now.fields[Policy::WrathEpoch] == 0);

    Policy::Ledger second = tbc;
    Check("Wrath purchase after TBC staged", Policy::RecordPurchase(second, Policy::Era::Wrath, 98765, 1700000001));
    WriteAllEight(now, second);
    auto const twice = Policy::DecodeRaw(now.Serialize(), true);
    Check("both purchases yield exact eight slots", now.fields.size() == Policy::Slots);
    Check("both receipts survive strict login", twice &&
          Policy::HasPaidReceipt(twice.ledger, Policy::Era::Tbc) &&
          Policy::HasPaidReceipt(twice.ledger, Policy::Era::Wrath));
    Check("receipt does not allow same TBC purchase twice",
          !Policy::RecordPurchase(second, Policy::Era::Tbc, 12345, 1700000002));

    Policy::Ledger wrathFirst = EmptyV1();
    Check("Wrath-only purchase staged", Policy::RecordPurchase(wrathFirst, Policy::Era::Wrath, 67890, 1700000003));
    CoreVectorModel freshWrath;
    freshWrath.UpdatePlayerSetting(Policy::WrathFlag, Policy::FlagPaid);
    WriteAllEight(freshWrath, wrathFirst);
    Check("first Wrath write is also eight slots", freshWrath.fields.size() == Policy::Slots);
    Check("Wrath-only ledger stays valid", Policy::DecodeRaw(freshWrath.Serialize(), true).status == Policy::Status::Valid);

    Policy::Ledger evaluated = EmptyV1();
    evaluated.fields[Policy::GrandfatherEvaluated] = 1;
    Check("previous evaluated flag respected", Policy::RecordPurchase(evaluated, Policy::Era::Tbc, 222, 1700000004));
    CoreVectorModel retained;
    WriteAllEight(retained, evaluated);
    auto const retainedResult = Policy::DecodeRaw(retained.Serialize(), true);
    Check("grandfather evaluation preserved across all slots", retainedResult &&
          retainedResult.ledger.fields[Policy::GrandfatherEvaluated] == 1);

    Check("no row never auto-decodes as paid", Policy::DecodeRaw({}, false).status == Policy::Status::Missing);
    Check("legacy three-slot row still refuses", Policy::DecodeRaw("1 0 0 ", true).status == Policy::Status::WrongSlotCount);
    Policy::Ledger free = EmptyV1();
    Check("zero-price receipt cannot be staged", !Policy::RecordPurchase(free, Policy::Era::Tbc, 0, 1700000004));
    std::cout << "RECEIPT CORE VECTOR MODEL " << (checks - failures) << "/" << checks
              << " passed (no Core link, no DB, no server)" << std::endl;
    return failures ? 1 : 0;
}
