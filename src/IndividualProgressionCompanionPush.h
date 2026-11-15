#pragma once
#include <cstdint>
#include <optional>
#include <string>

// Version 1 legacy addon whisper. Raw marker stage, NOT entitlement or
// effective progression. No message for a no-op or an invalid stage.
namespace IndividualProgressionCompanionPush
{
inline std::optional<std::string> Payload(std::uint8_t before, std::uint8_t after)
{
    if (before > 18 || after > 18 || before == after)
        return std::nullopt;
    return "IPPSTATE\t1\t" + std::to_string(after);
}
}
