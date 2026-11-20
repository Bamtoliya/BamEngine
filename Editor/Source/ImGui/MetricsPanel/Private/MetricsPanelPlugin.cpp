#include "MetricsPanel.h"
#include "EditorPanelPluginApi.h"
#include "ModuleApiExport.h"
#include <imgui.h>
#include <cstring>
#include <iterator>

namespace
{
    using namespace Editor;

    void BindMetricsPanelImGui(const PanelDrawContextV2& context)
    {
        ImGui::SetAllocatorFunctions(context.Allocate, context.Free, context.AllocatorData);
        ImGui::SetCurrentContext(context.ImGui);
    }

    void* CreateMetricsPanel(const void* state, std::uint64_t stateSize)
    {
        auto* panel = new MetricsPanel();
        if (stateSize > 0 && !panel->RestorePluginState(state, static_cast<std::size_t>(stateSize)))
        {
            delete panel;
            return nullptr;
        }
        return panel;
    }

    void DestroyMetricsPanel(void* instance)
    {
        delete static_cast<MetricsPanel*>(instance);
    }

    std::uint64_t GetMetricsPanelStateSize(const void* instance)
    {
        return static_cast<const MetricsPanel*>(instance)->CapturePluginState().size();
    }

    bool CaptureMetricsPanelState(const void* instance, void* state, std::uint64_t stateSize)
    {
        const auto snapshot = static_cast<const MetricsPanel*>(instance)->CapturePluginState();
        if (snapshot.size() != stateSize || (!state && stateSize > 0))
            return false;
        if (!snapshot.empty())
            std::memcpy(state, snapshot.data(), snapshot.size());
        return true;
    }

    void UpdateMetricsPanel(void* instance, float dt)
    {
        static_cast<MetricsPanel*>(instance)->Update(dt);
    }

    void DrawMetricsPanel(void* instance, const PanelDrawContextV2& context)
    {
        BindMetricsPanelImGui(context);
        static_cast<MetricsPanel*>(instance)->Draw(*context.Host);
    }

    void SaveEmptyWorkspace(const void*, void*, SetWorkspaceFloatV2)
    {
    }

    void RestoreEmptyWorkspace(void*, const void*, GetWorkspaceFloatV2)
    {
    }

    const PanelRegistrationV2 Panels[] = {
        {
            .Id = L"Metrics",
            .TitleKey = "Metrics.Title",
            .InitialWidth = 0.0f,
            .InitialHeight = 0.0f,
            .WindowFlags = 0,
            .StateVersion = 1,
            .Create = &CreateMetricsPanel,
            .Destroy = &DestroyMetricsPanel,
            .GetStateSize = &GetMetricsPanelStateSize,
            .CaptureState = &CaptureMetricsPanelState,
            .Update = &UpdateMetricsPanel,
            .Draw = &DrawMetricsPanel,
            .SaveWorkspace = &SaveEmptyWorkspace,
            .RestoreWorkspace = &RestoreEmptyWorkspace,
        },
    };

    const PluginApiV2 PluginApi{
        .Size = sizeof(PluginApiV2),
        .PanelSize = sizeof(PanelRegistrationV2),
        .DrawContextSize = sizeof(PanelDrawContextV2),
        .HostServicesSize = sizeof(PanelHostServicesV2),
        .PanelCount = static_cast<std::uint32_t>(std::size(Panels)),
        .Panels = Panels,
    };
}

EXPORT_PLUGIN_API_ENTRY(BamGetPluginV2, Editor::PluginApiV2, PluginApi)
