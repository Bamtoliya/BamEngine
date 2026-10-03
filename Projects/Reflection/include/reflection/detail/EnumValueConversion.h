#pragma once

#include <reflection/EnumValue.h>

#include <optional>
#include <type_traits>
#include <utility>

namespace reflection::detail
{
    template<typename T>
        requires std::is_enum_v<T>
    std::optional<T> ConvertEnumValue(const EnumValue& value)
    {
        using Underlying = std::underlying_type_t<T>;

        static_assert(!std::is_convertible_v<T, Underlying>, "Numeric enum conversion requires a scoped enum");
        static_assert(sizeof(Underlying) <= sizeof(std::uint64_t), "Enum values wider than 64 bits are not supported");

        return std::visit(
            [](auto input) -> std::optional<T>
            {
                if constexpr (std::is_same_v<Underlying, bool>)
                {
                    if (input != 0 && input != 1)
                    {
                        return std::nullopt;
                    }
                }
                else if (!std::in_range<Underlying>(input))
                {
                    return std::nullopt;
                }

                return static_cast<T>(static_cast<Underlying>(input));
            },
            value);
    }
}