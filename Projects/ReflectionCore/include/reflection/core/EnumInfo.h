#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace reflection
{
    struct EnumEntry
    {
        std::string_view Name{};
        std::uint64_t Value = 0;
    };

    struct EnumInfo
    {
        std::uint64_t ID = 0;
        std::string_view QualifiedName{};
        std::span<const EnumEntry> Entries{};

        std::string GetBitFlagsString(std::uint64_t value) const
        {
            if (value == 0)
            {
                for (const EnumEntry& entry : Entries)
                {
                    if (entry.Value == 0)
                    {
                        return std::string(entry.Name);
                    }
                }
                return "None";
            }

            std::string result;
            for (const EnumEntry& entry : Entries)
            {
                if (entry.Value == 0)
                {
                    continue;
                }

                if ((value & entry.Value) == entry.Value)
                {
                    if (!result.empty())
                    {
                        result += " | ";
                    }
                    result += entry.Name;
                }
            }

            return result.empty() ? std::to_string(value) : result;
        }
    };
}