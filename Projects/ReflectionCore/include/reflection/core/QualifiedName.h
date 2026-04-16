#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace reflection
{
    constexpr std::uint64_t CompileTimeHash(std::string_view text) noexcept
    {
        std::uint64_t value = 14695981039346656037ull;
        for (char ch : text)
        {
            value ^= static_cast<std::uint64_t>(static_cast<unsigned char>(ch));
            value *= 1099511628211ull;
        }
        return value;
    }

    inline bool IsQualifiedName(std::string_view name)
    {
        return name.find("::") != std::string_view::npos;
    }

    inline std::string_view GetShortNameFromQualifiedName(std::string_view qualified_name)
    {
        const std::size_t pos = qualified_name.rfind("::");
        if (pos == std::string_view::npos)
        {
            return qualified_name;
        }
        return qualified_name.substr(pos + 2);
    }

    inline std::string_view GetNamespaceFromQualifiedName(std::string_view qualified_name)
    {
        const std::size_t pos = qualified_name.rfind("::");
        if (pos == std::string_view::npos)
        {
            return {};
        }
        return qualified_name.substr(0, pos);
    }

    inline std::string MakeQualifiedMemberName(std::string_view owner_qualified_name, std::string_view member_name)
    {
        if (owner_qualified_name.empty())
        {
            return std::string(member_name);
        }

        std::string result;
        result.reserve(owner_qualified_name.size() + 2 + member_name.size());
        result.append(owner_qualified_name);
        result.append("::");
        result.append(member_name);
        return result;
    }
}