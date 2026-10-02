#pragma once

#include "ReflectionUnit.h"

#include <span>
#include <string>
#include <string_view>

namespace reflection_codegen
{
    std::string EmitRegistrationCpp(
        const ReflectionUnit& unit,
        std::string_view moduleName,
        std::span<const std::string> headerPaths);
}