#pragma once

#include "reflection/core/FunctionInfo.h"
#include "reflection/core/PropertyInfo.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace reflection
{
    struct TypeInfo
    {
        using CreateFunc = void (*)(void* buffer);
        using DestroyFunc = void (*)(void* buffer);
        using CopyFunc = void (*)(void* dst, const void* src);
        using PostLoadFunc = void (*)(void* instance);
        using DynamicTypeFunc = const TypeInfo& (*)(const void* instance);

        std::uint64_t ID = 0;
        std::string_view QualifiedName{};
        std::string_view ParentQualifiedName{};
        std::size_t Size = 0;

        std::span<const PropertyInfo> Properties{};
        std::span<const FunctionInfo> Functions{};

        CreateFunc Create = nullptr;
        DestroyFunc Destroy = nullptr;
        CopyFunc Copy = nullptr;
        PostLoadFunc PostLoad = nullptr;
        DynamicTypeFunc GetDynamicType = nullptr;
    };
}