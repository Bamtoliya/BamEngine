#pragma once

#include "Types.h"

namespace Editor
{
    struct AssetTaskMetrics
    {
        Engine::uint64 Outstanding = 0;
        Engine::uint64 Succeeded = 0;
        Engine::uint64 Failed = 0;

        Engine::uint64 GetCompleted() const
        {
            return Succeeded + Failed;
        }
    };
}