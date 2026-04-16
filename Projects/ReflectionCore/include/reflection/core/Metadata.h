#pragma once

#include "reflection/core/QualifiedName.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

namespace reflection
{
    using MetadataValue = std::variant<
        std::monostate,
        std::string_view,
        std::wstring_view,
        bool,
        std::int64_t,
        std::uint64_t,
        double
    >;

    constexpr MetadataValue MakeMetadataValue(const char* value)
    {
        return std::string_view(value);
    }

    constexpr MetadataValue MakeMetadataValue(std::string_view value)
    {
        return value;
    }

    constexpr MetadataValue MakeMetadataValue(const wchar_t* value)
    {
        return std::wstring_view(value);
    }

    constexpr MetadataValue MakeMetadataValue(std::wstring_view value)
    {
        return value;
    }

    constexpr MetadataValue MakeMetadataValue(bool value)
    {
        return value;
    }

    constexpr MetadataValue MakeMetadataValue(std::int64_t value)
    {
        return value;
    }

    constexpr MetadataValue MakeMetadataValue(std::uint64_t value)
    {
        return value;
    }

    constexpr MetadataValue MakeMetadataValue(double value)
    {
        return value;
    }

    struct MetadataEntry
    {
        std::uint64_t KeyHash = 0;
        MetadataValue Value = std::monostate{};

        constexpr MetadataEntry() = default;

        constexpr MetadataEntry(std::uint64_t key_hash, MetadataValue value)
            : KeyHash(key_hash)
            , Value(value)
        {
        }

        static constexpr const MetadataEntry* Find(std::span<const MetadataEntry> entries, std::uint64_t key_hash)
        {
            for (const MetadataEntry& entry : entries)
            {
                if (entry.KeyHash == key_hash)
                {
                    return &entry;
                }
            }
            return nullptr;
        }
    };
}