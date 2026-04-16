#pragma once

#include "Archive.h"
#include "ReflectionTypes.h"

BEGIN(Engine)

class SerializationHelper
{
public:
    static void SerializeReflectionProperties(Archive& ar, const TypeInfo* typeInfo, void* instance);

    template<typename T>
    static void SerializeStaticType(Archive& ar, T& instance)
    {
        SerializeReflectionProperties(ar, &T::GetStaticTypeInfo(), &instance);
    }
};

END