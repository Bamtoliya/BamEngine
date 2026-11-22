#include "UILayout.h"
#include "UIClipLayout.h"
#include "ComponentEnable.h"
#include "WorldComponents.h"
#include <numeric>

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

    glm::vec2 ResolveSize(const Engine::RectTransformComponent& rect, const glm::vec2& parentSize)
    {
        return parentSize * (rect.anchorMax - rect.anchorMin) + rect.sizeDelta;
    }

    struct LayoutSlot
    {
        glm::vec2 origin{ 0.0f };
        glm::vec2 size{ 0.0f };
    };

    bool BuildGroupSlots(const entt::registry& world, const Engine::UILayoutGroupComponent& group,
        const std::vector<entt::entity>& children, const glm::vec2& parentSize, std::vector<LayoutSlot>& slots)
    {
        using Engine::EUILayoutGroupMode;
        const glm::vec2 start(group.padding.x, group.padding.y);
        const glm::vec2 remaining = glm::max(glm::vec2(0.0f), parentSize - start - glm::vec2(group.padding.z, group.padding.w));
        slots.resize(children.size());
        if (slots.empty()) return true;

        if (group.mode == EUILayoutGroupMode::Grid)
        {
            const auto columns = static_cast<std::size_t>(group.columns);
            const auto rows = (slots.size() - 1) / columns + 1;
            const glm::vec2 count(static_cast<float>(std::min(columns, slots.size())), static_cast<float>(rows));
            const glm::vec2 used = count * group.cellSize + (count - 1.0f) * group.spacing;
            const glm::vec2 origin = start + glm::max(glm::vec2(0.0f), remaining - used) * group.alignment;
            const glm::vec2 stride = group.cellSize + group.spacing;
            for (std::size_t index = 0; index < slots.size(); ++index)
            {
                const glm::vec2 grid(static_cast<float>(index % columns), static_cast<float>(index / columns));
                slots[index] = { origin + grid * stride, group.cellSize };
            }
            return true;
        }

        const int axis = group.mode == EUILayoutGroupMode::Row ? 0 : 1;
        const int cross = 1 - axis;
        float used = group.spacing[axis] * static_cast<float>(slots.size() - 1);
        for (std::size_t index = 0; index < slots.size(); ++index)
        {
            const auto entity = children[index];
            const auto* rect = world.valid(entity) ? world.try_get<Engine::RectTransformComponent>(entity) : nullptr;
            if (!rect || !Engine::UILayout::IsValid(*rect)) return false;
            slots[index].size = ResolveSize(*rect, parentSize);
            if (!Finite(slots[index].size) || glm::any(glm::lessThan(slots[index].size, glm::vec2(0.0f))))
                return false;
            used += slots[index].size[axis];
        }
        if (!std::isfinite(used)) return false;
        float cursor = start[axis] + std::max(0.0f, remaining[axis] - used) * group.alignment[axis];
        for (auto& slot : slots)
        {
            slot.origin[axis] = cursor;
            slot.origin[cross] = start[cross] +
                std::max(0.0f, remaining[cross] - slot.size[cross]) * group.alignment[cross];
            cursor += slot.size[axis] + group.spacing[axis];
        }
        return true;
    }

    bool Visible(const entt::registry& world, entt::entity entity)
    {
        const auto* flag = world.try_get<Engine::FlagComponent>(entity);
        return flag && flag->IsActive() && flag->IsVisible() &&
            !HasFlag(flag->flags, Engine::EEntityFlag::Dead);
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

bool Engine::UILayout::IsValid(const UITextComponent& text)
{
    if (text.fontPixels < UITextComponent::MinFontPixels || text.fontPixels > UITextComponent::MaxFontPixels ||
        text.text.size() > UITextComponent::MaxTextBytes ||
        (!text.multiline && !text.wordWrap && text.text.find_first_of("\r\n") != std::string::npos) ||
        text.fontFamily.size() > UITextComponent::MaxFontFamilyBytes ||
        text.fontFamily.find_first_of("\r\n") != std::string::npos || text.fontFamily.find('\0') != std::string::npos ||
        text.alignment < EUITextAlignment::Left || text.alignment > EUITextAlignment::Right ||
        text.verticalAlignment < EUITextVerticalAlignment::Top || text.verticalAlignment > EUITextVerticalAlignment::Bottom)
        return false;
    for (int channel = 0; channel < 4; ++channel)
        if (!std::isfinite(text.color[channel]) || text.color[channel] < 0.0f || text.color[channel] > 1.0f)
            return false;
    return true;
}

bool Engine::UILayout::IsValid(const UILayoutGroupComponent& group)
{
    if (group.mode != EUILayoutGroupMode::Row && group.mode != EUILayoutGroupMode::Column &&
        group.mode != EUILayoutGroupMode::Grid) return false;
    const auto padding = group.padding;
    const auto inRange = [](float value, float minimum, float maximum) {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    };
    return inRange(padding.x, 0.0f, 8192.0f) && inRange(padding.y, 0.0f, 8192.0f) &&
        inRange(padding.z, 0.0f, 8192.0f) && inRange(padding.w, 0.0f, 8192.0f) &&
        inRange(group.spacing.x, 0.0f, 8192.0f) && inRange(group.spacing.y, 0.0f, 8192.0f) &&
        inRange(group.alignment.x, 0.0f, 1.0f) && inRange(group.alignment.y, 0.0f, 1.0f) &&
        (group.mode != EUILayoutGroupMode::Grid ||
            (group.columns >= 1 && group.columns <= 256 &&
                inRange(group.cellSize.x, 1.0f, 8192.0f) && inRange(group.cellSize.y, 1.0f, 8192.0f)));
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
    const auto* order = world.valid(entity) ? TryGetEnabledComponent<UIRenderOrderComponent>(world, entity) : nullptr;
    return order ? order->zIndex : 0;
}

bool Engine::UILayout::Resolve(const RectTransformComponent& rect, const glm::vec2& parentSize,
    const glm::vec2& parentPivot, const glm::mat4& parentTransform, UIElementLayout& result)
{
    if (!IsValid(rect) || !Finite(parentSize) || !Finite(parentPivot) || !Finite(parentTransform) ||
        glm::any(glm::lessThan(parentSize, glm::vec2(0.0f)))) return false;
    UIElementLayout candidate;
    candidate.size = ResolveSize(rect, parentSize);
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

bool Engine::UILayout::Build(const entt::registry& world, entt::entity canvas, std::vector<UIElementLayout>& result,
    UILayoutBuildMode mode)
{
    const auto* root = world.valid(canvas) ? world.try_get<CanvasComponent>(canvas) : nullptr;
    if (!root || !IsValid(*root)) return false;
    const auto* rootRelation = world.try_get<HierarchyComponent>(canvas);
    if (!rootRelation || rootRelation->parent != entt::null) return false; // Nested canvases are a later stage.
    std::vector<UIElementLayout> candidate;
    std::unordered_set<entt::entity> visited{ canvas };
    const auto visitChildren = [&](auto&& self, entt::entity parent, const std::vector<entt::entity>& children,
        const glm::vec2& parentSize, const glm::vec2& parentPivot, const glm::mat4& parentMatrix,
        bool parentVisible, std::size_t depth) -> bool
    {
        if (children.empty()) return true;
        if (depth > 256) return false;
        std::vector<LayoutSlot> slots;
        const auto* group = TryGetEnabledComponent<UILayoutGroupComponent>(world, parent);
        if (group && IsValid(*group) && !BuildGroupSlots(world, *group, children, parentSize, slots)) return false;

        // Z-index drives draw order; layout slot indices stay in authored sibling order.
        std::vector<std::size_t> drawOrder(children.size());
        std::iota(drawOrder.begin(), drawOrder.end(), std::size_t{ 0 });
        std::stable_sort(drawOrder.begin(), drawOrder.end(), [&](auto left, auto right)
        {
            return GetZIndex(world, children[left]) < GetZIndex(world, children[right]);
        });
        for (const auto index : drawOrder)
        {
            const auto entity = children[index];
            if (!world.valid(entity) || !visited.insert(entity).second || world.all_of<CanvasComponent>(entity)) return false;
            const auto* rect = world.try_get<RectTransformComponent>(entity);
            const auto* relation = world.try_get<HierarchyComponent>(entity);
            if (!rect || !relation || relation->parent != parent) return false;

            RectTransformComponent resolvedRect = *rect;
            if (!slots.empty())
            {
                if (!IsValid(*rect)) return false;
                const auto& slot = slots[index];
                resolvedRect.anchorMin = resolvedRect.anchorMax = glm::vec2(0.0f);
                resolvedRect.sizeDelta = slot.size;
                resolvedRect.anchoredPosition = slot.origin + slot.size * rect->pivot;
            }
            UIElementLayout layout;
            if (!Resolve(resolvedRect, parentSize, parentPivot, parentMatrix, layout)) return false;
            layout.entity = entity;
            layout.managedByGroup = !slots.empty();
            layout.visible = parentVisible && IsComponentEnabled<RectTransformComponent>(world, entity) && Visible(world, entity);
            const auto layoutIndex = candidate.size();
            candidate.push_back(layout);
            if (!self(self, entity, relation->children, layout.size, rect->pivot, layout.worldTransform, layout.visible, depth + 1))
                return false;
            candidate[layoutIndex].subtreeEnd = candidate.size();
        }
        return true;
    };
    if (!visitChildren(visitChildren, canvas, rootRelation->children, root->referenceResolution, glm::vec2(0.0f), glm::mat4(1.0f),
        IsComponentEnabled<CanvasComponent>(world, canvas) && Visible(world, canvas), 2)) return false;
    if (mode == UILayoutBuildMode::Runtime) UIClipLayout::Apply(world, candidate);
    result.swap(candidate);
    return true;
}

bool Engine::UILayout::Contains(const UIElementLayout& layout, const glm::vec2& canvasPoint)
{
    const float determinant = glm::determinant(layout.renderMatrix);
    if (!layout.visible || !Finite(canvasPoint) || !std::isfinite(determinant) || determinant == 0.0f) return false;
    if (const auto& clip = layout.clipRect; clip && (canvasPoint.x < clip->x || canvasPoint.y < clip->y ||
        canvasPoint.x >= clip->z || canvasPoint.y >= clip->w)) return false;
    const glm::vec3 point(canvasPoint, 1.0f);
    for (std::size_t index = 0; index < layout.ellipseMaskCount; ++index)
    {
        const glm::vec2 localMask(glm::dot(glm::vec3(layout.ellipseMaskPlanes[index * 2]), point),
            glm::dot(glm::vec3(layout.ellipseMaskPlanes[index * 2 + 1]), point));
        if (!(glm::dot(localMask, localMask) <= 1.0f)) return false;
    }
    if (!UIClipLayout::AlphaMasksContain(layout, point)) return false;
    const glm::vec2 local(glm::inverse(layout.renderMatrix) * glm::vec4(canvasPoint, 0.0f, 1.0f));
    return Finite(local) && local.x >= 0.0f && local.y >= 0.0f && local.x <= 1.0f && local.y <= 1.0f;
}
