#pragma once

#include "Reflection/ComponentAnnotations.h"

#include "Interface/EnumBit.h"

#include <cstdint>

namespace Engine
{
    ENUM(BITMASK)
    enum class EEntityFlag : std::uint8_t
    {
        None = 0,
        Active = 1 << 0,
        Visible = 1 << 1,
        Paused = 1 << 2,
        Dead = 1 << 3,
        Default = Active | Visible
    };

    ENABLE_BITMASK_OPERATORS(EEntityFlag)

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_FLAG"), CATEGORY("Core"))
    struct FlagComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FLAG"))
        EEntityFlag flags = EEntityFlag::Default;
    };
}