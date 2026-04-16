#pragma once

#include <reflection/core/Metadata.h>

#include "BamReflectionTypes.h"

#include <array>
#include <cfloat>
#include <string_view>
#include <type_traits>
#include <variant>
#include <utility>

namespace bam_reflection
{
    struct MetaRange
    {
        f32 Min = -FLT_MAX;
        f32 Max = FLT_MAX;
        f32 Speed = 1.0f;
    };

    struct MetaColor
    {
        vec4 RGBA = vec4(1.0f);
    };

    struct MetaEditCondition
    {
        std::string_view ConditionVariableName{};
        uint64 Mask = 0;
        bool ExactMatch = false;

        constexpr MetaEditCondition() = default;

        constexpr MetaEditCondition(std::string_view condition_variable_name, uint64 mask = 0, bool exact_match = false)
            : ConditionVariableName(condition_variable_name)
            , Mask(mask)
            , ExactMatch(exact_match)
        {
        }

        template<typename T>
            requires(std::is_integral_v<T> || std::is_enum_v<T>)
        constexpr MetaEditCondition(std::string_view condition_variable_name, T mask, bool exact_match = false)
            : ConditionVariableName(condition_variable_name)
            , Mask(static_cast<uint64>(mask))
            , ExactMatch(exact_match)
        {
        }
    };

    struct MetaOnChanged
    {
        static constexpr size_t MaxArgumentCount = 4;

        std::string_view FunctionName{};
        std::array<std::string_view, MaxArgumentCount> ArgumentPropertyNames{};
        uint32 ArgumentCount = 0;

        constexpr MetaOnChanged() = default;

        constexpr explicit MetaOnChanged(std::string_view function_name)
            : FunctionName(function_name)
        {
        }

        template<typename... TArgs>
            requires((std::is_convertible_v<TArgs, std::string_view> && ...) && (sizeof...(TArgs) <= MaxArgumentCount))
        constexpr MetaOnChanged(std::string_view function_name, TArgs... args)
            : FunctionName(function_name)
            , ArgumentPropertyNames{ std::string_view(args)... }
            , ArgumentCount(static_cast<uint32>(sizeof...(args)))
        {
        }
    };

    using BamMetadataValue = std::variant<
        reflection::MetadataValue,
        MetaRange,
        MetaColor,
        MetaEditCondition,
        MetaOnChanged,
        vec2,
        vec3,
        vec4,
        quat,
        mat3,
        mat4
    >;

    struct BamMetadataEntry
    {
        uint64 KeyHash = 0;
        BamMetadataValue Value{ reflection::MetadataValue{} };

        constexpr BamMetadataEntry() = default;

        constexpr BamMetadataEntry(uint64 key_hash, BamMetadataValue value)
            : KeyHash(key_hash)
            , Value(std::move(value))
        {
        }
    };

#define BAM_NAME(text) bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("Name"), std::string_view(text) },
#define BAM_TOOLTIP(text) bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("Tooltip"), std::string_view(text) },
#define BAM_CATEGORY(text) bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("Category"), std::string_view(text) },
#define BAM_RANGE(...) bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("Range"), bam_reflection::MetaRange{ __VA_ARGS__ } },
#define BAM_COLOR(r, g, b, a) bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("Color"), bam_reflection::MetaColor{ vec4(r, g, b, a) } },
#define BAM_EDITABLE bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("Editable"), true },
#define BAM_NOSERIALIZE bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("NoSerialize"), true },
#define BAM_READONLY bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("ReadOnly"), true },
#define BAM_EDITCONDITION(condition_var, ...) bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("EditCondition"), bam_reflection::MetaEditCondition{ condition_var, __VA_ARGS__ } },
#define BAM_DEFAULT(value) bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("Default"), std::string_view(#value) },
#define BAM_ONCHANGED(...) bam_reflection::BamMetadataEntry{ reflection::CompileTimeHash("OnChanged"), bam_reflection::MetaOnChanged{ __VA_ARGS__ } },
}