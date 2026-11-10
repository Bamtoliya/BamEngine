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

    struct PluginApiV1
    {
        std::uint32_t Size;
        std::uint32_t PanelSize;
        std::uint32_t DrawContextSize;
        std::uint32_t PanelCount;
        const PanelRegistrationV1* Panels;
    };

    using GetPluginApiV1 = const PluginApiV1* (*)() noexcept;
#if defined(_WIN32) && defined(_M_X64) && defined(_MSC_VER) && !defined(__clang__)
#define BAM_PLUGIN_SDK_STRING_IMPL(value) #value
#define BAM_PLUGIN_SDK_STRING(value) BAM_PLUGIN_SDK_STRING_IMPL(value)

#if defined(_DEBUG)
    inline constexpr char PluginSdkCompatibility[] = "bam-panel-v1-win64-msvc-"
        BAM_PLUGIN_SDK_STRING(_MSC_VER) "-debug";
#else
    inline constexpr char PluginSdkCompatibility[] = "bam-panel-v1-win64-msvc-"
        BAM_PLUGIN_SDK_STRING(_MSC_VER) "-release";
#endif

#undef BAM_PLUGIN_SDK_STRING
#undef BAM_PLUGIN_SDK_STRING_IMPL
#else
    inline constexpr char PluginSdkCompatibility[] = "";
#endif
    inline constexpr char PluginEntryV1[] = "BamGetPluginV1";
}