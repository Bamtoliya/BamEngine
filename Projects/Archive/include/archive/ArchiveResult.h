#pragma once

#include <string>

namespace archive
{
    enum class ArchiveErrorCode
    {
        None,
        MissingField,
        InvalidValue,
        InvalidJson,
        SerializationFailed,
        FileOpenFailed,
        FileReadFailed,
        FileWriteFailed,
        InvalidOperation,
        IndexOutOfRange,
        UnsupportedType
    };

    struct ArchiveResult
    {
        ArchiveErrorCode Code = ArchiveErrorCode::None;
        std::string Field;

        explicit operator bool() const noexcept
        {
            return Code == ArchiveErrorCode::None;
        }
    };
}