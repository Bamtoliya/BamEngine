#pragma once

#include "Reflection/ComponentAnnotations.h"
#include <glm/glm.hpp>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_CANVAS"), CATEGORY("CATEGORY_UI"))
    struct CanvasComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RESOLUTION"), STEP(1.0))
        glm::vec2 referenceResolution{ 1920.0f, 1080.0f };
    };
}
