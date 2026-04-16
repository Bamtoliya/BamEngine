#pragma once

#include "reflection/core/TypeInfo.h"

#include <type_traits>

namespace reflection
{
    class ReflectableInterface
    {
    public:
        virtual ~ReflectableInterface() = default;
        virtual const TypeInfo& GetTypeInfo() const noexcept = 0;
    };

    template<typename T>
    inline const TypeInfo& ResolveDynamicTypeInfo(const void* instance) noexcept
    {
        if constexpr (std::is_base_of_v<ReflectableInterface, T>)
        {
            return static_cast<const T*>(instance)->GetTypeInfo();
        }
        else
        {
            return T::GetStaticTypeInfo();
        }
    }
}