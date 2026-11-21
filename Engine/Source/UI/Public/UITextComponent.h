#pragma once

#include "ComponentAnnotations.h"
#include <glm/glm.hpp>
#include <string>

namespace Engine
{
    ENUM()
    enum class EUITextAlignment
    {
        Left,
        Center,
        Right
    };

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_TEXT"), CATEGORY("CATEGORY_UI"))
    struct UITextComponent
    {
        REFLECT_BODY()

        static constexpr int MinFontPixels = 8;
        static constexpr int MaxFontPixels = 128;
        static constexpr std::size_t MaxTextBytes = 512;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_TEXT"))
        std::string text;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_TEXT_LOCALE_KEY"))
        std::string localizationKey;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_FONT_PIXELS"))
        int fontPixels = 24;
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_COLOR"))
        glm::vec4 color{ 1.0f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_TEXT_ALIGNMENT"))
        EUITextAlignment alignment = EUITextAlignment::Center;
    };
}
