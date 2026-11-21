#include "UIClipLayout.h"
#include "UIClipComponent.h"
#include "UIScrollComponent.h"
#include "ComponentEnable.h"
#include "WorldComponents.h"
#include <algorithm>

namespace
{
    using Engine::UIElementLayout;
    using Engine::UIScrollComponent;

    glm::vec2 TransformPoint(const glm::mat4& matrix, float x, float y)
    {
        return glm::vec2(matrix * glm::vec4(x, y, 0.0f, 1.0f));
    }

    glm::vec4 Bounds(const glm::mat4& matrix)
    {
        const auto origin = TransformPoint(matrix, 0.0f, 0.0f);
        const auto x = TransformPoint(matrix, 1.0f, 0.0f);
        const auto y = TransformPoint(matrix, 0.0f, 1.0f);
        const auto opposite = TransformPoint(matrix, 1.0f, 1.0f);
        const auto minimum = glm::min(glm::min(origin, x), glm::min(y, opposite));
        const auto maximum = glm::max(glm::max(origin, x), glm::max(y, opposite));
        return { minimum, maximum };
    }

    bool AxisAligned(const glm::mat4& matrix)
    {
        const glm::vec2 x(matrix[0]), y(matrix[1]);
        const float xSize = glm::length(x), ySize = glm::length(y);
        if (!(xSize > 0.0f && ySize > 0.0f) || !std::isfinite(xSize) || !std::isfinite(ySize)) return false;

        constexpr float tolerance = 0.00001f;
        return (std::abs(x.y) <= xSize * tolerance && std::abs(y.x) <= ySize * tolerance) ||
            (std::abs(x.x) <= xSize * tolerance && std::abs(y.y) <= ySize * tolerance);
    }

    glm::vec4 Intersect(const glm::vec4& a, const glm::vec4& b)
    {
        return { std::max(a.x, b.x), std::max(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w) };
    }

    bool NonEmpty(const glm::vec4& rectangle)
    {
        return rectangle.x < rectangle.z && rectangle.y < rectangle.w;
    }

    float ScrollRange(const entt::registry& world, const UIElementLayout& viewport)
    {
        if (!AxisAligned(viewport.renderMatrix)) return 0.0f;
        const auto* relation = world.try_get<Engine::HierarchyComponent>(viewport.entity);
        const auto* rect = world.try_get<Engine::RectTransformComponent>(viewport.entity);
        if (!relation || !rect || relation->children.size() != 1) return 0.0f;

        const auto* content = Engine::TryGetEnabledComponent<Engine::RectTransformComponent>(world, relation->children.front());
        if (!content) return 0.0f;
        UIElementLayout contentLayout;
        if (!Engine::UILayout::Resolve(*content, viewport.size, rect->pivot, viewport.worldTransform, contentLayout))
            return 0.0f;
        return std::max(0.0f, contentLayout.size.y - viewport.size.y);
    }

    struct ClippingState
    {
        std::optional<glm::vec4> rectangle;
        glm::vec2 displacement{ 0.0f };
    };
}

void Engine::UIClipLayout::Apply(const entt::registry& world, std::vector<UIElementLayout>& layouts)
{
    std::unordered_map<entt::entity, ClippingState> states;
    for (auto& layout : layouts)
    {
        const auto* relation = world.try_get<HierarchyComponent>(layout.entity);
        const auto parent = relation ? states.find(relation->parent) : states.end();
        ClippingState state = parent != states.end() ? parent->second : ClippingState{};

        if (state.displacement != glm::vec2(0.0f))
        {
            const auto translation = glm::translate(glm::mat4(1.0f), glm::vec3(state.displacement, 0.0f));
            layout.worldTransform = translation * layout.worldTransform;
            layout.renderMatrix = translation * layout.renderMatrix;
            layout.parentTransform = translation * layout.parentTransform;
        }

        layout.clipRect = state.rectangle;
        if (layout.visible && state.rectangle)
            layout.visible = NonEmpty(Intersect(*state.rectangle, Bounds(layout.renderMatrix)));

        const auto* scroll = TryGetEnabledComponent<UIScrollComponent>(world, layout.entity);
        const bool clips = world.all_of<UIClipComponent>(layout.entity) && IsComponentEnabled<UIClipComponent>(world, layout.entity);
        if (scroll || clips)
        {
            // Rotated or degenerate mask rectangles clip the subtree completely.
            const auto bounds = AxisAligned(layout.renderMatrix) ? Bounds(layout.renderMatrix) : glm::vec4(0.0f);
            state.rectangle = state.rectangle ? Intersect(*state.rectangle, bounds) : bounds;
            if (scroll && std::isfinite(scroll->offset))
            {
                const float offset = std::clamp(scroll->offset, 0.0f, ScrollRange(world, layout));
                state.displacement -= glm::vec2(layout.worldTransform[1]) * offset;
            }
        }
        if (state.rectangle || state.displacement != glm::vec2(0.0f))
            states.emplace(layout.entity, state);
    }
}

bool Engine::UIClipLayout::ScrollAt(entt::registry& world, const std::vector<UIElementLayout>& layouts,
    const glm::vec2& canvasPosition, float wheelDelta)
{
    if (!std::isfinite(wheelDelta) || wheelDelta == 0.0f) return false;
    for (auto it = layouts.rbegin(); it != layouts.rend(); ++it)
    {
        if (!UILayout::Contains(*it, canvasPosition)) continue;

        bool consumed = false;
        const UIElementLayout* current = &*it;
        while (current)
        {
            auto* scroll = TryGetEnabledComponent<UIScrollComponent>(world, current->entity);
            if (scroll && std::isfinite(scroll->wheelStep) && scroll->wheelStep > 0.0f)
            {
                const float range = ScrollRange(world, *current);
                if (range > 0.0f)
                {
                    consumed = true;
                    const float old = std::isfinite(scroll->offset) ? std::clamp(scroll->offset, 0.0f, range) : 0.0f;
                    const float next = std::clamp(old - wheelDelta * scroll->wheelStep, 0.0f, range);
                    scroll->offset = next;
                    if (next != old) return true;
                }
            }

            const auto* relation = world.try_get<HierarchyComponent>(current->entity);
            if (!relation) break;
            const auto parent = std::ranges::find(layouts, relation->parent, &UIElementLayout::entity);
            current = parent != layouts.end() ? &*parent : nullptr;
        }

        // An opaque input blocker prevents scrolling widgets drawn behind it.
        if (consumed) return true;
        const auto* image = TryGetEnabledComponent<UIImageComponent>(world, it->entity);
        if (image && image->raycastTarget) return false;
    }
    return false;
}
