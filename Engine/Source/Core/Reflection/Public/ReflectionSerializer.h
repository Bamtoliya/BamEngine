#pragma once

#include "Archive.h"
#include "SerializableInterface.h"
#include "ReflectionTypes.h"

BEGIN(Engine)
class ReflectionSerializer
{
public:
	static void Serialize(Archive& ar, const void* instance, const TypeInfo& typeInfo);
	static void Deserialize(Archive& ar, void* instance, const TypeInfo& typeInfo);
private:
	static void ProcessProperty(Archive& ar, const void* instance, const PropertyInfo& propInfo);
	static void ProcessContainer(Archive& ar, const void* instance, const PropertyInfo& propInfo);
};
END