#pragma once

#include "Types.h"
#include "ComponentAnnotations.h"

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