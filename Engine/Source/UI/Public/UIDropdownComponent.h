#pragma once

#include "ComponentAnnotations.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace Engine
{
    STRUCT(COMPONENT, CATEGORY("CATEGORY_UI"))
    struct UIDropdownComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE)
        std::vector<std::string> options;

        PROPERTY(EDITABLE)
        int selectedIndex = -1;

        PROPERTY(EDITABLE)
        bool isOpen = false;

        PROPERTY(EDITABLE)
        int maxVisibleItems = 5;

        PROPERTY(EDITABLE)
        float itemHeight = 24.0f;

        PROPERTY(EDITABLE)
        float scrollOffset = 0.0f;

        PROPERTY(EDITABLE)
        std::string placeholder = "Select...";

        PROPERTY(EDITABLE)
        bool interactable = true;

        PROPERTY(EDITABLE, COLOR_PROPERTY)
        glm::vec4 normalColor{ 0.22f, 0.22f, 0.25f, 1.0f };

        PROPERTY(EDITABLE, COLOR_PROPERTY)
        glm::vec4 popupBackgroundColor{ 0.18f, 0.18f, 0.20f, 1.0f };

        PROPERTY(EDITABLE, COLOR_PROPERTY)
        glm::vec4 highlightColor{ 0.26f, 0.59f, 0.98f, 1.0f };

        PROPERTY(EDITABLE, COLOR_PROPERTY)
        glm::vec4 textColor{ 1.0f, 1.0f, 1.0f, 1.0f };

        bool SelectIndex(int index) noexcept
        {
            if (index >= 0 && index < static_cast<int>(options.size()))
            {
                selectedIndex = index;
                isOpen = false;
                return true;
            }
            if (index == -1)
            {
                selectedIndex = -1;
                isOpen = false;
                return true;
            }
            return false;
        }

        void ToggleOpen() noexcept
        {
            if (!interactable) return;
            if (isOpen)
            {
                Close();
            }
            else
            {
                Open();
            }
        }

        void Open() noexcept
        {
            if (!interactable || options.empty()) return;
            isOpen = true;
            scrollOffset = 0.0f;
        }

        void Close() noexcept
        {
            isOpen = false;
            scrollOffset = 0.0f;
        }

        [[nodiscard]] std::string_view GetSelectedLabel() const noexcept
        {
            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(options.size()))
            {
                return options[static_cast<std::size_t>(selectedIndex)];
            }
            return placeholder;
        }

        [[nodiscard]] float GetPopupHeight() const noexcept
        {
            if (!isOpen || options.empty() || itemHeight <= 0.0f) return 0.0f;
            const int visibleCount = std::min(static_cast<int>(options.size()), std::max(1, maxVisibleItems));
            return static_cast<float>(visibleCount) * itemHeight;
        }

        [[nodiscard]] float GetTotalContentHeight() const noexcept
        {
            if (options.empty() || itemHeight <= 0.0f) return 0.0f;
            return static_cast<float>(options.size()) * itemHeight;
        }

        [[nodiscard]] int HitTestOptionIndex(float localY) const noexcept
        {
            if (!isOpen || options.empty() || itemHeight <= 0.0f || localY < 0.0f) return -1;
            const float popupHeight = GetPopupHeight();
            if (localY >= popupHeight) return -1;

            const float scrolledY = localY + scrollOffset;
            if (scrolledY < 0.0f) return -1;
            const int index = static_cast<int>(scrolledY / itemHeight);
            if (index >= 0 && index < static_cast<int>(options.size()))
            {
                return index;
            }
            return -1;
        }
    };
}
