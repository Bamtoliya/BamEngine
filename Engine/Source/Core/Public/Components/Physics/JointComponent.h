#pragma once

#include "Types.h"

#include <entt/entity/entity.hpp>

namespace Engine
{
    // 두 앵커 사이 거리를 유지하는 Distance Joint.
    struct JointComponent
    {
        bool Enabled = true;
        entt::entity ConnectedEntity = entt::null;

        vec3 LocalAnchor = vec3(0.f);
        vec3 ConnectedLocalAnchor = vec3(0.f);

        f32 RestLength = 1.f;
        bool EnableCollision = false;
    };
}