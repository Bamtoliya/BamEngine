#pragma once

#include <reflection/Annotations.h>

#define PACKAGE_NAME(value) META("DisplayName", value)

STRUCT(PACKAGE_NAME("Package Settings"))
struct PackageSettings
{
    PROPERTY(PACKAGE_NAME("Speed"))
        float Speed = 1.0f;
};