#pragma once

#include "UILayout.h"

namespace Engine
{
    class ENGINE_API UIClipLayout
    {
    public:
        static void Apply(const entt::registry& world, std::vector<UIElementLayout>& layouts);
        // Positive wheel movement scrolls toward the start of the content.
        static bool ScrollAt(entt::registry& world, const std::vector<UIElementLayout>& layouts,
            const glm::vec2& canvasPosition, float wheelDelta);
    };
}
