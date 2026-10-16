#pragma once

#include "Reflection/ComponentAnnotations.h"
#include <glm/glm.hpp>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_BUTTON"), CATEGORY("CATEGORY_UI"))
    struct UIButtonComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INTERACTABLE"))
        bool interactable = true;
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_NORMAL_COLOR"))
        glm::vec4 normalColor{ 1.0f };
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_HOVER_COLOR"))
        glm::vec4 hoveredColor{ 0.9f, 0.9f, 0.9f, 1.0f };
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_PRESS_COLOR"))
        glm::vec4 pressedColor{ 0.7f, 0.7f, 0.7f, 1.0f };
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_DISABLE_COLOR"))
        glm::vec4 disabledColor{ 0.5f, 0.5f, 0.5f, 0.5f };
    };
}
