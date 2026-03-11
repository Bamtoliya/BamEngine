#pragma once

#include "ReflectionSerializer.h"

void ReflectionSerializer::Serialize(Archive& ar, const void* instance, const TypeInfo& typeInfo)
{
}

void ReflectionSerializer::Deserialize(Archive& ar, void* instance, const TypeInfo& typeInfo)
{
}

void ReflectionSerializer::ProcessProperty(Archive& ar, const void* instance, const PropertyInfo& propInfo)
{
}

void ReflectionSerializer::ProcessContainer(Archive& ar, const void* instance, const PropertyInfo& propInfo)
{
}
