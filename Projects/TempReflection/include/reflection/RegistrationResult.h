#pragma once

#include <string>
#include <string_view>

namespace reflection
{
    enum class RegistrationError
    {
        None,
        InvalidTypeName,
        DuplicateType,
        InvalidProperty,
        DuplicateProperty,
        InvalidFunction,
        DuplicateFunction,
        InvalidParameter,
        InvalidEnum,
        DuplicateEnum,
        InvalidEnumEntry,
        DuplicateEnumEntry,
        InvalidFreeFunction,
        DuplicateFreeFunction,
        DuplicateMetadata,
        InvalidMergeSource
    };

    constexpr std::string_view ToString(RegistrationError error)
    {
        switch (error)
        {
        case RegistrationError::None: return "None";
        case RegistrationError::InvalidTypeName: return "InvalidTypeName";
        case RegistrationError::DuplicateType: return "DuplicateType";
        case RegistrationError::InvalidProperty: return "InvalidProperty";
        case RegistrationError::DuplicateProperty: return "DuplicateProperty";
        case RegistrationError::InvalidFunction: return "InvalidFunction";
        case RegistrationError::DuplicateFunction: return "DuplicateFunction";
        case RegistrationError::InvalidParameter: return "InvalidParameter";
        case RegistrationError::InvalidEnum: return "InvalidEnum";
        case RegistrationError::DuplicateEnum: return "DuplicateEnum";
        case RegistrationError::InvalidEnumEntry: return "InvalidEnumEntry";
        case RegistrationError::DuplicateEnumEntry: return "DuplicateEnumEntry";
        case RegistrationError::InvalidFreeFunction: return "InvalidFreeFunction";
        case RegistrationError::DuplicateFreeFunction: return "DuplicateFreeFunction";
        case RegistrationError::DuplicateMetadata: return "DuplicateMetadata";
        case RegistrationError::InvalidMergeSource: return "InvalidMergeSource";
        }

        return "Unknown";
    }

    struct RegistrationResult
    {
        RegistrationError Error = RegistrationError::None;
        std::string Owner;
        std::string Member;

        [[nodiscard]]
        bool Succeeded() const noexcept
        {
            return Error == RegistrationError::None;
        }

        explicit operator bool() const noexcept
        {
            return Succeeded();
        }
    };
}