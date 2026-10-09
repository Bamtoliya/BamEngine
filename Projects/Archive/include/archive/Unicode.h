#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace archive::detail
{
    static_assert(sizeof(wchar_t) == 2 || sizeof(wchar_t) == 4, "Unsupported wchar_t size.");

    inline bool WideToUtf8(std::wstring_view text, std::string& result)
    {
        using UnsignedWide = std::make_unsigned_t<wchar_t>;

        std::string candidate;
        candidate.reserve(text.size());

        for (std::size_t index = 0; index < text.size(); ++index)
        {
            std::uint32_t codePoint = static_cast<UnsignedWide>(text[index]);

            if constexpr (sizeof(wchar_t) == 2)
            {
                if (codePoint >= 0xD800 && codePoint <= 0xDBFF)
                {
                    if (index + 1 == text.size())
                    {
                        return false;
                    }

                    const std::uint32_t low = static_cast<UnsignedWide>(text[++index]);

                    if (low < 0xDC00 || low > 0xDFFF)
                    {
                        return false;
                    }

                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                }
                else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF)
                {
                    return false;
                }
            }
            else if (codePoint >= 0xD800 && codePoint <= 0xDFFF)
            {
                return false;
            }

            if (codePoint > 0x10FFFF)
            {
                return false;
            }

            if (codePoint <= 0x7F)
            {
                candidate.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FF)
            {
                candidate.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
                candidate.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else if (codePoint <= 0xFFFF)
            {
                candidate.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
                candidate.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                candidate.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else
            {
                candidate.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
                candidate.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
                candidate.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                candidate.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
        }

        result = std::move(candidate);
        return true;
    }

    inline bool Utf8ToWide(std::string_view text, std::wstring& result)
    {
        std::wstring candidate;
        candidate.reserve(text.size());

        for (std::size_t index = 0; index < text.size();)
        {
            const auto first = static_cast<unsigned char>(text[index++]);
            std::uint32_t codePoint = 0;
            std::uint32_t minimum = 0;
            std::size_t continuationCount = 0;

            if (first <= 0x7F)
            {
                codePoint = first;
            }
            else if (first >= 0xC2 && first <= 0xDF)
            {
                codePoint = first & 0x1F;
                minimum = 0x80;
                continuationCount = 1;
            }
            else if (first >= 0xE0 && first <= 0xEF)
            {
                codePoint = first & 0x0F;
                minimum = 0x800;
                continuationCount = 2;
            }
            else if (first >= 0xF0 && first <= 0xF4)
            {
                codePoint = first & 0x07;
                minimum = 0x10000;
                continuationCount = 3;
            }
            else
            {
                return false;
            }

            if (text.size() - index < continuationCount)
            {
                return false;
            }

            for (std::size_t count = 0; count < continuationCount; ++count)
            {
                const auto next = static_cast<unsigned char>(text[index++]);

                if ((next & 0xC0) != 0x80)
                {
                    return false;
                }

                codePoint = (codePoint << 6) | (next & 0x3F);
            }

            if (codePoint < minimum || codePoint > 0x10FFFF ||
                (codePoint >= 0xD800 && codePoint <= 0xDFFF))
            {
                return false;
            }

            if constexpr (sizeof(wchar_t) == 2)
            {
                if (codePoint > 0xFFFF)
                {
                    const std::uint32_t offset = codePoint - 0x10000;
                    candidate.push_back(static_cast<wchar_t>(0xD800 + (offset >> 10)));
                    candidate.push_back(static_cast<wchar_t>(0xDC00 + (offset & 0x3FF)));
                }
                else
                {
                    candidate.push_back(static_cast<wchar_t>(codePoint));
                }
            }
            else
            {
                candidate.push_back(static_cast<wchar_t>(codePoint));
            }
        }

        result = std::move(candidate);
        return true;
    }
}