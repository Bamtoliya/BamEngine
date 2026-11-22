#pragma once

#include "BoundingBox.h"
#include "Types.h"

#include <algorithm>
#include <cmath>

namespace Engine
{
    struct StreamingView
    {
        vec3 CameraPosition{ 0.0f };
        vec3 Forward{ 0.0f, 0.0f, 1.0f };
        f32 ProjectionScaleY = 1.0f;
        f32 ViewHeight = 1080.0f;
        bool Orthographic = false;
    };

    // 카메라에서 경계 상자의 가장 가까운 점까지의 거리. 안에 있으면 0.
    [[nodiscard]] inline f32 ComputeBoundsDistance(const vec3& position, const BoundingBox& bounds)
    {
        const vec3 nearest = glm::clamp(position, bounds.min, bounds.max);
        return glm::length(position - nearest);
    }

    // UV 1만큼이 화면에서 차지하는 픽셀 수. 알 수 없으면 0이며 가장 자세한 밉을 뜻한다.
    // 경계 상자의 가장 가까운 점을 기준으로 하므로 실제보다 자세한 쪽으로 어긋난다.
    [[nodiscard]] inline f32 ComputePixelsPerUV(const StreamingView& view, const BoundingBox& worldBounds, f32 worldScale, f32 uvDensity)
    {
        if (!(uvDensity > 0.0f) || !(worldScale > 0.0f) || !std::isfinite(uvDensity) || !std::isfinite(worldScale)) return 0.0f;
        constexpr f32 MinDistance = 0.05f;
        const f32 distance = view.Orthographic ? 1.0f : (std::max)(ComputeBoundsDistance(view.CameraPosition, worldBounds), MinDistance);
        const f32 pixelsPerUnit = 0.5f * view.ViewHeight * view.ProjectionScaleY / distance;
        return pixelsPerUnit * worldScale / uvDensity;
    }

    // 화면 비중이 클수록 먼저 읽는다. 카메라 뒤에 있는 것은 그림자에만 쓰이므로 뒤로 미룬다.
    [[nodiscard]] inline f32 ComputeStreamingPriority(const StreamingView& view, const BoundingBox& worldBounds, f32 coverage)
    {
        const vec3 center = worldBounds.Center();
        const f32 radius = glm::length(worldBounds.Extent());
        const bool behind = glm::dot(center - view.CameraPosition, view.Forward) < -radius;
        return behind ? coverage * 0.25f : coverage;
    }
}
