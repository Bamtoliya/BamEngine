#pragma once

#include <cstdint>
#include <limits>

namespace archive_tests
{
    enum class MovementMode : std::uint8_t
    {
        Walk = 0,
        Run = 1
    };

    enum class Permission : std::uint64_t
    {
        None = 0,
        Read = 1,
        Write = 2,
        High = std::uint64_t{ 1 } << 63,
        Maximum = (std::numeric_limits<std::uint64_t>::max)()
    };

    constexpr Permission operator|(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<std::uint64_t>(lhs) | static_cast<std::uint64_t>(rhs));
    }

    constexpr Permission operator&(Permission lhs, Permission rhs)
    {
        return static_cast<Permission>(static_cast<std::uint64_t>(lhs) & static_cast<std::uint64_t>(rhs));
    }

    enum class SignedState : std::int8_t
    {
        Disabled = -1,
        Enabled = 1
    };
}