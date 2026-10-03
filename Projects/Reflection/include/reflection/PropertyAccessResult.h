#pragma once

#include <reflection/ValueView.h>
#include <reflection/ObjectView.h>
#include <string_view>
#include <cstddef>

namespace reflection
{
    enum class PropertyAccessError
    {
        None,
        InvalidObject,
        OwnerTypeMismatch,
        ReadUnavailable,
        WriteUnavailable,
        ReadOnlyObject,
        InvalidValue,
        ValueTypeMismatch,
        ObjectUnavailable,
        ReadOnlyProperty,
        NotContainer,
        IndexOutOfRange,
        ElementUnavailable,
        ElementWriteUnavailable,
        ResizeUnavailable,
        SizeOutOfRange,
        ClearUnavailable,
        AppendUnavailable,
        InsertUnavailable,
        EraseUnavailable,
        EnumWriteUnavailable,
        ValueOutOfRange
    };

    constexpr std::string_view ToString(PropertyAccessError error)
    {
        switch (error)
        {
        case PropertyAccessError::None: return "None";
        case PropertyAccessError::InvalidObject: return "InvalidObject";
        case PropertyAccessError::OwnerTypeMismatch: return "OwnerTypeMismatch";
        case PropertyAccessError::ReadUnavailable: return "ReadUnavailable";
        case PropertyAccessError::WriteUnavailable: return "WriteUnavailable";
        case PropertyAccessError::ReadOnlyObject: return "ReadOnlyObject";
        case PropertyAccessError::InvalidValue: return "InvalidValue";
        case PropertyAccessError::ValueTypeMismatch: return "ValueTypeMismatch";
        case PropertyAccessError::ObjectUnavailable: return "ObjectUnavailable";
        case PropertyAccessError::ReadOnlyProperty: return "ReadOnlyProperty";
        case PropertyAccessError::NotContainer: return "NotContainer";
        case PropertyAccessError::IndexOutOfRange: return "IndexOutOfRange";
        case PropertyAccessError::ElementUnavailable: return "ElementUnavailable";
        case PropertyAccessError::ElementWriteUnavailable: return "ElementWriteUnavailable";
        case PropertyAccessError::ResizeUnavailable: return "ResizeUnavailable";
        case PropertyAccessError::SizeOutOfRange: return "SizeOutOfRange";
        case PropertyAccessError::ClearUnavailable: return "ClearUnavailable";
        case PropertyAccessError::AppendUnavailable: return "AppendUnavailable";
        case PropertyAccessError::InsertUnavailable: return "InsertUnavailable";
        case PropertyAccessError::EraseUnavailable: return "EraseUnavailable";
        case PropertyAccessError::EnumWriteUnavailable: return "EnumWriteUnavailable";
        case PropertyAccessError::ValueOutOfRange: return "ValueOutOfRange";
        }

        return "Unknown";
    }

    struct PropertyReadResult
    {
        PropertyAccessError Error = PropertyAccessError::ReadUnavailable;
        ValueView Value;

        [[nodiscard]]
        bool Succeeded() const noexcept
        {
            return Error == PropertyAccessError::None && Value.IsValid();
        }

        explicit operator bool() const noexcept
        {
            return Succeeded();
        }
    };

    struct PropertyObjectResult
    {
        PropertyAccessError Error = PropertyAccessError::ObjectUnavailable;
        ObjectView Object;

        [[nodiscard]]
        bool Succeeded() const noexcept
        {
            return Error == PropertyAccessError::None && Object.IsValid();
        }

        explicit operator bool() const noexcept
        {
            return Succeeded();
        }
    };

    struct PropertySizeResult
    {
        PropertyAccessError Error = PropertyAccessError::NotContainer;
        std::size_t Size = 0;

        [[nodiscard]]
        bool Succeeded() const noexcept
        {
            return Error == PropertyAccessError::None;
        }

        explicit operator bool() const noexcept
        {
            return Succeeded();
        }
    };
}