#pragma once

#include "Types.h"
#include "Reflection/ComponentAnnotations.h"

namespace Engine
{
    ENUM()
    enum class ERigidBodyType : uint8
    {
        Static,
        Dynamic,
        Kinematic
    };
}