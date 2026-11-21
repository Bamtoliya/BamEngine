#pragma once

#include <array>
#include <cstddef>
#include <glm/glm.hpp>

namespace Engine
{
    inline constexpr std::size_t MaxUIEllipseMasks = 4;
    // Each pair transforms a canvas-space point into the unit-circle coordinate system.
    using UIEllipseMaskPlanes = std::array<glm::vec4, MaxUIEllipseMasks * 2>;
}
