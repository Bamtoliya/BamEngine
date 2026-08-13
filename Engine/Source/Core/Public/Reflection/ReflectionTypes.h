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
    using EPropertyType = reflection::EPropertyType;
    using VariableInfo = reflection::VariableInfo;
    using ContainerInfo = reflection::ContainerInfo;
    using ContainerAccessor = reflection::ContainerAccessor;

    using MetadataValue = reflection::MetadataValue;
    using MetadataEntry = reflection::MetadataEntry;

    using EnumEntry = reflection::EnumEntry;
    using EnumInfo = reflection::EnumInfo;

    using FunctionInfo = reflection::FunctionInfo;
    using PropertyInfo = reflection::PropertyInfo;
    using TypeInfo = reflection::TypeInfo;
}