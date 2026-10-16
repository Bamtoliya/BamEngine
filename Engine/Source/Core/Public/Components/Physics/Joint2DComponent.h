#pragma once

#include "Reflection/ComponentAnnotations.h"

#include "Types.h"

#include <entt/entity/entity.hpp>

namespace Engine
{
    // 두 앵커 사이 거리를 유지하는 2D Distance Joint.
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_JOINT_2D"), CATEGORY("CATEGORY_PHYSICS_2D"))
    struct Joint2DComponent
    {
        REFLECT_BODY()
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
        bool Enabled = true;
        // Raw entity references require a scene reference codec; serialization must fail until then.
        PROPERTY(READONLY, DISPLAY_NAME("PROP_CONNECTED_ENTITY"))
        entt::entity ConnectedEntity = entt::null;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOCAL_ANCHOR"))
        vec2 LocalAnchor = vec2(0.f);
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CONNECTED_LOCAL_ANCHOR"))
        vec2 ConnectedLocalAnchor = vec2(0.f);

        PROPERTY(EDITABLE, RANGE(0.0, 1.0e12), DISPLAY_NAME("PROP_REST_LENGTH"))
        f32 RestLength = 1.f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLE_COLLISION"))
        bool EnableCollision = false;
    };
}
