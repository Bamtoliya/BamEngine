#pragma once

#include "ComponentAnnotations.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_PROGRESS_BAR"), CATEGORY("CATEGORY_UI"))
    struct UIProgressBarComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_VALUE_MIN"))
        float minimum = 0.0f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_VALUE_MAX"))
        float maximum = 1.0f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_VALUE"))
        float value = 0.5f;
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_UI_FILL_COLOR"))
        glm::vec4 fillColor{ 0.25f, 0.72f, 0.42f, 1.0f };
    };

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_SLIDER"), CATEGORY("CATEGORY_UI"))
    struct UISliderComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_VALUE_MIN"))
        float minimum = 0.0f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_VALUE_MAX"))
        float maximum = 1.0f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_VALUE_STEP"))
        float step = 0.1f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_VALUE"))
        float value = 0.5f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INTERACTABLE"))
        bool interactable = true;
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_UI_FILL_COLOR"))
        glm::vec4 fillColor{ 0.25f, 0.62f, 0.90f, 1.0f };
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_UI_HANDLE_COLOR"))
        glm::vec4 handleColor{ 0.92f, 0.94f, 1.0f, 1.0f };
    };

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_TOGGLE"), CATEGORY("CATEGORY_UI"))
    struct UIToggleComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_CHECKED"))
        bool checked = false;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INTERACTABLE"))
        bool interactable = true;
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_UI_CHECK_COLOR"))
        glm::vec4 checkColor{ 0.25f, 0.72f, 0.42f, 1.0f };
    };

    namespace UIWidgetRange
    {
        inline bool IsValid(float minimum, float maximum, float value)
        {
            return std::isfinite(minimum) && std::isfinite(maximum) && std::isfinite(value) && maximum > minimum;
        }

        inline bool IsValid(const UISliderComponent& slider)
        {
            return IsValid(slider.minimum, slider.maximum, slider.value) && std::isfinite(slider.step) && slider.step >= 0.0f;
        }

        inline float Fraction(float minimum, float maximum, float value)
        {
            const double extent = static_cast<double>(maximum) - minimum;
            const double offset = static_cast<double>(value) - minimum;
            return static_cast<float>(std::clamp(offset / extent, 0.0, 1.0));
        }

        inline float ClampSliderValue(const UISliderComponent& slider, double value)
        {
            if (value <= slider.minimum) return slider.minimum;
            if (value >= slider.maximum) return slider.maximum;
            if (slider.step > 0.0f)
            {
                const double steps = (value - slider.minimum) / slider.step;
                value = slider.minimum + std::round(steps) * slider.step;
            }
            return static_cast<float>(std::clamp(value, static_cast<double>(slider.minimum), static_cast<double>(slider.maximum)));
        }
    }
}
