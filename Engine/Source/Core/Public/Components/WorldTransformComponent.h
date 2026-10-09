#pragma once

#include "Reflection/ComponentAnnotations.h"

#include <glm/glm.hpp>

namespace Engine
{
    STRUCT(COMPONENT, TRANSIENT, DISPLAY_NAME("STRUCT_WORLD_TRANSFORM"))
    struct WorldTransformComponent
    {
        REFLECT_BODY()

        PROPERTY(READONLY, DISPLAY_NAME("PROP_WORLD_MATRIX"))
        glm::mat4 worldMatrix = glm::identity<glm::mat4>();
    };
}