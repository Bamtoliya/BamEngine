#pragma once 

#include "ReflectionTypes.h"

BEGIN(Engine)

class PropertyAccessor
{
public:
	static void* GetValuePtr(void* instance, const PropertyInfo& propInfo)
	{
		if (!instance) return nullptr;
		return static_cast<void*>(static_cast<char*>(instance) + propInfo.Offset);
	}
	static const void* GetValuePtr(const void* instance, const PropertyInfo& propInfo)
	{
		if (!instance) return nullptr;
		return static_cast<const void*>(static_cast<const char*>(instance) + propInfo.Offset);
	}

	template<typename T>
	static T GetValue(void* instance, const PropertyInfo& propInfo)
	{
		return *static_cast<T*>(GetValuePtr(instance, propInfo));
	}

	template<typename T>
	static void SetValue(void* instance, const PropertyInfo& propInfo, const T& value)
	{
		*static_cast<T*>(GetValuePtr(instance, propInfo)) = value;
	}
};
END