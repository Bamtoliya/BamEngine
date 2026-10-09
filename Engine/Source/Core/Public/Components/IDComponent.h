#pragma once

#include "Reflection/ComponentAnnotations.h"

#include <cstdint>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_ID"), CATEGORY("Core"))
    struct IDComponent
    {
        REFLECT_BODY()

        PROPERTY(READONLY, DISPLAY_NAME("PROP_ID"))
        std::uint64_t id = 0;
    };
}