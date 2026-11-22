#pragma once

#include "RaytracedShadowSettings.h"
#include "Types.h"

#include <algorithm>
#include <array>
#include <vector>

namespace Engine
{
    constexpr uint32 INVALID_SHADOW_LIGHT = 0xFFFFFFFFu;
    // 마스크(RGBA8)의 채널 수. raytracedShadowMaskRTCS·lightingPS와 같아야 한다.
    constexpr uint32 MAX_RAYTRACED_SHADOW_LIGHTS = 4;

    struct ShadowLight
    {
        uint32 LightIndex = INVALID_SHADOW_LIGHT;
        bool RaytracedRequested = false;
    };

    // 광원 버퍼 순서로 모은 그림자 광원과, 섀도맵을 가진 광원의 버퍼 인덱스.
    struct ShadowLightSet
    {
        std::vector<ShadowLight> Lights;
        uint32 CascadeLight = INVALID_SHADOW_LIGHT;
        uint32 SpotLight = INVALID_SHADOW_LIGHT;
        uint32 PointLight = INVALID_SHADOW_LIGHT;

        [[nodiscard]] bool HasShadowMap(uint32 lightIndex) const
        {
            return lightIndex == CascadeLight || lightIndex == SpotLight || lightIndex == PointLight;
        }
    };

    // 마스크 채널마다 추적할 광원의 버퍼 인덱스. 빈 채널은 INVALID_SHADOW_LIGHT.
    struct RaytracedShadowAssignment
    {
        std::array<uint32, MAX_RAYTRACED_SHADOW_LIGHTS> Lights{ INVALID_SHADOW_LIGHT, INVALID_SHADOW_LIGHT, INVALID_SHADOW_LIGHT, INVALID_SHADOW_LIGHT };

        [[nodiscard]] bool Empty() const { return Lights[0] == INVALID_SHADOW_LIGHT; }
        [[nodiscard]] bool Contains(uint32 lightIndex) const
        {
            return lightIndex != INVALID_SHADOW_LIGHT && std::ranges::find(Lights, lightIndex) != Lights.end();
        }
    };

    // 채널이 모자라면 캐스케이드 방향광, 섀도맵이 없는 광원, 스포트·점광원 섀도맵을 가진 광원 순으로 배정한다.
    // 섀도맵이 없는 광원은 채널을 받지 못하면 그림자가 없다.
    [[nodiscard]] inline RaytracedShadowAssignment AssignRaytracedShadows(const ShadowLightSet& set, const RaytracedShadowOverrides& overrides)
    {
        RaytracedShadowAssignment assignment;
        uint32 channel = 0;
        const auto assign = [&](auto&& accepts)
            {
                for (const auto& light : set.Lights)
                {
                    if (channel >= MAX_RAYTRACED_SHADOW_LIGHTS) return;
                    if (accepts(light.LightIndex) && ResolveRaytracedShadow(light.RaytracedRequested, overrides))
                        assignment.Lights[channel++] = light.LightIndex;
                }
            };
        assign([&](uint32 index) { return index == set.CascadeLight; });
        assign([&](uint32 index) { return !set.HasShadowMap(index); });
        assign([&](uint32 index) { return index != set.CascadeLight && set.HasShadowMap(index); });
        return assignment;
    }
}
