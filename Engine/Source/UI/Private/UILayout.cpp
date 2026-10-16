#include "UILayout.h"
#include "CoreComponents.h"
#include <cmath>
#include <unordered_set>
#include <algorithm>

namespace
{
    bool Finite(const glm::vec2& value) { return std::isfinite(value.x) && std::isfinite(value.y); }
    bool Finite(const glm::mat4& value)
    {
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                if (!std::isfinite(value[column][row])) return false;
        return true;
    }

    bool Visible(const entt::registry& world, entt::entity entity)
    {
        const auto* flag = world.try_get<Engine::FlagComponent>(entity);
        return flag && HasFlag(flag->flags, Engine::EEntityFlag::Active) &&
            HasFlag(flag->flags, Engine::EEntityFlag::Visible) && !HasFlag(flag->flags, Engine::EEntityFlag::Dead);
    }
}

bool Engine::UILayout::IsValid(const CanvasComponent& canvas)
{
    const auto size = canvas.referenceResolution;
    return Finite(size) && size.x >= 1.0f && size.y >= 1.0f && size.x <= 8192.0f && size.y <= 8192.0f &&
        std::floor(size.x) == size.x && std::floor(size.y) == size.y;
}

bool Engine::UILayout::IsValid(const RectTransformComponent& rect)
{
    return Finite(rect.anchorMin) && Finite(rect.anchorMax) && Finite(rect.anchoredPosition) &&
        Finite(rect.sizeDelta) && Finite(rect.pivot) && Finite(rect.scale) && std::isfinite(rect.rotation) &&
        glm::all(glm::greaterThanEqual(rect.anchorMin, glm::vec2(0.0f))) &&
        glm::all(glm::lessThanEqual(rect.anchorMax, glm::vec2(1.0f))) &&
        glm::all(glm::lessThanEqual(rect.anchorMin, rect.anchorMax)) &&
        glm::all(glm::greaterThanEqual(rect.pivot, glm::vec2(0.0f))) &&
        glm::all(glm::lessThanEqual(rect.pivot, glm::vec2(1.0f)));
}

bool Engine::UILayout::IsValid(const UIImageComponent& image)
{
    for (int channel = 0; channel < 4; ++channel)
        if (!std::isfinite(image.color[channel]) || image.color[channel] < 0.0f || image.color[channel] > 1.0f)
            return false;
    return true;
}

bool Engine::UILayout::IsValid(const UIButtonComponent& button)
{
    for (const auto color : { button.normalColor, button.hoveredColor, button.pressedColor, button.disabledColor })
        for (int channel = 0; channel < 4; ++channel)
            if (!std::isfinite(color[channel]) || color[channel] < 0.0f || color[channel] > 1.0f) return false;
    return true;
}

entt::entity Engine::UILayout::FindCanvas(const entt::registry& world, entt::entity entity)
{
    std::unordered_set<entt::entity> visited;
    while (entity != entt::null && world.valid(entity) && visited.size() < 256 && visited.insert(entity).second)
    {
        if (world.all_of<CanvasComponent>(entity)) return entity;
        const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
        entity = hierarchy ? hierarchy->parent : entt::null;
    }
    return entt::null;
}

std::int32_t Engine::UILayout::GetZIndex(const entt::registry& world, entt::entity entity)
{
    const auto* order = world.valid(entity) ? world.try_get<UIRenderOrderComponent>(entity) : nullptr;
    return order ? order->zIndex : 0;
}

bool Engine::UILayout::Resolve(const RectTransformComponent& rect, const glm::vec2& parentSize,
    const glm::vec2& parentPivot, const glm::mat4& parentTransform, UIElementLayout& result)
{
    if (!IsValid(rect) || !Finite(parentSize) || !Finite(parentPivot) || !Finite(parentTransform) ||
        glm::any(glm::lessThan(parentSize, glm::vec2(0.0f)))) return false;
    UIElementLayout candidate;
    candidate.size = parentSize * (rect.anchorMax - rect.anchorMin) + rect.sizeDelta;
    if (!Finite(candidate.size) || glm::any(glm::lessThan(candidate.size, glm::vec2(0.0f)))) return false;
    const glm::vec2 anchor = rect.anchorMin + (rect.anchorMax - rect.anchorMin) * rect.pivot;
    const glm::vec2 position = parentSize * (anchor - parentPivot) + rect.anchoredPosition;
    const glm::mat4 local = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), glm::radians(rect.rotation), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(rect.scale, 1.0f));
    candidate.worldTransform = parentTransform * local;
    candidate.renderMatrix = candidate.worldTransform *
        glm::scale(glm::mat4(1.0f), glm::vec3(candidate.size, 1.0f)) *
        glm::translate(glm::mat4(1.0f), glm::vec3(-rect.pivot, 0.0f));
    candidate.parentTransform = parentTransform;
    candidate.parentSize = parentSize;
    candidate.parentPivot = parentPivot;
    if (!Finite(candidate.worldTransform) || !Finite(candidate.renderMatrix)) return false;
    result = candidate;
    return true;
}

bool Engine::UILayout::Build(const entt::registry& world, entt::entity canvas, std::vector<UIElementLayout>& result)
{
    const auto* root = world.valid(canvas) ? world.try_get<CanvasComponent>(canvas) : nullptr;
    if (!root || !IsValid(*root)) return false;
    const auto* rootRelation = world.try_get<HierarchyComponent>(canvas);
    if (!rootRelation || rootRelation->parent != entt::null) return false; // Nested canvases are a later stage.
    std::vector<UIElementLayout> candidate;
    std::unordered_set<entt::entity> visited{ canvas };
    const auto orderedChildren = [&](const std::vector<entt::entity>& children)
    {
        auto ordered = children;
        std::stable_sort(ordered.begin(), ordered.end(), [&](auto left, auto right)
        {
            return GetZIndex(world, left) < GetZIndex(world, right);
        });
        return ordered;
    };
    const auto visit = [&](auto&& self, entt::entity entity, entt::entity parent, const glm::vec2& parentSize,
        const glm::vec2& parentPivot, const glm::mat4& parentMatrix, bool parentVisible, std::size_t depth) -> bool
    {
        if (depth > 256 || !world.valid(entity) || !visited.insert(entity).second ||
            world.all_of<CanvasComponent>(entity)) return false;
        const auto* rect = world.try_get<RectTransformComponent>(entity);
        const auto* relation = world.try_get<HierarchyComponent>(entity);
        if (!rect || !relation || relation->parent != parent) return false;
        UIElementLayout layout;
        if (!Resolve(*rect, parentSize, parentPivot, parentMatrix, layout)) return false;
        layout.entity = entity;
        layout.visible = parentVisible && Visible(world, entity);
        candidate.push_back(layout);
        for (const auto child : orderedChildren(relation->children))
            if (!self(self, child, entity, layout.size, rect->pivot, layout.worldTransform, layout.visible, depth + 1))
                return false;
        return true;
    };
    for (const auto child : orderedChildren(rootRelation->children))
        if (!visit(visit, child, canvas, root->referenceResolution, glm::vec2(0.0f), glm::mat4(1.0f),
            Visible(world, canvas), 2)) return false;
    result.swap(candidate);
    return true;
}

bool Engine::UILayout::Contains(const UIElementLayout& layout, const glm::vec2& canvasPoint)
{
    const float determinant = glm::determinant(layout.renderMatrix);
    if (!layout.visible || !Finite(canvasPoint) || !std::isfinite(determinant) || determinant == 0.0f) return false;
    const glm::vec2 local(glm::inverse(layout.renderMatrix) * glm::vec4(canvasPoint, 0.0f, 1.0f));
    return Finite(local) && local.x >= 0.0f && local.y >= 0.0f && local.x <= 1.0f && local.y <= 1.0f;
}
