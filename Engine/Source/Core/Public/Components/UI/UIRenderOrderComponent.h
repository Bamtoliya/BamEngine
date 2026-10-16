#pragma once

#include "Reflection/ComponentAnnotations.h"
#include <cstdint>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_RENDER_ORDER"), CATEGORY("CATEGORY_UI"))
    struct UIRenderOrderComponent
    {
        REFLECT_BODY()

        // Sibling groups are painted back-to-front. Root canvases use the same rule within a scene.
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_Z_INDEX"), STEP(1.0))
        std::int32_t zIndex = 0;
    };
}
