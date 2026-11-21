#include "UIClipLayout.h"
#include "UIClipComponent.h"
#include "UIEllipseMaskComponent.h"
#include "UIScrollComponent.h"
#include "ComponentEnable.h"
#include "WorldComponents.h"
#include "Sprite.h"
#include "Texture.h"
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
        const auto contentEntity = Engine::UIClipLayout::FindScrollContent(world, viewport.entity);
        const auto* rect = world.try_get<Engine::RectTransformComponent>(viewport.entity);
        if (!rect || contentEntity == entt::null) return 0.0f;
        const auto* content = world.try_get<Engine::RectTransformComponent>(contentEntity);
        UIElementLayout contentLayout;
        if (!Engine::UILayout::Resolve(*content, viewport.size, rect->pivot, viewport.worldTransform, contentLayout))
            return 0.0f;
        return std::max(0.0f, contentLayout.size.y - viewport.size.y);
    }

    bool HasReadableAlpha(const Engine::Texture& texture)
    {
        const auto& header = texture.GetCachedHeader();
        if (header.dimension != ETextureDimension::Texture2D || header.arraySize != 1 || header.depth != 1 ||
            header.mipLevels == 0 || header.width == 0 || header.height == 0) return false;
        if (header.format != ETextureFormat::R8G8B8A8_UNORM && header.format != ETextureFormat::R8G8B8A8_UNORM_SRGB &&
            header.format != ETextureFormat::B8G8R8A8_UNORM && header.format != ETextureFormat::B8G8R8A8_UNORM_SRGB) return false;
        const auto* rhi = texture.GetRHITexture();
        return (!rhi || (rhi->GetWidth() == header.width && rhi->GetHeight() == header.height &&
            rhi->GetFormat() == header.format)) && texture.GetTempData().size() / 4 / header.height >= header.width;
    }

    bool IsFinite(const glm::vec4& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
    }

    std::optional<Engine::UIAlphaMaskRegion> ResolveAlphaMask(const Engine::UIAlphaMaskComponent& component, const glm::mat4& matrix)
    {
        Engine::UIAlphaMaskRegion region;
        glm::vec4 spritePixels(0.0f);
        if (component.spriteHandle.GetRawHandle().IsValid())
        {
            const auto* sprite = component.spriteHandle.Get();
            if (!sprite) return {};
            region.texture = sprite->GetTextureHandle();
            spritePixels = sprite->GetRegionUV();
        }
        else
        {
            region.texture = component.textureHandle;
        }
        const auto* texture = region.texture.Get();
        if (!texture || !HasReadableAlpha(*texture) || !IsFinite(spritePixels)) return {};
        const auto& header = texture->GetCachedHeader();
        if (spritePixels.z != 0.0f || spritePixels.w != 0.0f)
        {
            const glm::vec2 size(header.width, header.height);
            const glm::vec2 start(spritePixels), span(spritePixels.z, spritePixels.w);
            if (start.x < 0.0f || start.y < 0.0f || span.x <= 0.0f || span.y <= 0.0f ||
                glm::any(glm::greaterThan(start + span, size))) return {};
            region.uvTransform = { start.x / size.x, start.y / size.y, span.x / size.x, span.y / size.y };
        }

        const float determinant = glm::determinant(matrix);
        if (!std::isfinite(determinant) || determinant == 0.0f) return {};
        const auto inverse = glm::inverse(matrix);
        region.xPlane = { inverse[0][0], inverse[1][0], inverse[3][0], 0.0f };
        region.yPlane = { inverse[0][1], inverse[1][1], inverse[3][1], 0.0f };
        if (!IsFinite(region.xPlane) || !IsFinite(region.yPlane)) return {};
        return region;
    }

    float SampleAlpha(const Engine::UIAlphaMaskRegion& mask, const glm::vec3& point)
    {
        const glm::vec2 local(glm::dot(glm::vec3(mask.xPlane), point), glm::dot(glm::vec3(mask.yPlane), point));
        if (!(local.x >= 0.0f && local.y >= 0.0f && local.x <= 1.0f && local.y <= 1.0f)) return 0.0f;

        const auto* texture = mask.texture.Get();
        if (!texture || !HasReadableAlpha(*texture)) return 0.0f;
        const auto& header = texture->GetCachedHeader();
        const glm::vec2 size(header.width, header.height);
        const auto bounds = Engine::UIAlphaMaskUVBounds(mask.uvTransform, size);
        const glm::vec2 uv = glm::vec2(mask.uvTransform) + local * glm::vec2(mask.uvTransform.z, mask.uvTransform.w);
        const glm::vec2 pixel = glm::clamp(glm::clamp(uv, glm::vec2(bounds), glm::vec2(bounds.z, bounds.w)) * size -
            glm::vec2(0.5f), glm::vec2(0.0f), size - 1.0f);
        const auto x = static_cast<std::size_t>(pixel.x), y = static_cast<std::size_t>(pixel.y);
        const auto nextX = std::min(x + 1, static_cast<std::size_t>(header.width - 1));
        const auto nextY = std::min(y + 1, static_cast<std::size_t>(header.height - 1));
        const auto& bytes = texture->GetTempData();
        const auto alpha = [&](std::size_t column, std::size_t row)
        {
            return bytes[(row * header.width + column) * 4 + 3] / 255.0f;
        };
        const float dx = pixel.x - static_cast<float>(x), dy = pixel.y - static_cast<float>(y);
        return std::lerp(std::lerp(alpha(x, y), alpha(nextX, y), dx),
            std::lerp(alpha(x, nextY), alpha(nextX, nextY), dx), dy);
    }

    struct ClippingState
    {
        std::optional<glm::vec4> rectangle;
        glm::vec2 displacement{ 0.0f };
        Engine::UIEllipseMaskPlanes ellipseMaskPlanes{};
        std::size_t ellipseMaskCount = 0;
        Engine::UIAlphaMaskRegions alphaMasks{};
        std::size_t alphaMaskCount = 0;
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
        layout.ellipseMaskPlanes = state.ellipseMaskPlanes;
        layout.ellipseMaskCount = state.ellipseMaskCount;
        layout.alphaMasks = state.alphaMasks;
        layout.alphaMaskCount = state.alphaMaskCount;
        if (layout.visible && state.rectangle)
            layout.visible = NonEmpty(Intersect(*state.rectangle, Bounds(layout.renderMatrix)));

        const auto* scroll = TryGetEnabledComponent<UIScrollComponent>(world, layout.entity);
        const bool clips = world.all_of<UIClipComponent>(layout.entity) && IsComponentEnabled<UIClipComponent>(world, layout.entity);
        const bool ellipse = world.all_of<UIEllipseMaskComponent>(layout.entity) &&
            IsComponentEnabled<UIEllipseMaskComponent>(world, layout.entity);
        if (scroll || clips || ellipse)
        {
            const float determinant = glm::determinant(layout.renderMatrix);
            const bool validEllipse = ellipse && std::isfinite(determinant) && determinant != 0.0f;
            // A rectangular clip needs aligned axes; a single ellipse clip accepts a rotated transform.
            const bool validBounds = scroll || clips ? AxisAligned(layout.renderMatrix) : validEllipse;
            const auto bounds = validBounds ? Bounds(layout.renderMatrix) : glm::vec4(0.0f);
            state.rectangle = state.rectangle ? Intersect(*state.rectangle, bounds) : bounds;

            // Additional nesting keeps the cheap rectangular bound without exceeding shader capacity.
            if (validEllipse && NonEmpty(*state.rectangle) && state.ellipseMaskCount < MaxUIEllipseMasks)
            {
                const auto inverse = glm::inverse(layout.renderMatrix);
                const auto row = [&](int axis)
                {
                    return glm::vec4(2.0f * inverse[0][axis], 2.0f * inverse[1][axis],
                        2.0f * inverse[3][axis] - 1.0f, 0.0f);
                };
                const auto rowOffset = state.ellipseMaskCount * 2;
                const auto xPlane = row(0);
                const auto yPlane = row(1);
                const auto finite = [](const glm::vec4& plane)
                {
                    return std::isfinite(plane.x) && std::isfinite(plane.y) && std::isfinite(plane.z);
                };
                if (finite(xPlane) && finite(yPlane))
                {
                    state.ellipseMaskPlanes[rowOffset] = xPlane;
                    state.ellipseMaskPlanes[rowOffset + 1] = yPlane;
                    ++state.ellipseMaskCount;
                }
                else
                {
                    state.rectangle = glm::vec4(0.0f);
                }
            }
            if (scroll && std::isfinite(scroll->offset))
            {
                const float offset = std::clamp(scroll->offset, 0.0f, ScrollRange(world, layout));
                state.displacement -= glm::vec2(layout.worldTransform[1]) * offset;
            }
        }
        if (const auto* alphaMask = TryGetEnabledComponent<UIAlphaMaskComponent>(world, layout.entity))
        {
            const auto region = state.alphaMaskCount < MaxUIAlphaMasks ? ResolveAlphaMask(*alphaMask, layout.renderMatrix)
                : std::nullopt;
            const auto bounds = region ? Bounds(layout.renderMatrix) : glm::vec4(0.0f);
            state.rectangle = state.rectangle ? Intersect(*state.rectangle, bounds) : bounds;
            if (region && NonEmpty(*state.rectangle)) state.alphaMasks[state.alphaMaskCount++] = *region;
        }
        if (state.rectangle || state.displacement != glm::vec2(0.0f))
            states.emplace(layout.entity, state);
    }
}

bool Engine::UIClipLayout::AlphaMasksContain(const UIElementLayout& layout, const glm::vec3& point)
{
    float coverage = 1.0f;
    for (std::size_t index = 0; index < layout.alphaMaskCount; ++index)
    {
        coverage *= SampleAlpha(layout.alphaMasks[index], point);
        if (coverage < 0.5f) return false;
    }
    return true;
}

entt::entity Engine::UIClipLayout::FindScrollContent(const entt::registry& world, entt::entity viewport)
{
    const auto* relation = world.valid(viewport) ? world.try_get<HierarchyComponent>(viewport) : nullptr;
    if (!relation || relation->children.size() != 1) return entt::null;
    const auto content = relation->children.front();
    if (!world.valid(content)) return entt::null;
    const auto* contentRelation = world.try_get<HierarchyComponent>(content);
    return contentRelation && contentRelation->parent == viewport &&
        TryGetEnabledComponent<RectTransformComponent>(world, content) ? content : entt::null;
}

std::optional<Engine::UIScrollbarLayout> Engine::UIClipLayout::GetScrollbar(
    const entt::registry& world, const UIElementLayout& viewport)
{
    if (!viewport.visible || viewport.size.x <= 0.0f || viewport.size.y <= 0.0f) return {};
    const auto* scroll = TryGetEnabledComponent<UIScrollComponent>(world, viewport.entity);
    if (!scroll || !scroll->showScrollbar) return {};
    const float range = ScrollRange(world, viewport);
    if (!(range > 0.0f)) return {};

    const float width = std::min(12.0f / viewport.size.x, 0.25f);
    const float ratio = 1.0f / (1.0f + range / viewport.size.y);
    const float height = std::min(std::max(ratio, 18.0f / viewport.size.y), 0.95f);
    const float offset = std::isfinite(scroll->offset) ? std::clamp(scroll->offset, 0.0f, range) : 0.0f;
    return UIScrollbarLayout{ width, (offset / range) * (1.0f - height), height, range };
}

std::vector<Engine::UIScrollbarOverlay> Engine::UIClipLayout::GetScrollbarOverlays(
    const entt::registry& world, const std::vector<UIElementLayout>& layouts)
{
    std::vector<UIScrollbarOverlay> overlays;
    for (std::size_t index = 0; index < layouts.size(); ++index)
    {
        if (const auto geometry = GetScrollbar(world, layouts[index])) overlays.push_back({ index, *geometry });
    }
    // Draw parent scrollbars over their descendants and before later siblings.
    std::ranges::sort(overlays, [&](const UIScrollbarOverlay& a, const UIScrollbarOverlay& b)
    {
        const auto aEnd = layouts[a.index].subtreeEnd;
        const auto bEnd = layouts[b.index].subtreeEnd;
        return aEnd != bEnd ? aEnd < bEnd : a.index > b.index;
    });
    return overlays;
}

bool Engine::UIClipLayout::IsScrollbarHit(
    const UIElementLayout& viewport, const UIScrollbarLayout& scrollbar, const glm::vec2& point)
{
    return UILayout::Contains(viewport, point) && ToScrollbarLocal(viewport, point).x >= 1.0f - scrollbar.width;
}

glm::vec2 Engine::UIClipLayout::ToScrollbarLocal(const UIElementLayout& viewport, const glm::vec2& point)
{
    return glm::vec2(glm::inverse(viewport.renderMatrix) * glm::vec4(point, 0.0f, 1.0f));
}

bool Engine::UIClipLayout::ScrollAt(entt::registry& world, const std::vector<UIElementLayout>& layouts,
    const glm::vec2& canvasPosition, float wheelDelta)
{
    if (!std::isfinite(wheelDelta) || wheelDelta == 0.0f) return false;
    const auto overlays = GetScrollbarOverlays(world, layouts);
    auto overlay = overlays.rbegin();
    for (auto index = layouts.size(); index > 0; --index)
    {
        const UIElementLayout* hit = nullptr;
        while (overlay != overlays.rend() && layouts[overlay->index].subtreeEnd == index)
        {
            const auto& viewport = layouts[overlay->index];
            if (!hit && IsScrollbarHit(viewport, overlay->geometry, canvasPosition)) hit = &viewport;
            ++overlay;
        }
        const bool scrollbarHit = hit != nullptr;
        if (!hit && UILayout::Contains(layouts[index - 1], canvasPosition)) hit = &layouts[index - 1];
        if (!hit) continue;

        bool consumed = false;
        const UIElementLayout* current = hit;
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
        const auto* image = TryGetEnabledComponent<UIImageComponent>(world, hit->entity);
        if (scrollbarHit || (image && image->raycastTarget)) return false;
    }
    return false;
}
