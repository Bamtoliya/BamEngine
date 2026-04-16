#pragma once

#include <reflection/core/ContainerInfo.h>

#include "Types.h"
#include <string_view>

namespace bam_reflection
{
    using int8 = Engine::int8;
    using int16 = Engine::int16;
    using int32 = Engine::int32;
    using int64 = Engine::int64;

    using uint8 = Engine::uint8;
    using uint16 = Engine::uint16;
    using uint32 = Engine::uint32;
    using uint64 = Engine::uint64;

    using f32 = Engine::f32;
    using f64 = Engine::f64;

    using vec2 = Engine::vec2;
    using vec3 = Engine::vec3;
    using vec4 = Engine::vec4;

    using quat = Engine::quat;
    using mat3 = Engine::mat3;
    using mat4 = Engine::mat4;
    inline reflection::EPropertyType ResolveBuiltinPropertyType(std::string_view type_name)
    {
        if (type_name == "int8") return reflection::EPropertyType::Int8;
        if (type_name == "int16") return reflection::EPropertyType::Int16;
        if (type_name == "int32" || type_name == "int") return reflection::EPropertyType::Int32;
        if (type_name == "int64") return reflection::EPropertyType::Int64;

        if (type_name == "uint8") return reflection::EPropertyType::UInt8;
        if (type_name == "uint16") return reflection::EPropertyType::UInt16;
        if (type_name == "uint32" || type_name == "uint") return reflection::EPropertyType::UInt32;
        if (type_name == "uint64") return reflection::EPropertyType::UInt64;

        if (type_name == "f32" || type_name == "float") return reflection::EPropertyType::Float32;
        if (type_name == "f64" || type_name == "double") return reflection::EPropertyType::Float64;
        if (type_name == "bool") return reflection::EPropertyType::Bool;

        if (type_name == "string" || type_name == "std::string") return reflection::EPropertyType::String;
        if (type_name == "wstring" || type_name == "std::wstring") return reflection::EPropertyType::WString;

        if (type_name == "vec2" || type_name == "glm::vec2") return reflection::EPropertyType::UserDefined;
        if (type_name == "vec3" || type_name == "glm::vec3") return reflection::EPropertyType::UserDefined;
        if (type_name == "vec4" || type_name == "glm::vec4") return reflection::EPropertyType::UserDefined;
        if (type_name == "quat" || type_name == "glm::quat") return reflection::EPropertyType::UserDefined;
        if (type_name == "mat3" || type_name == "glm::mat3") return reflection::EPropertyType::UserDefined;
        if (type_name == "mat4" || type_name == "glm::mat4") return reflection::EPropertyType::UserDefined;

        return reflection::EPropertyType::UserDefined;
    }
}