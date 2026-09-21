#pragma once

#include "Resource.h"

#include <array>
#include <cstddef>

namespace Engine
{
    struct ResourceMetrics
    {
        // 현재 EResourceType은 Unknown부터 Save까지 연속된 값입니다.
        static constexpr std::size_t TypeCount =
            static_cast<std::size_t>(EResourceType::Save) + 1;

        uint64 ActiveResources = 0;
        uint64 TotalSlots = 0;
        uint64 FreeSlots = 0;

        std::array<uint64, TypeCount> CountsByType{};
    };
}