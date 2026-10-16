#pragma once

#include "Reflection/ComponentAnnotations.h"
#include <glm/glm.hpp>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_RECT_TRANSFORM"), CATEGORY("CATEGORY_UI"))
    struct RectTransformComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ANCHOR_MIN"), RANGE(0.0, 1.0, 0.01), CATEGORY("CATEGORY_ANCHOR"))
        glm::vec2 anchorMin{ 0.5f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ANCHOR_MAX"), RANGE(0.0, 1.0, 0.01), CATEGORY("CATEGORY_ANCHOR"))
        glm::vec2 anchorMax{ 0.5f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ANCHOR_POSITION"), STEP(1.0), CATEGORY("CATEGORY_ANCHOR"))
        glm::vec2 anchoredPosition{ 0.0f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SIZE"), STEP(1.0))
        glm::vec2 sizeDelta{ 100.0f, 100.0f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PIVOT"), RANGE(0.0, 1.0, 0.01))
        glm::vec2 pivot{ 0.5f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SCALE"), STEP(0.01))
        glm::vec2 scale{ 1.0f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ROTATION"), STEP(1.0))
        f32 rotation = 0.0f;
    };
}
