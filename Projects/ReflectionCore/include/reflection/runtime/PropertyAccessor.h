#pragma once

#include "reflection/core/PropertyInfo.h"

#include <concepts>
#include <cstring>
#include <type_traits>
#include <utility>

namespace reflection
{
    class PropertyAccessor
    {
    public:
        static void* GetValuePtr(void* instance, const PropertyInfo& prop_info)
        {
            if (!instance)
            {
                return nullptr;
            }
            return static_cast<void*>(static_cast<unsigned char*>(instance) + prop_info.Offset);
        }

        static const void* GetValuePtr(const void* instance, const PropertyInfo& prop_info)
        {
            if (!instance)
            {
                return nullptr;
            }
            return static_cast<const void*>(static_cast<const unsigned char*>(instance) + prop_info.Offset);
        }

        template<typename T>
        static T& GetValueRef(void* instance, const PropertyInfo& prop_info)
        {
            return *static_cast<T*>(GetValuePtr(instance, prop_info));
        }

        template<typename T>
        static const T& GetValueRef(const void* instance, const PropertyInfo& prop_info)
        {
            return *static_cast<const T*>(GetValuePtr(instance, prop_info));
        }

        template<typename T>
        static void SetValue(void* instance, const PropertyInfo& prop_info, const T& value)
        {
            *static_cast<T*>(GetValuePtr(instance, prop_info)) = value;
        }
    };

    template<typename T>
    inline void PropertyCopy(void* dst, const void* src)
    {
        *static_cast<T*>(dst) = *static_cast<const T*>(src);
    }

    template<typename T>
    inline bool PropertyEqual(const void* a, const void* b)
    {
        if constexpr (requires(const T & lhs, const T & rhs) { { lhs == rhs } -> std::convertible_to<bool>; })
        {
            return *static_cast<const T*>(a) == *static_cast<const T*>(b);
        }
        else
        {
            return std::memcmp(a, b, sizeof(T)) == 0;
        }
    }
}