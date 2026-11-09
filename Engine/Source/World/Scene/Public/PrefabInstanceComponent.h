#pragma once

#include "ComponentAnnotations.h"
#include <string>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_PREFAB_INSTANCE"), CATEGORY("CATEGORY_CORE"))
        struct PrefabInstanceComponent
    {
        REFLECT_BODY()

            PROPERTY(READONLY, DISPLAY_NAME("PROP_PREFAB_PATH"))
            std::wstring prefabAssetPath;
    };
}