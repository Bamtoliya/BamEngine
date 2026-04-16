#pragma once

#include <cstdint>
#include <string_view>

namespace reflection
{
    struct ContainerAccessor;

    enum class EPropertyType : std::uint8_t
    {
        None = 0,

        Int8,
        Int16,
        Int32,
        Int64,

        UInt8,
        UInt16,
        UInt32,
        UInt64,

        Float32,
        Float64,
        Bool,

        String,
        WString,

        Object,
        Enum,
        BitFlag,

        Struct,
        Array,
        List,
        Map,
        Set,

        ResourceHandle,
        UserDefined
    };

    struct VariableInfo
    {
        std::string_view Name{};
        EPropertyType Type = EPropertyType::None;
        const ContainerAccessor* Accessor = nullptr;
    };

    struct ContainerInfo
    {
        VariableInfo Inner{};
        VariableInfo Key{};

        const ContainerAccessor* Accessor = nullptr;
        const ContainerInfo* InnerContainerData = nullptr;
    };
}