#pragma once

#include "Types.h"
#include <reflection/Annotations.h>
#include <glm/glm.hpp>
#include <array>
#include <string>

namespace Engine
{
    STRUCT(DISPLAY_NAME("STRUCT_PHYSICS_SETTINGS"), CATEGORY("CATEGORY_PHYSICS"))
    struct PhysicsSettings
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_GRAVITY_2D"))
        vec2 Gravity2D = vec2(0.f, -9.81f);

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_GRAVITY_3D"))
        vec3 Gravity3D = vec3(0.f, -9.81f, 0.f);

        PROPERTY(EDITABLE, RANGE(0.001, 0.1), DISPLAY_NAME("PROP_FIXED_TIMESTEP"))
        f32 FixedTimeStep = 0.02f;

        PROPERTY(EDITABLE, RANGE(1, 32), DISPLAY_NAME("PROP_POSITION_ITERATIONS"))
        int32 PositionIterations = 8;

        PROPERTY(EDITABLE, RANGE(1, 16), DISPLAY_NAME("PROP_VELOCITY_ITERATIONS"))
        int32 VelocityIterations = 1;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LAYER_NAMES"))
        std::array<std::string, 32> LayerNames = {
            "Default", "Player", "Enemy", "Terrain", "Projectile", "Trigger", "Vehicle", "Water"
        };
    };
}
