#pragma once

#include "Engine_API.h"
#include "Reflection/ComponentReflectionBindings.h"

namespace Engine
{
    [[nodiscard]]
    ENGINE_API const reflection::Registry* GetCoreComponentReflectionRegistry();

    [[nodiscard]]
    ENGINE_API const ComponentReflectionBindings* GetCoreComponentReflectionBindings();
}