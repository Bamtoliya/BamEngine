#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "ResourceHandle.h"
#include <glm/glm.hpp>

namespace Engine
{
    class Sprite;
    class MaterialInterface;

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_IMAGE"), CATEGORY("CATEGORY_UI"))
    struct UIImageComponent
    {
        REFLECT_BODY()

        // An empty Sprite renders a solid rectangle using the engine's white texture.
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPRITE_HANDLE"))
        ResourceHandle<Sprite> spriteHandle;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
        ResourceHandle<MaterialInterface> materialHandle;
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_COLOR"))
        glm::vec4 color{ 1.0f };
        // Optional preserves scenes written before pointer interaction was introduced.
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_RAYCAST_TARGET"))
        bool raycastTarget = true;
    };
}
