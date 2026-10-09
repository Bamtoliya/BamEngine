#pragma once

#include "Reflection/ComponentAnnotations.h"
#include <string>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_NAME"), CATEGORY("Core"))
    struct NameComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NAME"))
        std::wstring name = L"Entity";
    };
}