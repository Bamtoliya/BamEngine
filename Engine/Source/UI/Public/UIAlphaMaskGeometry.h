#pragma once

#include "ResourceHandle.h"
#include <array>
#include <cstddef>
#include <glm/glm.hpp>

namespace Engine
{
    class Texture;

    inline constexpr std::size_t MaxUIAlphaMasks = 2;

    struct UIAlphaMaskRegion
    {
        ResourceHandle<Texture> texture;
        glm::vec4 uvTransform{ 0.0f, 0.0f, 1.0f, 1.0f };
        // Dot each plane's XYZ with (canvasX, canvasY, 1) to get normalized mask coordinates.
        glm::vec4 xPlane{ 0.0f };
        glm::vec4 yPlane{ 0.0f };
    };

    using UIAlphaMaskRegions = std::array<UIAlphaMaskRegion, MaxUIAlphaMasks>;

    // Bilinear filtering must stay inside the sprite's texel centers. A subpixel region uses its midpoint.
    inline glm::vec4 UIAlphaMaskUVBounds(const glm::vec4& uvTransform, const glm::vec2& textureSize)
    {
        const glm::vec2 start(uvTransform);
        const glm::vec2 span(uvTransform.z, uvTransform.w);
        const auto midpoint = start + span * 0.5f;
        const auto halfTexel = glm::vec2(0.5f) / textureSize;
        return { glm::min(start + halfTexel, midpoint), glm::max(start + span - halfTexel, midpoint) };
    }
}
