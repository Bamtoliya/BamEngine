#pragma once

#include "ReflectionUnit.h"

#include <span>
#include <string>
#include <string_view>

namespace reflection_codegen
{
    std::string EmitRegistrationHeader(const ReflectionUnit& unit, std::string_view moduleName, std::span<const std::string> headerPaths, std::string_view typeListMetadata = {});
    std::string EmitRegistrationCpp(const ReflectionUnit& unit, std::string_view moduleName, std::span<const std::string> headerPaths, std::string_view generatedHeaderName);
}