#pragma once

#include "ReflectionRegistry.h"

#define BEGIN_METADATA(ClassName, PropName) \
    static constexpr Engine::MetadataEntry ClassName##_##PropName##_Meta[] = {

#define END_METADATA };

#define DECLARE_CONTAINER_INFO(ClassName, PropName, InnerTypeName, InnerTypeEnum, ...) \
    static constexpr Engine::ContainerAccessor ClassName##_##PropName##_Accessor = __VA_ARGS__; \
    static constexpr Engine::ContainerInfo ClassName##_##PropName##_ContainerData = { \
        {InnerTypeName, InnerTypeEnum}, {"", Engine::EPropertyType::None}, &ClassName##_##PropName##_Accessor, nullptr \
    };

#define DECLARE_NESTED_CONTAINER_INFO(ClassName, PropName, InnerTypeName, InnerTypeEnum, InnerContainerPtr, ...) \
    static constexpr Engine::ContainerAccessor ClassName##_##PropName##_Accessor = __VA_ARGS__; \
    static constexpr Engine::ContainerInfo ClassName##_##PropName##_ContainerData = { \
        {InnerTypeName, InnerTypeEnum}, {"", Engine::EPropertyType::None}, &ClassName##_##PropName##_Accessor, InnerContainerPtr \
    };

#define DECLARE_MAP_INFO(ClassName, PropName, KeyTypeName, KeyTypeEnum, InnerTypeName, InnerTypeEnum, ...) \
    static constexpr Engine::ContainerAccessor ClassName##_##PropName##_Accessor = __VA_ARGS__; \
    static constexpr Engine::ContainerInfo ClassName##_##PropName##_ContainerData = { \
        {InnerTypeName, InnerTypeEnum}, {KeyTypeName, KeyTypeEnum}, &ClassName##_##PropName##_Accessor, nullptr \
    };

#define DECLARE_MAP_NESTED_VALUE_INFO(ClassName, PropName, KeyTypeName, KeyTypeEnum, ValueTypeName, ValueTypeEnum, ValueContainerPtr, ...) \
    static constexpr Engine::ContainerAccessor ClassName##_##PropName##_Accessor = __VA_ARGS__; \
    static constexpr Engine::ContainerInfo ClassName##_##PropName##_ContainerData = { \
        {ValueTypeName, ValueTypeEnum}, {KeyTypeName, KeyTypeEnum}, &ClassName##_##PropName##_Accessor, ValueContainerPtr \
    };

#define BEGIN_PROPERTIES(ClassName) \
    constexpr std::span<const Engine::PropertyInfo> ClassName::GetProperties() noexcept { \
        static constexpr Engine::PropertyInfo Props[] = {

#define END_PROPERTIES \
        }; \
        return Props; \
    }

#define EMPTY_PROPERTIES(ClassName) \
    constexpr std::span<const Engine::PropertyInfo> ClassName::GetProperties() noexcept { \
        return {}; \
    }

#define REFLECT_PROPERTY(Class, VarName, TypeName, TypeEnum, MetaSpanExpr) \
    { Engine::CompileTimeHash(#VarName), #VarName, {TypeName, TypeEnum}, offsetof(Class, VarName), sizeof(Class::VarName), nullptr, MetaSpanExpr, \
      &Engine::PropertyCopy<decltype(Class::VarName)>, &Engine::PropertyEqual<decltype(Class::VarName)> },

#define REFLECT_CONTAINER_PROPERTY(Class, VarName, TypeName, TypeEnum, ContainerDataPtr, MetaSpanExpr) \
    { Engine::CompileTimeHash(#VarName), #VarName, {TypeName, TypeEnum}, offsetof(Class, VarName), sizeof(Class::VarName), ContainerDataPtr, MetaSpanExpr, \
      &Engine::PropertyCopy<decltype(Class::VarName)>, &Engine::PropertyEqual<decltype(Class::VarName)> },