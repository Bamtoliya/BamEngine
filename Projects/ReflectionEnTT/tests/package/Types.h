#pragma once
#include <reflection_entt/Annotations.h>
#define PACKAGE_NAME(value) META("DisplayName", value)

namespace package_example
{
    STRUCT(PACKAGE_NAME("Settings"))
    struct Settings
    {
        PROPERTY(PACKAGE_NAME("Speed"))
        float Speed = 1.0f;
    };
}
