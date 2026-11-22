#pragma once

#include "UITextComponent.h"
#include <algorithm>
#include <cstddef>
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

namespace Engine
{
    enum class EUIVerticalAlignment
    {
        Top,
        Middle,
        Bottom
    };

    struct UILineMetrics
    {
        std::size_t startByte = 0;
        std::size_t byteCount = 0;
        float width = 0.0f;
        glm::vec2 offset{ 0.0f }; // (x, y) relative to bounding box top-left
        std::string_view text;
    };

    struct UIMultilineLayoutResult
    {
        std::vector<UILineMetrics> lines;
        float totalWidth = 0.0f;
        float totalHeight = 0.0f;
    };

    namespace Detail
    {
        [[nodiscard]] constexpr bool IsWhitespace(char c) noexcept
        {
            return c == ' ' || c == '\t';
        }

        [[nodiscard]] constexpr bool IsCjkByte(unsigned char c) noexcept
        {
            return c >= 0xE0;
        }

        [[nodiscard]] inline std::size_t NextCodepointLength(std::string_view str, std::size_t pos) noexcept
        {
            if (pos >= str.size()) return 0;
            const auto c = static_cast<unsigned char>(str[pos]);
            if ((c & 0x80) == 0) return 1;
            if ((c & 0xE0) == 0xC0) return (pos + 1 < str.size()) ? 2 : 1;
            if ((c & 0xF0) == 0xE0) return (pos + 2 < str.size()) ? 3 : (str.size() - pos);
            if ((c & 0xF8) == 0xF0) return (pos + 3 < str.size()) ? 4 : (str.size() - pos);
            return 1;
        }

        [[nodiscard]] inline std::string_view TrimRightWhitespace(std::string_view str) noexcept
        {
            while (!str.empty() && (str.back() == ' ' || str.back() == '\t' || str.back() == '\r'))
            {
                str.remove_suffix(1);
            }
            return str;
        }

        template<typename MeasureFunc>
        void WrapParagraph(
            std::string_view para,
            std::size_t paraStartByte,
            float maxWidth,
            MeasureFunc&& measure,
            std::vector<UILineMetrics>& outLines)
        {
            if (para.empty())
            {
                UILineMetrics line;
                line.startByte = paraStartByte;
                line.byteCount = 0;
                line.width = 0.0f;
                line.text = std::string_view{};
                outLines.push_back(line);
                return;
            }

            if (maxWidth <= 0.0f)
            {
                std::string_view trimmed = TrimRightWhitespace(para);
                UILineMetrics line;
                line.startByte = paraStartByte;
                line.byteCount = trimmed.size();
                line.text = trimmed;
                line.width = measure(trimmed);
                outLines.push_back(line);
                return;
            }

            std::size_t lineStartPos = 0;
            std::size_t lineLength = 0;
            std::size_t pos = 0;

            while (pos < para.size())
            {
                std::size_t tokenLen = 0;
                const auto leadChar = static_cast<unsigned char>(para[pos]);

                if (IsWhitespace(para[pos]))
                {
                    std::size_t end = pos + 1;
                    while (end < para.size() && IsWhitespace(para[end]))
                    {
                        ++end;
                    }
                    tokenLen = end - pos;
                }
                else if (IsCjkByte(leadChar))
                {
                    tokenLen = NextCodepointLength(para, pos);
                }
                else
                {
                    std::size_t end = pos + 1;
                    while (end < para.size() && !IsWhitespace(para[end]) && !IsCjkByte(static_cast<unsigned char>(para[end])))
                    {
                        ++end;
                    }
                    tokenLen = end - pos;
                }

                const std::string_view token = para.substr(pos, tokenLen);

                if (lineLength == 0)
                {
                    if (pos > lineStartPos && IsWhitespace(token.front()))
                    {
                        pos += tokenLen;
                        lineStartPos = pos;
                        continue;
                    }

                    const float tokenWidth = measure(TrimRightWhitespace(token));
                    if (tokenWidth <= maxWidth)
                    {
                        lineLength = tokenLen;
                        pos += tokenLen;
                    }
                    else
                    {
                        std::size_t chunkLen = 0;
                        while (chunkLen < tokenLen)
                        {
                            const std::size_t cpLen = NextCodepointLength(token, chunkLen);
                            const std::string_view sub = token.substr(0, chunkLen + cpLen);
                            if (chunkLen > 0 && measure(sub) > maxWidth)
                            {
                                break;
                            }
                            chunkLen += cpLen;
                        }
                        if (chunkLen == 0) chunkLen = NextCodepointLength(token, 0);

                        std::string_view sub = token.substr(0, chunkLen);
                        UILineMetrics line;
                        line.startByte = paraStartByte + pos;
                        line.byteCount = sub.size();
                        line.text = sub;
                        line.width = measure(sub);
                        outLines.push_back(line);

                        pos += chunkLen;
                        lineStartPos = pos;
                        lineLength = 0;
                    }
                }
                else
                {
                    const std::size_t candidateLen = (pos + tokenLen) - lineStartPos;
                    const std::string_view candidate = TrimRightWhitespace(para.substr(lineStartPos, candidateLen));
                    const float candidateWidth = measure(candidate);

                    if (candidateWidth <= maxWidth)
                    {
                        lineLength = candidateLen;
                        pos += tokenLen;
                    }
                    else
                    {
                        const std::string_view lineStr = TrimRightWhitespace(para.substr(lineStartPos, lineLength));
                        UILineMetrics line;
                        line.startByte = paraStartByte + lineStartPos;
                        line.byteCount = lineStr.size();
                        line.text = lineStr;
                        line.width = measure(lineStr);
                        outLines.push_back(line);

                        lineStartPos = pos;
                        lineLength = 0;

                        if (IsWhitespace(token.front()))
                        {
                            pos += tokenLen;
                            lineStartPos = pos;
                        }
                    }
                }
            }

            if (lineLength > 0)
            {
                const std::string_view lineStr = TrimRightWhitespace(para.substr(lineStartPos, lineLength));
                UILineMetrics line;
                line.startByte = paraStartByte + lineStartPos;
                line.byteCount = lineStr.size();
                line.text = lineStr;
                line.width = measure(lineStr);
                outLines.push_back(line);
            }
        }
    }

    template<typename MeasureFunc>
    inline UIMultilineLayoutResult WrapMultilineText(
        std::string_view text,
        float maxWidth,
        MeasureFunc&& measureUtf8Width,
        float lineHeight = 24.0f,
        float boxHeight = 0.0f,
        EUITextAlignment horizontalAlign = EUITextAlignment::Left,
        EUIVerticalAlignment verticalAlign = EUIVerticalAlignment::Top)
    {
        UIMultilineLayoutResult result;
        if (lineHeight <= 0.0f) lineHeight = 1.0f;

        std::size_t start = 0;
        for (std::size_t i = 0; i <= text.size(); ++i)
        {
            if (i == text.size() || text[i] == '\n')
            {
                std::size_t len = i - start;
                if (len > 0 && text[start + len - 1] == '\r')
                {
                    --len;
                }
                const std::string_view para = text.substr(start, len);
                Detail::WrapParagraph(para, start, maxWidth, measureUtf8Width, result.lines);
                start = i + 1;
            }
        }

        float maxLineWidth = 0.0f;
        for (const auto& line : result.lines)
        {
            if (line.width > maxLineWidth)
            {
                maxLineWidth = line.width;
            }
        }
        result.totalWidth = maxLineWidth;
        result.totalHeight = static_cast<float>(result.lines.size()) * lineHeight;

        float startY = 0.0f;
        const float effectiveBoxHeight = (boxHeight > 0.0f) ? boxHeight : result.totalHeight;
        if (effectiveBoxHeight > result.totalHeight)
        {
            if (verticalAlign == EUIVerticalAlignment::Middle)
            {
                startY = (effectiveBoxHeight - result.totalHeight) * 0.5f;
            }
            else if (verticalAlign == EUIVerticalAlignment::Bottom)
            {
                startY = effectiveBoxHeight - result.totalHeight;
            }
        }

        const float effectiveBoxWidth = (maxWidth > 0.0f) ? maxWidth : maxLineWidth;
        for (std::size_t i = 0; i < result.lines.size(); ++i)
        {
            auto& line = result.lines[i];
            float offsetX = 0.0f;
            if (effectiveBoxWidth > line.width)
            {
                if (horizontalAlign == EUITextAlignment::Center)
                {
                    offsetX = (effectiveBoxWidth - line.width) * 0.5f;
                }
                else if (horizontalAlign == EUITextAlignment::Right)
                {
                    offsetX = effectiveBoxWidth - line.width;
                }
            }
            line.offset.x = offsetX;
            line.offset.y = startY + static_cast<float>(i) * lineHeight;
        }

        return result;
    }

    template<typename MeasureFunc>
    inline UIMultilineLayoutResult WrapMultilineText(
        std::string_view text,
        float maxWidth,
        float lineHeight,
        float boxHeight,
        EUITextAlignment horizontalAlign,
        EUIVerticalAlignment verticalAlign,
        MeasureFunc&& measureUtf8Width)
    {
        return WrapMultilineText(text, maxWidth, std::forward<MeasureFunc>(measureUtf8Width),
            lineHeight, boxHeight, horizontalAlign, verticalAlign);
    }
}
