#pragma once

#include <cstdint>

namespace Editor
{
    struct PanelDrawContextV1
    {
        std::uint32_t FrameNumber;
        void (*TextInteger)(const char* labelKey, std::uint32_t value);
    };

    using DrawPanelV1 = void (*)(const PanelDrawContextV1&);

    struct PanelRegistrationV1
    {
        const wchar_t* Id;
        const char* TitleKey;
        DrawPanelV1 Draw;
    };

    struct EditorPluginApiV1
    {
        std::uint32_t Size;
        std::uint32_t PanelSize;
        std::uint32_t DrawContextSize;
        std::uint32_t PanelCount;
        const PanelRegistrationV1* Panels;
    };

    using GetEditorPluginApiV1 = const EditorPluginApiV1* (*)() noexcept;
    inline constexpr char EditorPluginEntryV1[] = "BamGetEditorPluginV1";
}