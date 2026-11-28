#pragma once

#include <archive/Unicode.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Engine::Unicode
{
    [[nodiscard]]
    inline bool TryWideToUtf8(std::wstring_view input, std::string& output)
    {
        return archive::detail::WideToUtf8(input, output);
    }

    [[nodiscard]]
    inline bool TryUtf8ToWide(std::string_view input, std::wstring& output)
    {
        return archive::detail::Utf8ToWide(input, output);
    }

    [[nodiscard]]
    inline std::string WideToUtf8(std::wstring_view input)
    {
        std::string output;

        if (!TryWideToUtf8(input, output))
            throw std::invalid_argument("Invalid wide Unicode string");

        return output;
    }

    [[nodiscard]]
    inline std::wstring Utf8ToWide(std::string_view input)
    {
        std::wstring output;

        if (!TryUtf8ToWide(input, output))
            throw std::invalid_argument("Invalid UTF-8 string");

        return output;
    }

    [[nodiscard]]
    inline std::string ToUtf8Bytes(std::u8string_view input)
    {
        return std::string(input.begin(), input.end());
    }

    [[nodiscard]]
    inline std::u8string FromUtf8Bytes(std::string_view input)
    {
        return std::u8string(input.begin(), input.end());
    }
}