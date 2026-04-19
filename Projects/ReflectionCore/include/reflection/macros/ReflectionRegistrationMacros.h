#pragma once

#include "reflection/core/EnumInfo.h"
#include "reflection/core/TypeInfo.h"
#include "reflection/runtime/ReflectableInterface.h"
#include "reflection/runtime/Registry.h"

#include <memory>
#include <type_traits>
#include <utility>

namespace reflection::detail
{
    template<typename T, typename = void>
    struct HasPostLoad : std::false_type
    {
    };

    template<typename T>
    struct HasPostLoad<T, std::void_t<decltype(std::declval<T&>().PostLoad())>> : std::true_type
    {
    };

    template<typename T>
    inline constexpr bool HasPostLoad_v = HasPostLoad<T>::value;

    template<typename T>
    inline constexpr bool CanCreateReflectionObject_v = !std::is_abstract_v<T>;
    template<typename T>
    inline constexpr bool CanDestroyReflectionObject_v = !std::is_abstract_v<T>;
    template<typename T>
    inline constexpr bool CanCopyReflectionObject_v = !std::is_abstract_v<T>;

    //template<typename T>
    //inline void CreateReflectionObjectIfSupported(void* buffer)
    //{
    //    if constexpr (CanCreateReflectionObject_v<T>)
    //    {
    //        std::construct_at(static_cast<T*>(buffer));
    //    }
    //}

    //template<typename T>
    //inline void DestroyReflectionObjectIfSupported(void* buffer)
    //{
    //    if constexpr (CanDestroyReflectionObject_v<T>)
    //    {
    //        std::destroy_at(static_cast<T*>(buffer));
    //    }
    //}

    //template<typename T>
    //inline void CopyReflectionObjectIfSupported(void* dst, const void* src)
    //{
    //    if constexpr (CanCopyReflectionObject_v<T>)
    //    {
    //        *static_cast<T*>(dst) = *static_cast<const T*>(src);
    //    }
    //}

    //template<typename T>
    //inline void PostLoadReflectionObjectIfSupported(void* instance)
    //{
    //    if constexpr (HasPostLoad_v<T>)
    //    {
    //        static_cast<T*>(instance)->PostLoad();
    //    }
    //}
}

#define IMPLEMENT_CLASS_EX(TypeName, QualifiedTypeNameLiteral, QualifiedParentTypeNameLiteral) \
    void TypeName::CreateReflectionObject(void* buffer) \
    { \
        [](auto* tag, void* buf) { \
            using T = std::remove_pointer_t<decltype(tag)>; \
            if constexpr (reflection::detail::CanCreateReflectionObject_v<T>) \
            { \
                new(buf) T(); \
            } \
        }(static_cast<TypeName*>(nullptr), buffer); \
    } \
    void TypeName::DestroyReflectionObject(void* buffer) \
    { \
        [](auto* tag, void* buf) { \
            using T = std::remove_pointer_t<decltype(tag)>; \
            if constexpr (reflection::detail::CanDestroyReflectionObject_v<T>) \
            { \
                static_cast<T*>(buf)->~T(); \
            } \
        }(static_cast<TypeName*>(nullptr), buffer); \
    } \
    void TypeName::CopyReflectionObject(void* dst, const void* src) \
    { \
        [](auto* tag, void* d, const void* s) { \
            using T = std::remove_pointer_t<decltype(tag)>; \
            if constexpr (reflection::detail::CanCopyReflectionObject_v<T>) \
            { \
                *static_cast<T*>(d) = *static_cast<const T*>(s); \
            } \
        }(static_cast<TypeName*>(nullptr), dst, src); \
    } \
    /*void TypeName::PostLoadReflectionObject(void* instance) \
    { \
        if constexpr (reflection::detail::HasPostLoad_v<TypeName>) \
        { \
            static_cast<TypeName*>(instance)->PostLoad(); \
        } \
    }*/ \
    static constexpr reflection::TypeInfo TypeName##_TypeInfo = { \
        reflection::CompileTimeHash(QualifiedTypeNameLiteral), \
        QualifiedTypeNameLiteral, \
        QualifiedParentTypeNameLiteral, \
        sizeof(TypeName), \
        TypeName::GetProperties(), \
        TypeName##_Functions, \
        reflection::detail::CanCreateReflectionObject_v<TypeName> ? &TypeName::CreateReflectionObject : nullptr, \
        reflection::detail::CanDestroyReflectionObject_v<TypeName> ? &TypeName::DestroyReflectionObject : nullptr, \
        reflection::detail::CanCopyReflectionObject_v<TypeName> ? &TypeName::CopyReflectionObject : nullptr, \
        reflection::detail::HasPostLoad_v<TypeName> ? &TypeName::PostLoadReflectionObject : nullptr, \
        &reflection::ResolveDynamicTypeInfo<TypeName> \
    }; \
    const reflection::TypeInfo& TypeName::GetStaticTypeInfo() \
    { \
        return TypeName##_TypeInfo; \
    } \
    namespace \
    { \
        struct AutoRegister_##TypeName \
        { \
            AutoRegister_##TypeName() \
            { \
                auto& registry = reflection::Registry::Get(); \
                registry.RegisterType(TypeName##_TypeInfo); \
                for (const auto& function_info : TypeName##_Functions) \
                { \
                    registry.RegisterFunction(function_info); \
                } \
            } \
        }; \
        static AutoRegister_##TypeName g_AutoRegister_##TypeName; \
    }

#define IMPLEMENT_CLASS(TypeName, ParentQualifiedNameLiteral) \
    IMPLEMENT_CLASS_EX(TypeName, #TypeName, #ParentQualifiedNameLiteral)

#define BEGIN_ENUM(EnumName) \
    static constexpr reflection::EnumEntry EnumName##_Entries[] = {

#define REFLECT_ENUM_ENTRY(EnumName, EntryName) \
    { #EntryName, static_cast<std::uint64_t>(EnumName::EntryName) },

#define END_ENUM_REFLECT_EX(EnumName, QualifiedEnumNameLiteral) \
    }; \
    static constexpr reflection::EnumInfo EnumName##_EnumInfo = { \
        reflection::CompileTimeHash(QualifiedEnumNameLiteral), \
        QualifiedEnumNameLiteral, \
        EnumName##_Entries \
    }; \
    namespace \
    { \
        struct AutoRegister_##EnumName \
        { \
            AutoRegister_##EnumName() \
            { \
                reflection::Registry::Get().RegisterEnum(EnumName##_EnumInfo); \
            } \
        }; \
        static AutoRegister_##EnumName g_AutoRegister_##EnumName; \
    }

#define END_ENUM_REFLECT(EnumName) \
    END_ENUM_REFLECT_EX(EnumName, #EnumName)