#pragma once

#include <cstdint>
#include <variant>

namespace reflection
{
    using EnumValue = std::variant<std::int64_t, std::uint64_t>;
}