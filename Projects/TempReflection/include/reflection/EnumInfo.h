#pragma once

#include <reflection/Metadata.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace reflection
{
    using EnumValue = std::variant<
        std::int64_t,
        std::uint64_t
    >;

    struct EnumEntry
    {
        std::string Name;
        EnumValue Value;
    };

    struct EnumInfo
    {
        std::string QualifiedName;
        std::string UnderlyingType;
        bool IsScoped = false;

        std::vector<EnumEntry> Entries;
        reflection::Metadata Metadata;

        [[nodiscard]]
        const EnumEntry* FindEntry(std::string_view name) const
        {
            for (const EnumEntry& entry : Entries)
            {
                if (entry.Name == name)
                {
                    return &entry;
                }
            }

            return nullptr;
        }
    };
}