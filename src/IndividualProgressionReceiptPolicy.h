/*
 * CosmicCuddle IP — standalone purchase receipt policy (development only).
 *
 * Strictly reads the RAW character_settings data, never Core's permissively
 * parsed GetPlayerSetting vector. No DB, Player or wall-clock dependencies.
 * This helper is NOT YET wired to a production purchase. Unknown is NOT free.
 */
#ifndef COSMIC_IP_PAID_RECEIPT_POLICY_H
#define COSMIC_IP_PAID_RECEIPT_POLICY_H

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

namespace IndividualProgressionReceiptPolicy
{
    constexpr std::size_t Slots = 8;
    constexpr std::uint32_t Version1 = 1;
    constexpr std::uint32_t FlagNone = 0;
    constexpr std::uint32_t FlagPaid = 1;
    constexpr std::uint32_t FlagGrandfathered = 2;
    constexpr std::uint32_t MaxChargedCopper =
        static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());

    enum class Status
    {
        Valid, Missing, Malformed, WrongSlotCount, UnknownVersion, Inconsistent
    };

    enum class Era : std::uint8_t { Tbc, Wrath };

    // Slots 0–2 are HISTORIC and must NEVER move. 3 is the nonzero
    // version/presence sentinel. 4–7 are two cost/time receipt pairs.
    enum Slot : std::size_t
    {
        TbcFlag = 0, WrathFlag = 1, GrandfatherEvaluated = 2,
        Version = 3, TbcCopper = 4, TbcEpoch = 5,
        WrathCopper = 6, WrathEpoch = 7
    };

    struct Ledger
    {
        std::array<std::uint32_t, Slots> fields{};
    };

    struct Decoded
    {
        Status status = Status::Missing;
        Ledger ledger{};
        explicit operator bool() const { return status == Status::Valid; }
    };

    inline bool DecimalUInt32(std::string_view token, std::uint32_t& result)
    {
        if (token.empty())
            return false;
        for (char character : token)
            if (character < '0' || character > '9')
                return false;
        auto const conversion = std::from_chars(token.data(), token.data() + token.size(), result);
        return conversion.ec == std::errc{} &&
            conversion.ptr == token.data() + token.size();
    }

    inline bool PairValid(std::uint32_t flag, std::uint32_t copper, std::uint32_t epoch)
    {
        if (flag == FlagNone || flag == FlagGrandfathered)
            return copper == 0 && epoch == 0;
        return flag == FlagPaid && copper > 0 && copper <= MaxChargedCopper && epoch > 0;
    }

    inline bool ValidLedger(Ledger const& ledger)
    {
        auto const& f = ledger.fields;
        return (f[TbcFlag] == FlagNone || f[TbcFlag] == FlagPaid ||
                f[TbcFlag] == FlagGrandfathered) &&
            (f[WrathFlag] == FlagNone || f[WrathFlag] == FlagPaid ||
             f[WrathFlag] == FlagGrandfathered) &&
            f[GrandfatherEvaluated] <= 1 && f[Version] == Version1 &&
            PairValid(f[TbcFlag], f[TbcCopper], f[TbcEpoch]) &&
            PairValid(f[WrathFlag], f[WrathCopper], f[WrathEpoch]);
    }

    // Core SerializeSettingsData emits a single trailing space. Accept only
    // that optional terminator; repeated separators, junk, sign, overflow and
    // extra/missing slots MUST NOT shift indexes or mint entitlements.
    inline Decoded DecodeRaw(std::string_view data, bool rowPresent)
    {
        if (!rowPresent)
            return { Status::Missing, {} };
        if (data.empty())
            return { Status::WrongSlotCount, {} };
        if (data.back() == ' ')
            data.remove_suffix(1);
        if (data.empty())
            return { Status::Malformed, {} };

        Ledger ledger;
        std::size_t i = 0;
        while (!data.empty())
        {
            if (i == Slots)
                return { Status::WrongSlotCount, {} };
            std::size_t const separator = data.find(' ');
            std::string_view const token = data.substr(0, separator);
            if (!DecimalUInt32(token, ledger.fields[i++]))
                return { Status::Malformed, {} };
            if (separator == std::string_view::npos)
                break;
            data.remove_prefix(separator + 1);
            if (data.empty())
                return { Status::Malformed, {} };
        }
        if (i != Slots)
            return { Status::WrongSlotCount, {} };
        if (ledger.fields[Version] != Version1)
            return { Status::UnknownVersion, {} };
        if (!ValidLedger(ledger))
            return { Status::Inconsistent, {} };
        return { Status::Valid, ledger };
    }

    inline std::string Serialize(Ledger const& ledger)
    {
        std::string data;
        for (std::uint32_t value : ledger.fields)
        {
            data += std::to_string(value);
            data += ' ';
        }
        return data;
    }

    inline bool HasPaidReceipt(Ledger const& ledger, Era era)
    {
        if (!ValidLedger(ledger))
            return false;
        auto const flag = era == Era::Tbc ? TbcFlag : WrathFlag;
        return ledger.fields[flag] == FlagPaid;
    }

    // This stages an IN-MEMORY copy only. Caller must eventually write all
    // slots through Player::UpdatePlayerSetting before one SaveToDB transaction.
    // There must be no receipt after denial, double purchase or refund.
    inline bool RecordPurchase(Ledger& ledger, Era era,
        std::uint32_t chargedCopper, std::uint32_t epoch)
    {
        if (!ValidLedger(ledger) || chargedCopper == 0 ||
            chargedCopper > MaxChargedCopper || epoch == 0)
            return false;

        std::size_t const flag = era == Era::Tbc ? TbcFlag : WrathFlag;
        std::size_t const money = era == Era::Tbc ? TbcCopper : WrathCopper;
        std::size_t const time = era == Era::Tbc ? TbcEpoch : WrathEpoch;
        if (ledger.fields[flag] != FlagNone)
            return false; // paid / grandfathered / duplicate: never replace
        ledger.fields[flag] = FlagPaid;
        ledger.fields[money] = chargedCopper;
        ledger.fields[time] = epoch;
        return true;
    }
}

#endif
