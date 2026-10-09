#pragma once

#include <reflection/core/ContainerInfo.h>
#include <reflection/core/EnumInfo.h>
#include <reflection/core/FunctionInfo.h>
#include <reflection/core/Metadata.h>
#include <reflection/core/PropertyInfo.h>
#include <reflection/core/QualifiedName.h>
#include <reflection/core/TypeInfo.h>
#include <reflection/runtime/ContainerAccessor.h>

namespace Engine
{
    using EPropertyType = legacy_reflection::EPropertyType;
    using VariableInfo = legacy_reflection::VariableInfo;
    using ContainerInfo = legacy_reflection::ContainerInfo;
    using ContainerAccessor = legacy_reflection::ContainerAccessor;

    using MetadataValue = legacy_reflection::MetadataValue;
    using MetadataEntry = legacy_reflection::MetadataEntry;

    using EnumEntry = legacy_reflection::EnumEntry;
    using EnumInfo = legacy_reflection::EnumInfo;

    using FunctionInfo = legacy_reflection::FunctionInfo;
    using PropertyInfo = legacy_reflection::PropertyInfo;
    using TypeInfo = legacy_reflection::TypeInfo;
}