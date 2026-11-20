#pragma once

#include <cstddef>
#include <cstdint>

struct ImGuiContext;

namespace Editor
{
    using PanelImGuiMemAllocFuncV2 = void* (*)(std::size_t size, void* userData);
    using PanelImGuiMemFreeFuncV2 = void (*)(void* memory, void* userData);

    inline constexpr std::uint32_t PluginReferenceTimingKindCountV2 = 8;

    struct PluginReferenceTimingV2
    {
        std::uint64_t Samples;
        double LastMs;
        double TotalMs;
        double PeakMs;
    };

    struct PluginReferenceScanTimingV2
    {
        std::uint32_t Kind;
        std::int32_t Frame;
        std::uint64_t Directories;
        std::uint64_t Documents;
        std::uint64_t References;
        std::uint64_t Issues;
        double TraversalMs;
        double ReadParseMs;
        double ResolveMs;
        double RefreshViewMs;
    };

    struct PluginEngineTaskMetricsV2
    {
        std::uint32_t Size;
        std::uint64_t Outstanding;
        std::uint64_t Succeeded;
        std::uint64_t Failed;
        std::uint64_t ActiveSceneCount;
        std::uint64_t RegisteredPassCount;
        std::int32_t LastFrame;
        double LastFrameMs;
        double PeakFrameMs;
        PluginReferenceScanTimingV2 LastScan;
        PluginReferenceTimingV2 ByKind[PluginReferenceTimingKindCountV2];
    };

    struct PanelHostServicesV2
    {
        bool (*IsRightMouseButtonDown)();
        void (*GetEngineTaskMetrics)(PluginEngineTaskMetricsV2* metrics);
        void (*ResetAssetReferenceTimings)();
    };

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

    struct PanelDrawContextV2
    {
        std::uint32_t FrameNumber;
        const PanelHostServicesV2* Host;
        ImGuiContext* ImGui;
        PanelImGuiMemAllocFuncV2 Allocate;
        PanelImGuiMemFreeFuncV2 Free;
        void* AllocatorData;
    };

    using CreatePanelV2 = void* (*)(const void* state, std::uint64_t stateSize);
    using DestroyPanelV2 = void (*)(void* instance);
    using GetPanelStateSizeV2 = std::uint64_t (*)(const void* instance);
    using CapturePanelStateV2 = bool (*)(const void* instance, void* state, std::uint64_t stateSize);
    using UpdatePanelV2 = void (*)(void* instance, float dt);
    using DrawPanelV2 = void (*)(void* instance, const PanelDrawContextV2& context);
    using SetWorkspaceFloatV2 = void (*)(void* context, const char* key, float value);
    using GetWorkspaceFloatV2 = float (*)(const void* context, const char* key, float defaultValue);

    struct PanelRegistrationV2
    {
        const wchar_t* Id;
        const char* TitleKey;
        float InitialWidth;
        float InitialHeight;
        std::uint32_t WindowFlags;
        std::uint32_t StateVersion;
        CreatePanelV2 Create;
        DestroyPanelV2 Destroy;
        GetPanelStateSizeV2 GetStateSize;
        CapturePanelStateV2 CaptureState;
        UpdatePanelV2 Update;
        DrawPanelV2 Draw;
        void (*SaveWorkspace)(const void* instance, void* context, SetWorkspaceFloatV2 setFloat);
        void (*RestoreWorkspace)(void* instance, const void* context, GetWorkspaceFloatV2 getFloat);
    };

    struct PluginApiV2
    {
        std::uint32_t Size;
        std::uint32_t PanelSize;
        std::uint32_t DrawContextSize;
        std::uint32_t HostServicesSize;
        std::uint32_t PanelCount;
        const PanelRegistrationV2* Panels;
    };

    using GetPluginApiV2 = const PluginApiV2* (*)() noexcept;
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
    inline constexpr char PluginEntryV2[] = "BamGetPluginV2";
}