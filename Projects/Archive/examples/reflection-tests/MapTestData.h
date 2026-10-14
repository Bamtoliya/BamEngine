#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>

namespace archive_reflection_tests
{
    struct MapSettings
    {
        std::map<std::int64_t, std::wstring> Numbers{ { -1, L"음수" }, { 10, L"한글" } };
        std::unordered_map<std::wstring, std::uint64_t> Counts
        {
            { L"플레이어", 3 },
            { L"최대", std::numeric_limits<std::uint64_t>::max() }
        };
    };
}