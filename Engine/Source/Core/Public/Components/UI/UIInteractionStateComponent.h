#pragma once

#include "Reflection/ComponentAnnotations.h"

namespace Engine
{
    // Owned by one UIEventSystem per canvas. Never authored or saved with the scene.
    STRUCT(COMPONENT, TRANSIENT, HIDDEN, CATEGORY("CATEGORY_UI"), DISPLAY_NAME("STRUCT_UI_INTERACTION_STATE"))
    struct UIInteractionStateComponent
    {
        REFLECT_BODY()

        bool hovered = false;
        bool pressed = false;
    };
}
