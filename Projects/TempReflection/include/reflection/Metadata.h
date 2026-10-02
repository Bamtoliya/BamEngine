#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace reflection
{
    using MetadataValue = std::variant<bool, std::int64_t, std::uint64_t, double, std::string>;

    struct MetadataEntry
    {
        std::string Key;
        MetadataValue Value;
    };

    class Metadata
    {
    public:
        // 같은 키가 이미 있으면 추가하지 않고 false를 반환합니다.
        [[nodiscard]]
        bool Add(std::string key, MetadataValue value);

        // 키가 없으면 nullptr를 반환합니다.
        [[nodiscard]]
        const MetadataValue* Find(std::string_view key) const;

    private:
        std::vector<MetadataEntry> m_entries;
    };
}