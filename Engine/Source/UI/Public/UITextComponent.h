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

    ENUM()
    enum class EUITextVerticalAlignment
    {
        Top,
        Middle,
        Bottom
    };

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_UI_TEXT"), CATEGORY("CATEGORY_UI"))
    struct UITextComponent
    {
        REFLECT_BODY()

        static constexpr int MinFontPixels = 8;
        static constexpr int MaxFontPixels = 128;
        static constexpr std::size_t MaxTextBytes = 512;
        static constexpr std::size_t MaxFontFamilyBytes = 96;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_TEXT"))
        std::string text;
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_TEXT_USE_LOCALIZATION"))
        bool useLocalization = true;
        PROPERTY(EDITABLE, EDITCONDITION(useLocalization), DISPLAY_NAME("PROP_UI_TEXT_LOCALE_KEY"))
        std::string localizationKey;
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_FONT_FAMILY"))
        std::string fontFamily;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_FONT_PIXELS"))
        int fontPixels = 24;
        PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_COLOR"))
        glm::vec4 color{ 1.0f };
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UI_TEXT_ALIGNMENT"))
        EUITextAlignment alignment = EUITextAlignment::Center;
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_TEXT_VERTICAL_ALIGNMENT"))
        EUITextVerticalAlignment verticalAlignment = EUITextVerticalAlignment::Middle;
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_TEXT_MULTILINE"))
        bool multiline = false;
        PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_UI_TEXT_WORD_WRAP"))
        bool wordWrap = false;
    };
}
