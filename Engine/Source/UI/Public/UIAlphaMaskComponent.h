#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"

namespace Engine
{
    class Sprite;
    class Texture;

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_ALPHA_MASK"), CATEGORY("CATEGORY_UI"))
    struct UIAlphaMaskComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_MASK_TEXTURE"))
        ResourceHandle<Texture> textureHandle;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_MASK_SPRITE"))
        ResourceHandle<Sprite> spriteHandle;
    };
}
