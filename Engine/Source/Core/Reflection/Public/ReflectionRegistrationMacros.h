#pragma once

#include "ReflectionRegistry.h"

#define IMPLEMENT_CLASS_EX(ClassName, QualifiedTypeNameLiteral, QualifiedParentTypeNameLiteral) \
    void ClassName::CreateCDO(void* cdoBuf) \
    { \
        [](auto* tag, void* b) { \
            using T = std::remove_pointer_t<decltype(tag)>; \
            if constexpr (!std::is_abstract_v<T>) { new(b) T(); } \
        }(static_cast<ClassName*>(nullptr), cdoBuf); \
    } \
    void ClassName::DestroyCDO(void* cdoBuf) \
    { \
        [](auto* tag, void* b) { \
            using T = std::remove_pointer_t<decltype(tag)>; \
            if constexpr (!std::is_abstract_v<T>) { static_cast<T*>(b)->~T(); } \
        }(static_cast<ClassName*>(nullptr), cdoBuf); \
    } \
    void ClassName::CopyCDO(void* dst, const void* src) \
    { \
        [](auto* tag, void* d, const void* s) { \
            using T = std::remove_pointer_t<decltype(tag)>; \
            if constexpr (!std::is_abstract_v<T>) { \
                *static_cast<T*>(d) = *static_cast<const T*>(s); \
            } \
        }(static_cast<ClassName*>(nullptr), dst, src); \
    } \
    static constexpr Engine::TypeInfo ClassName##_TypeInfo = { \
        Engine::CompileTimeHash(QualifiedTypeNameLiteral), \
        QualifiedTypeNameLiteral, \
        QualifiedParentTypeNameLiteral, \
        sizeof(ClassName), \
        ClassName::GetProperties(), \
        ClassName##_Functions, \
        std::is_abstract_v<ClassName> ? nullptr : &ClassName::CreateCDO, \
        std::is_abstract_v<ClassName> ? nullptr : &ClassName::DestroyCDO, \
        std::is_abstract_v<ClassName> ? nullptr : &ClassName::CopyCDO, \
        [](void* inst) { static_cast<ClassName*>(inst)->PostLoad(); } \
    }; \
    const Engine::TypeInfo& ClassName::GetStaticTypeInfo() { return ClassName##_TypeInfo; } \
    namespace { \
        struct AutoRegister_##ClassName { \
            AutoRegister_##ClassName() { \
                auto& registry = Engine::ReflectionRegistry::Get(); \
                registry.RegisterType(ClassName##_TypeInfo); \
                for (const auto& functionInfo : ClassName##_Functions) \
                { \
                    registry.RegisterFunction(functionInfo); \
                } \
            } \
        }; \
        static AutoRegister_##ClassName g_AutoRegister_##ClassName; \
    }

#define IMPLEMENT_CLASS(ClassName, ParentQualifiedName) \
    IMPLEMENT_CLASS_EX(ClassName, #ClassName, #ParentQualifiedName)

#define BEGIN_ENUM(EnumName) \
    static constexpr Engine::EnumEntry EnumName##_Entries[] = {

#define REFLECT_ENUM_ENTRY(EnumName, EntryName) \
    { #EntryName, static_cast<uint64_t>(EnumName::EntryName) },

#define END_ENUM_REFLECT_EX(EnumName, QualifiedEnumNameLiteral) \
    }; \
    static constexpr Engine::EnumInfo EnumName##_EnumInfo = { \
        Engine::CompileTimeHash(QualifiedEnumNameLiteral), \
        QualifiedEnumNameLiteral, \
        EnumName##_Entries \
    }; \
    namespace { \
        struct AutoRegister_##EnumName { \
            AutoRegister_##EnumName() { \
                Engine::ReflectionRegistry::Get().RegisterEnum(EnumName##_EnumInfo); \
            } \
        }; \
        static AutoRegister_##EnumName g_AutoRegister_##EnumName; \
    }

#define END_ENUM_REFLECT(EnumName) \
    END_ENUM_REFLECT_EX(EnumName, #EnumName)