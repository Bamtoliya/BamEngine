#pragma once

#include "reflection/core/ContainerInfo.h"
#include "reflection/core/Metadata.h"
#include "reflection/core/PropertyInfo.h"
#include "reflection/runtime/PropertyAccessor.h"
#include <entt/entt.hpp> // 필수!

#define BEGIN_METADATA(TypeName, PropertyName) \
    static constexpr reflection::MetadataEntry TypeName##_##PropertyName##_Meta[] = {

#define END_METADATA \
    };

#define DECLARE_CONTAINER_INFO(TypeName, PropertyName, InnerTypeName, InnerTypeEnum, ...) \
    static constexpr reflection::ContainerAccessor TypeName##_##PropertyName##_Accessor = __VA_ARGS__; \
    static constexpr reflection::ContainerInfo TypeName##_##PropertyName##_ContainerData = { \
        { InnerTypeName, InnerTypeEnum, nullptr }, \
        { "", reflection::EPropertyType::None, nullptr }, \
        &TypeName##_##PropertyName##_Accessor, \
        nullptr \
    };

#define DECLARE_NESTED_CONTAINER_INFO(TypeName, PropertyName, InnerTypeName, InnerTypeEnum, InnerContainerPtr, ...) \
    static constexpr reflection::ContainerAccessor TypeName##_##PropertyName##_Accessor = __VA_ARGS__; \
    static constexpr reflection::ContainerInfo TypeName##_##PropertyName##_ContainerData = { \
        { InnerTypeName, InnerTypeEnum, nullptr }, \
        { "", reflection::EPropertyType::None, nullptr }, \
        &TypeName##_##PropertyName##_Accessor, \
        InnerContainerPtr \
    };

#define DECLARE_MAP_INFO(TypeName, PropertyName, KeyTypeName, KeyTypeEnum, ValueTypeName, ValueTypeEnum, ...) \
    static constexpr reflection::ContainerAccessor TypeName##_##PropertyName##_Accessor = __VA_ARGS__; \
    static constexpr reflection::ContainerInfo TypeName##_##PropertyName##_ContainerData = { \
        { ValueTypeName, ValueTypeEnum, nullptr }, \
        { KeyTypeName, KeyTypeEnum, nullptr }, \
        &TypeName##_##PropertyName##_Accessor, \
        nullptr \
    };

#define DECLARE_MAP_NESTED_VALUE_INFO(TypeName, PropertyName, KeyTypeName, KeyTypeEnum, ValueTypeName, ValueTypeEnum, ValueContainerPtr, ...) \
    static constexpr reflection::ContainerAccessor TypeName##_##PropertyName##_Accessor = __VA_ARGS__; \
    static constexpr reflection::ContainerInfo TypeName##_##PropertyName##_ContainerData = { \
        { ValueTypeName, ValueTypeEnum, nullptr }, \
        { KeyTypeName, KeyTypeEnum, nullptr }, \
        &TypeName##_##PropertyName##_Accessor, \
        ValueContainerPtr \
    };

#define BEGIN_PROPERTIES(TypeName) \
    constexpr std::span<const reflection::PropertyInfo> TypeName::GetProperties() noexcept \
    { \
        static constexpr reflection::PropertyInfo Properties[] = {

#define END_PROPERTIES \
        }; \
        return Properties; \
    }

#define EMPTY_PROPERTIES(TypeName) \
    constexpr std::span<const reflection::PropertyInfo> TypeName::GetProperties() noexcept \
    { \
        return {}; \
    }

#define REFLECT_PROPERTY(TypeName, PropertyName, PropertyTypeName, PropertyTypeEnum, MetadataSpanExpr) \
    { \
        reflection::CompileTimeHash(#PropertyName), \
        #PropertyName, \
        { PropertyTypeName, PropertyTypeEnum, nullptr }, \
        offsetof(TypeName, PropertyName), \
        sizeof(TypeName::PropertyName), \
        nullptr, \
        MetadataSpanExpr, \
        &reflection::PropertyCopy<decltype(TypeName::PropertyName)>, \
        &reflection::PropertyEqual<decltype(TypeName::PropertyName)> \
    },

#define REFLECT_CONTAINER_PROPERTY(TypeName, PropertyName, PropertyTypeName, PropertyTypeEnum, ContainerDataPtr, MetadataSpanExpr) \
    { \
        reflection::CompileTimeHash(#PropertyName), \
        #PropertyName, \
        { PropertyTypeName, PropertyTypeEnum, (ContainerDataPtr)->Accessor }, \
        offsetof(TypeName, PropertyName), \
        sizeof(TypeName::PropertyName), \
        ContainerDataPtr, \
        MetadataSpanExpr, \
        &reflection::PropertyCopy<decltype(TypeName::PropertyName)>, \
        &reflection::PropertyEqual<decltype(TypeName::PropertyName)> \
    },


// meta<Type>() 대신 meta_factory<Type>{} 사용
#define BEGIN_ENTT_REFLECT(ClassType) { entt::meta_factory<ClassType>{}.type(#ClassType)
#define END_ENTT_REFLECT() ; }

#define ENTT_PROPERTY(ClassType, PropName) \
    .data< \
        +[](ClassType& obj, const decltype(ClassType::PropName)& val) { obj.PropName = val; }, \
        +[](const ClassType& obj) -> const decltype(ClassType::PropName)& { return obj.PropName; } \
    >(#PropName)

#define ENTT_PROPERTY_WITH_ATTR(ClassType, PropName, Attributes) \
    .data< \
        +[](ClassType& obj, const decltype(ClassType::PropName)& val) { obj.PropName = val; }, \
        +[](const ClassType& obj) -> const decltype(ClassType::PropName)& { return obj.PropName; } \
    >(#PropName).template custom<const char*>(Attributes)

#define ENTT_FUNCTION(ClassType, FuncName) \
    .func<&ClassType::FuncName>(#FuncName)

#define BEGIN_ENTT_REFLECT_ENUM(EnumType) { entt::meta_factory<EnumType>{}.type(#EnumType)
#define ENTT_ENUM_ENTRY(EnumType, EntryName) \
    .data<EnumType::EntryName>(#EntryName)
#define END_ENTT_REFLECT_ENUM() ; }