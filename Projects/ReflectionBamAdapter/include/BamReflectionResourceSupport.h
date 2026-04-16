#pragma once

#include <reflection/core/ContainerInfo.h>

#include "ResourceHandle.h"

#include <type_traits>

namespace bam_reflection
{
    template<typename T>
    struct IsResourceHandle : std::false_type
    {
    };

    template<typename T>
    struct IsResourceHandle<ResourceHandle<T>> : std::true_type
    {
    };

    template<typename T>
    inline constexpr bool IsResourceHandleV = IsResourceHandle<T>::value;

    template<typename T>
    constexpr reflection::EPropertyType ResolveResourceHandlePropertyType()
    {
        if constexpr (IsResourceHandleV<T>)
        {
            return reflection::EPropertyType::ResourceHandle;
        }
        else
        {
            return reflection::EPropertyType::UserDefined;
        }
    }
}