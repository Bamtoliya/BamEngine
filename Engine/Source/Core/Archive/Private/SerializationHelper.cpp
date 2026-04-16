#include "SerializationHelper.h"

#include <BamReflectionSerialization.h>

BEGIN(Engine)

void SerializationHelper::SerializeReflectionProperties(Archive& ar, const TypeInfo* typeInfo, void* instance)
{
    if (!typeInfo || !instance)
    {
        return;
    }

    bam_reflection::ReflectionSerialization::SerializeProperties(ar, instance, *typeInfo);
}

END