#pragma once

#include "ReflectionRegistry.h"

#define PROPERTY(...)
#define CLASS()
#define STRUCT()
#define ENUM(...)
#define FUNCTION(...)

#define REFLECT_CDO \
    static void CreateCDO(void* buffer); \
    static void DestroyCDO(void* buffer); \
    static void CopyCDO(void* dst, const void* src); \
    static void* HeapNew(); \
    virtual void PostLoad() {}

#define REFLECT_BASE() \
public: \
    static const Engine::TypeInfo& GetStaticTypeInfo(); \
    virtual const Engine::TypeInfo& GetTypeInfo() const { return GetStaticTypeInfo(); } \
    static constexpr std::span<const Engine::PropertyInfo> GetProperties() noexcept; \
    REFLECT_CDO

#define REFLECT_STRUCT() REFLECT_BASE()

#define REFLECT_CLASS() \
public: \
    static const Engine::TypeInfo& GetStaticTypeInfo(); \
    static constexpr std::span<const Engine::PropertyInfo> GetProperties() noexcept; \
    virtual const Engine::TypeInfo& GetTypeInfo() const override { return GetStaticTypeInfo(); } \
    REFLECT_CDO