#pragma once

#include "CameraBuffer.h"
#include "Light.h"

#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Engine
{
    // 아틀라스(4096) 사분면 하나의 해상도. DirectionalShadow 패스는 이 크기만 허용한다.
    constexpr f32 SHADOW_CASCADE_RESOLUTION = 2048.0f;

    // 방향광 하나의 캐스케이드 설정. 카메라와 무관하다.
    struct DirectionalShadowSettings
    {
        vec3 Position{ 0.0f };
        vec3 Direction{ 0.0f, -1.0f, 0.0f };
        f32 FallbackRange = 50.0f;
        f32 MaxDistance = 100.0f;
        f32 SplitLambda = 0.75f;
        vec4 ShadowParams{ 0.0f };
    };

    struct DirectionalShadowCascades
    {
        CameraBuffer Cameras[MAX_SHADOW_CASCADES]{};
        vec4 Splits{ 0.0f };
    };

    namespace detail
    {
        inline bool IsFiniteShadowMatrix(const mat4& matrix)
        {
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    if (!std::isfinite(matrix[column][row])) return false;
            return true;
        }

        // 카메라를 옮기거나 돌려도 크기가 바뀌지 않도록 뷰 공간에서 외접 구를 구한다.
        inline bool ComputeCascadeSphere(const CameraBuffer& camera, f32 splitNear, f32 splitFar, vec3& outCenter, f32& outRadius)
        {
            const mat4& projection = camera.projMatrix;
            if (!std::isfinite(projection[0][0]) || !std::isfinite(projection[1][1]) ||
                projection[0][0] == 0.0f || projection[1][1] == 0.0f) return false;

            const bool orthographic = projection[2][3] == 0.0f;
            const auto halfExtent = [&](f32 depth)
                {
                    const f32 scale = orthographic ? 1.0f : depth;
                    return vec2(std::abs(scale / projection[0][0]), std::abs(scale / projection[1][1]));
                };
            const vec2 nearExtent = halfExtent(splitNear);
            const vec2 farExtent = halfExtent(splitFar);
            const vec3 center(0.0f, 0.0f, (splitNear + splitFar) * 0.5f);
            f32 radius = std::max(glm::length(vec3(nearExtent, splitNear) - center), glm::length(vec3(farExtent, splitFar) - center));

            f32 scale = std::max({ glm::length(vec3(camera.invViewMatrix[0])), glm::length(vec3(camera.invViewMatrix[1])),
                glm::length(vec3(camera.invViewMatrix[2])) });
            if (std::abs(scale - 1.0f) < 0.001f) scale = 1.0f;

            outCenter = vec3(camera.invViewMatrix * vec4(center, 1.0f));
            outRadius = radius * scale;
            return std::isfinite(outCenter.x) && std::isfinite(outCenter.y) && std::isfinite(outCenter.z) &&
                std::isfinite(outRadius) && outRadius > 0.0f;
        }
    }

    // view의 절두체를 덮는 캐스케이드를 만든다. view가 없으면 광원 위치 주변을 덮는다.
    // 그림자 패스와 조명 패스가 같은 카메라로 호출하면 같은 결과를 얻는다.
    [[nodiscard]] inline bool BuildDirectionalShadowCascades(const DirectionalShadowSettings& settings, const CameraBuffer* view,
        DirectionalShadowCascades& outCascades)
    {
        const f32 directionLength = glm::length(settings.Direction);
        if (!std::isfinite(directionLength) || directionLength <= 0.000001f || !std::isfinite(settings.MaxDistance) ||
            !std::isfinite(settings.SplitLambda) || !std::isfinite(settings.FallbackRange) || settings.FallbackRange <= 0.0f) return false;

        const vec3 direction = settings.Direction / directionLength;
        vec3 up(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(direction, up)) > 0.999f) up = vec3(1.0f, 0.0f, 0.0f);

        const f32 nearZ = 0.1f;
        const f32 maxDistance = glm::clamp(settings.MaxDistance, 20.0f, 500.0f);
        const f32 exponent = glm::mix(1.0f, 1.6f, glm::clamp(settings.SplitLambda, 0.1f, 1.0f));
        f32 splits[MAX_SHADOW_CASCADES + 1]{ nearZ };
        for (uint32 i = 1; i <= MAX_SHADOW_CASCADES; ++i)
        {
            const f32 ratio = static_cast<f32>(i) / static_cast<f32>(MAX_SHADOW_CASCADES);
            splits[i] = nearZ + (maxDistance - nearZ) * std::pow(ratio, exponent);
        }

        DirectionalShadowCascades cascades;
        cascades.Splits = vec4(splits[1], splits[2], splits[3], 0.0f);

        // 카메라 위치와 무관한 광원 축. 이 축의 텍셀 격자에 중심을 맞춰야 카메라를 옮겨도 그림자 가장자리가 떨리지 않는다.
        const mat4 lightAxes = glm::lookAtLH(vec3(0.0f), direction, up);
        const mat4 lightAxesInverse = glm::inverse(lightAxes);

        for (uint32 c = 0; c < MAX_SHADOW_CASCADES; ++c)
        {
            vec3 center = settings.Position;
            f32 radius = settings.FallbackRange * (static_cast<f32>(c + 1) / static_cast<f32>(MAX_SHADOW_CASCADES)) * std::sqrt(3.0f);
            if (view && !detail::ComputeCascadeSphere(*view, splits[c], splits[c + 1], center, radius)) return false;
            radius = std::ceil(radius * 16.0f) / 16.0f;

            const f32 texel = (radius * 2.0f) / SHADOW_CASCADE_RESOLUTION;
            vec3 centerLS = vec3(lightAxes * vec4(center, 1.0f));
            centerLS.x = std::floor(centerLS.x / texel) * texel;
            centerLS.y = std::floor(centerLS.y / texel) * texel;
            center = vec3(lightAxesInverse * vec4(centerLS, 1.0f));

            const vec3 eye = center - direction * radius;
            const mat4 shadowView = glm::lookAtLH(eye, center, up);
            const mat4 shadowProjection = glm::orthoLH_ZO(-radius, radius, -radius, radius, 0.0f, radius * 2.0f);
            cascades.Cameras[c] = CameraBuffer::FromMatrices(shadowView, shadowProjection, eye, 0.0f);
            if (!detail::IsFiniteShadowMatrix(cascades.Cameras[c].viewProjMatrix)) return false;
        }

        outCascades = cascades;
        return true;
    }
}
