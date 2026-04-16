#pragma once

#include "reflection/core/ContainerInfo.h"
#include "reflection/core/Metadata.h"
#include "reflection/core/PropertyInfo.h"
#include "reflection/runtime/PropertyAccessor.h"

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