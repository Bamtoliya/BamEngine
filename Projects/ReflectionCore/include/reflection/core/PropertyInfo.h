#pragma once

#include "reflection/core/ContainerInfo.h"
#include "reflection/core/Metadata.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace reflection
{
    struct PropertyInfo
    {
        std::uint64_t ID = 0;
        std::string_view Name{};
        VariableInfo TypeInfo{};
        std::size_t Offset = 0;
        std::size_t Size = 0;

        const ContainerInfo* ContainerData = nullptr;
        std::span<const MetadataEntry> Metadata{};

        void (*CopyProp)(void* dst, const void* src) = nullptr;
        bool (*EqualProp)(const void* a, const void* b) = nullptr;
    };
}