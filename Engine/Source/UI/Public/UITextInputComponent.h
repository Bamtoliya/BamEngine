#pragma once

#include "ComponentAnnotations.h"
#include <algorithm>
#include <cstddef>
#include <glm/glm.hpp>
#include <string>
#include <string_view>

namespace Engine
{
    namespace UIUtf8
    {
        [[nodiscard]] constexpr bool IsContinuationByte(unsigned char c) noexcept
        {
            return (c & 0xC0) == 0x80;
        }

        [[nodiscard]] inline std::size_t PrevCodepointIndex(std::string_view str, std::size_t byteIndex) noexcept
        {
            if (byteIndex == 0 || str.empty()) return 0;
            if (byteIndex > str.size()) byteIndex = str.size();
            --byteIndex;
            while (byteIndex > 0 && IsContinuationByte(static_cast<unsigned char>(str[byteIndex])))
            {
                --byteIndex;
            }
            return byteIndex;
        }

        [[nodiscard]] inline std::size_t NextCodepointIndex(std::string_view str, std::size_t byteIndex) noexcept
        {
            if (byteIndex >= str.size()) return str.size();
            ++byteIndex;
            while (byteIndex < str.size() && IsContinuationByte(static_cast<unsigned char>(str[byteIndex])))
            {
                ++byteIndex;
            }
            return byteIndex;
        }

        [[nodiscard]] inline std::size_t ClampToCodepointBoundary(std::string_view str, std::size_t byteIndex) noexcept
        {
            if (byteIndex >= str.size()) return str.size();
            while (byteIndex > 0 && IsContinuationByte(static_cast<unsigned char>(str[byteIndex])))
            {
                --byteIndex;
            }
            return byteIndex;
        }

        [[nodiscard]] inline std::size_t CountCodepoints(std::string_view str) noexcept
        {
            std::size_t count = 0;
            for (const char c : str)
            {
                if (!IsContinuationByte(static_cast<unsigned char>(c)))
                {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] inline std::string_view TruncateCodepoints(std::string_view str, std::size_t maxCodepoints) noexcept
        {
            std::size_t count = 0;
            for (std::size_t i = 0; i < str.size(); ++i)
            {
                if (!IsContinuationByte(static_cast<unsigned char>(str[i])))
                {
                    if (count == maxCodepoints)
                    {
                        return str.substr(0, i);
                    }
                    ++count;
                }
            }
            return str;
        }
    }

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_TEXT_INPUT"), CATEGORY("CATEGORY_UI"))
    struct UITextInputComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE)
        std::string text;

        PROPERTY(EDITABLE)
        std::string placeholder;

        PROPERTY(EDITABLE, VISIBLE(false))
        std::string imeComposition;

        PROPERTY(EDITABLE)
        std::size_t maxLength = 256;

        PROPERTY(EDITABLE)
        bool multiline = false;

        PROPERTY(EDITABLE)
        bool isPassword = false;

        PROPERTY(EDITABLE)
        bool readOnly = false;

        PROPERTY(EDITABLE, VISIBLE(false))
        bool isFocused = false;

        PROPERTY(EDITABLE, VISIBLE(false))
        std::size_t cursorByteIndex = 0;

        PROPERTY(EDITABLE, VISIBLE(false))
        std::size_t selectionAnchorByteIndex = 0;

        PROPERTY(EDITABLE, COLOR_PROPERTY)
        glm::vec4 textColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        PROPERTY(EDITABLE, COLOR_PROPERTY)
        glm::vec4 placeholderColor{ 0.5f, 0.5f, 0.5f, 1.0f };

        PROPERTY(EDITABLE, COLOR_PROPERTY)
        glm::vec4 selectionColor{ 0.26f, 0.59f, 0.98f, 0.4f };

        PROPERTY(EDITABLE, COLOR_PROPERTY)
        glm::vec4 caretColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        [[nodiscard]] bool HasSelection() const noexcept
        {
            return selectionAnchorByteIndex != cursorByteIndex && !text.empty();
        }

        [[nodiscard]] std::size_t SelectionStart() const noexcept
        {
            return std::min(cursorByteIndex, selectionAnchorByteIndex);
        }

        [[nodiscard]] std::size_t SelectionEnd() const noexcept
        {
            return std::max(cursorByteIndex, selectionAnchorByteIndex);
        }

        [[nodiscard]] std::string GetSelectedText() const
        {
            if (!HasSelection()) return {};
            const std::size_t start = SelectionStart();
            const std::size_t end = SelectionEnd();
            if (start >= text.size()) return {};
            return text.substr(start, std::min(end, text.size()) - start);
        }

        void SelectAll() noexcept
        {
            selectionAnchorByteIndex = 0;
            cursorByteIndex = text.size();
        }

        void ClearSelection() noexcept
        {
            selectionAnchorByteIndex = cursorByteIndex;
        }

        void ClampIndices() noexcept
        {
            cursorByteIndex = UIUtf8::ClampToCodepointBoundary(text, cursorByteIndex);
            selectionAnchorByteIndex = UIUtf8::ClampToCodepointBoundary(text, selectionAnchorByteIndex);
        }

        bool DeleteSelection()
        {
            if (readOnly || !HasSelection()) return false;
            ClampIndices();
            const std::size_t start = SelectionStart();
            const std::size_t end = SelectionEnd();
            if (start >= text.size()) return false;
            text.erase(start, std::min(end, text.size()) - start);
            cursorByteIndex = start;
            selectionAnchorByteIndex = start;
            return true;
        }

        void MoveCursorLeft(bool select = false) noexcept
        {
            ClampIndices();
            if (!select && HasSelection())
            {
                cursorByteIndex = SelectionStart();
                selectionAnchorByteIndex = cursorByteIndex;
                return;
            }
            cursorByteIndex = UIUtf8::PrevCodepointIndex(text, cursorByteIndex);
            if (!select)
            {
                selectionAnchorByteIndex = cursorByteIndex;
            }
        }

        void MoveCursorRight(bool select = false) noexcept
        {
            ClampIndices();
            if (!select && HasSelection())
            {
                cursorByteIndex = SelectionEnd();
                selectionAnchorByteIndex = cursorByteIndex;
                return;
            }
            cursorByteIndex = UIUtf8::NextCodepointIndex(text, cursorByteIndex);
            if (!select)
            {
                selectionAnchorByteIndex = cursorByteIndex;
            }
        }

        bool Backspace()
        {
            if (readOnly) return false;
            if (HasSelection())
            {
                return DeleteSelection();
            }
            ClampIndices();
            if (cursorByteIndex == 0 || text.empty()) return false;
            const std::size_t prev = UIUtf8::PrevCodepointIndex(text, cursorByteIndex);
            text.erase(prev, cursorByteIndex - prev);
            cursorByteIndex = prev;
            selectionAnchorByteIndex = prev;
            return true;
        }

        bool DeleteForward()
        {
            if (readOnly) return false;
            if (HasSelection())
            {
                return DeleteSelection();
            }
            ClampIndices();
            if (cursorByteIndex >= text.size()) return false;
            const std::size_t next = UIUtf8::NextCodepointIndex(text, cursorByteIndex);
            text.erase(cursorByteIndex, next - cursorByteIndex);
            selectionAnchorByteIndex = cursorByteIndex;
            return true;
        }

        bool InsertUtf8(std::string_view utf8Str)
        {
            if (readOnly || utf8Str.empty()) return false;
            if (HasSelection())
            {
                DeleteSelection();
            }
            ClampIndices();

            std::string filtered;
            if (!multiline)
            {
                filtered.reserve(utf8Str.size());
                for (const char c : utf8Str)
                {
                    if (c != '\r' && c != '\n')
                    {
                        filtered.push_back(c);
                    }
                }
                utf8Str = filtered;
                if (utf8Str.empty()) return false;
            }

            if (maxLength > 0)
            {
                const std::size_t currentCodepoints = UIUtf8::CountCodepoints(text);
                if (currentCodepoints >= maxLength)
                {
                    return false;
                }
                const std::size_t available = maxLength - currentCodepoints;
                utf8Str = UIUtf8::TruncateCodepoints(utf8Str, available);
                if (utf8Str.empty()) return false;
            }

            text.insert(cursorByteIndex, utf8Str);
            cursorByteIndex += utf8Str.size();
            selectionAnchorByteIndex = cursorByteIndex;
            return true;
        }

        void SetImeComposition(std::string_view composition)
        {
            if (readOnly) return;
            if (HasSelection())
            {
                DeleteSelection();
            }
            imeComposition = std::string(composition);
        }

        void CommitImeComposition()
        {
            if (imeComposition.empty()) return;
            std::string comp = std::move(imeComposition);
            imeComposition.clear();
            InsertUtf8(comp);
        }

        void CancelImeComposition() noexcept
        {
            imeComposition.clear();
        }

        [[nodiscard]] std::string GetDisplayText() const
        {
            if (!isPassword)
            {
                if (imeComposition.empty()) return text;
                const std::size_t clampedCursor = std::min(cursorByteIndex, text.size());
                std::string result;
                result.reserve(text.size() + imeComposition.size());
                result.append(text.substr(0, clampedCursor));
                result.append(imeComposition);
                result.append(text.substr(clampedCursor));
                return result;
            }
            else
            {
                const std::size_t totalChars = UIUtf8::CountCodepoints(text) + UIUtf8::CountCodepoints(imeComposition);
                return std::string(totalChars, '*');
            }
        }
    };
}
