#pragma once
#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace Acore
{
    template <typename T> std::optional<T> StringTo(std::string_view token)
    {
        if (token.empty())
            return std::nullopt;
        T result{};
        auto const parsed = std::from_chars(token.data(), token.data() + token.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
            return std::nullopt;
        return result;
    }
}
