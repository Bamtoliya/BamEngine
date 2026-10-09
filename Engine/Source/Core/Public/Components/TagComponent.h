#pragma once

#include <reflection/Annotations.h>

#include <string>
#include <unordered_set>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_TAG"), CATEGORY("Core"))
    struct TagComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TAGS"))
        std::unordered_set<std::wstring> tags;
    };
}