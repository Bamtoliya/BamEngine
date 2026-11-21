#pragma once

#include "ComponentAnnotations.h"

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_SCROLL"), CATEGORY("CATEGORY_UI"))
    struct UIScrollComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_SCROLL_WHEEL_STEP"), RANGE(1, 512, 1))
        float wheelStep = 40.0f;

        float offset = 0.0f; // Transient canvas-local pixel position.
    };
}
