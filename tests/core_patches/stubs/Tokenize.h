#pragma once
#include <string_view>
#include <vector>

namespace Acore
{
    inline std::vector<std::string_view> Tokenize(std::string_view data, char delimiter, bool)
    {
        std::vector<std::string_view> result;
        std::size_t i = 0;
        for (;;)
        {
            auto const j = data.find(delimiter, i);
            result.emplace_back(data.substr(i, j == std::string_view::npos ? j : j - i));
            if (j == std::string_view::npos)
                break;
            i = j + 1;
        }
        return result;
    }
}
