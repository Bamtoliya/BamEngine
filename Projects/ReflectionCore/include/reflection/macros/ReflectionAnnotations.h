#pragma once

#include "reflection/core/TypeInfo.h"
#include "reflection/runtime/ReflectableInterface.h"

#define PROPERTY(...)
#define CLASS()
#define STRUCT()
#define ENUM(...)
#define FUNCTION(...)

#define REFLECT_COMMON_DECLS() \
public: \
    static const reflection::TypeInfo& GetStaticTypeInfo(); \
    static constexpr std::span<const reflection::PropertyInfo> GetProperties() noexcept; \
    static void CreateReflectionObject(void* buffer); \
    static void DestroyReflectionObject(void* buffer); \
    static void CopyReflectionObject(void* dst, const void* src); \
    static void PostLoadReflectionObject(void* instance);

#define REFLECT_STRUCT() \
    REFLECT_COMMON_DECLS()

#define REFLECT_CLASS() \
public: \
    REFLECT_COMMON_DECLS() \
    const reflection::TypeInfo& GetTypeInfo() const noexcept override { return GetStaticTypeInfo(); }

#define REFLECT_BASE() \
    REFLECT_CLASS()