#pragma once

#include "Types.h"

#include <entt/entity/entity.hpp>

namespace Engine
{
    // 두 앵커 사이 거리를 유지하는 2D Distance Joint.
    struct Joint2DComponent
    {
        bool Enabled = true;
        entt::entity ConnectedEntity = entt::null;

        vec2 LocalAnchor = vec2(0.f);
        vec2 ConnectedLocalAnchor = vec2(0.f);

        f32 RestLength = 1.f;
        bool EnableCollision = false;
    };
}