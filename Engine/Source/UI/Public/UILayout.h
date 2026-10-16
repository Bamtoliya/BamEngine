#pragma once

#include "Engine_Includes.h"
#include "UIComponents.h"

namespace Engine
{
    // Computed values are returned to the caller, never inserted into serialized ECS storage.
    struct UIElementLayout
    {
        entt::entity entity = entt::null;
        glm::vec2 size{ 0.0f };
        glm::mat4 worldTransform{ 1.0f }; // Local pivot coordinates -> canvas pixels, Y down.
        glm::mat4 renderMatrix{ 1.0f }; // Unit quad [0, 1] -> canvas pixels.
        glm::mat4 parentTransform{ 1.0f };
        glm::vec2 parentSize{ 0.0f };
        glm::vec2 parentPivot{ 0.0f };
        bool visible = true;
    };

    class ENGINE_API UILayout
    {
    public:
        static bool IsValid(const CanvasComponent& canvas);
        static bool IsValid(const RectTransformComponent& rect);
        static bool IsValid(const UIImageComponent& image);
        static bool IsValid(const UIButtonComponent& button);
        static entt::entity FindCanvas(const entt::registry& world, entt::entity entity);
        static std::int32_t GetZIndex(const entt::registry& world, entt::entity entity);
        static bool Resolve(const RectTransformComponent& rect, const glm::vec2& parentSize,
            const glm::vec2& parentPivot, const glm::mat4& parentTransform, UIElementLayout& result);
        static bool Build(const entt::registry& world, entt::entity canvas, std::vector<UIElementLayout>& result);
        static bool Contains(const UIElementLayout& layout, const glm::vec2& canvasPoint);
    };
}
