#pragma once

#include "UILayout.h"

namespace Engine
{
    struct UIScrollbarLayout
    {
        float width;
        float thumbTop;
        float thumbHeight;
        float scrollRange;
    };

    struct UIScrollbarOverlay
    {
        std::size_t index;
        UIScrollbarLayout geometry;
    };

    class ENGINE_API UIClipLayout
    {
    public:
        static void Apply(const entt::registry& world, std::vector<UIElementLayout>& layouts);
        static entt::entity FindScrollContent(const entt::registry& world, entt::entity viewport);
        static std::optional<UIScrollbarLayout> GetScrollbar(const entt::registry& world, const UIElementLayout& viewport);
        static std::vector<UIScrollbarOverlay> GetScrollbarOverlays(const entt::registry& world, const std::vector<UIElementLayout>& layouts);
        static bool IsScrollbarHit(const UIElementLayout& viewport, const UIScrollbarLayout& scrollbar, const glm::vec2& point);
        static glm::vec2 ToScrollbarLocal(const UIElementLayout& viewport, const glm::vec2& point);
        // Positive wheel movement scrolls toward the start of the content.
        static bool ScrollAt(entt::registry& world, const std::vector<UIElementLayout>& layouts,
            const glm::vec2& canvasPosition, float wheelDelta);
    };
}
