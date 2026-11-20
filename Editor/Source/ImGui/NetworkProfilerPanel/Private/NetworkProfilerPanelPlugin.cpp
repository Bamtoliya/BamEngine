#include "NetworkProfilerPanel.h"
#include "EditorPanelPluginApi.h"
#include "ModuleApiExport.h"
#include <imgui.h>
#include <cstring>
#include <iterator>

namespace
{
    using namespace Editor;

    void BindNetworkProfilerPanelImGui(const PanelDrawContextV2& context)
    {
        ImGui::SetAllocatorFunctions(context.Allocate, context.Free, context.AllocatorData);
        ImGui::SetCurrentContext(context.ImGui);
    }

    void* CreateNetworkProfilerPanel(const void* state, std::uint64_t stateSize)
    {
        auto* panel = new NetworkProfilerPanel();
        if (stateSize > 0 && !panel->RestorePluginState(state, static_cast<std::size_t>(stateSize)))
        {
            delete panel;
            return nullptr;
        }
        return panel;
    }

    void DestroyNetworkProfilerPanel(void* instance)
    {
        delete static_cast<NetworkProfilerPanel*>(instance);
    }

    std::uint64_t GetNetworkProfilerPanelStateSize(const void* instance)
    {
        return static_cast<const NetworkProfilerPanel*>(instance)->CapturePluginState().size();
    }

    bool CaptureNetworkProfilerPanelState(const void* instance, void* state, std::uint64_t stateSize)
    {
        const auto snapshot = static_cast<const NetworkProfilerPanel*>(instance)->CapturePluginState();
        if (snapshot.size() != stateSize || (!state && stateSize > 0))
            return false;
        if (!snapshot.empty())
            std::memcpy(state, snapshot.data(), snapshot.size());
        return true;
    }

    void UpdateNetworkProfilerPanel(void* instance, float dt)
    {
        static_cast<NetworkProfilerPanel*>(instance)->Update(dt);
    }

    void DrawNetworkProfilerPanel(void* instance, const PanelDrawContextV2& context)
    {
        BindNetworkProfilerPanelImGui(context);
        static_cast<NetworkProfilerPanel*>(instance)->Draw();
    }

    void SaveEmptyWorkspace(const void*, void*, SetWorkspaceFloatV2)
    {
    }

    void RestoreEmptyWorkspace(void*, const void*, GetWorkspaceFloatV2)
    {
    }

    const PanelRegistrationV2 Panels[] = {
        {
            .Id = L"Network Profiler",
            .TitleKey = "NetworkProfiler.Title",
            .InitialWidth = 520.0f,
            .InitialHeight = 480.0f,
            .WindowFlags = 0,
            .StateVersion = 1,
            .Create = &CreateNetworkProfilerPanel,
            .Destroy = &DestroyNetworkProfilerPanel,
            .GetStateSize = &GetNetworkProfilerPanelStateSize,
            .CaptureState = &CaptureNetworkProfilerPanelState,
            .Update = &UpdateNetworkProfilerPanel,
            .Draw = &DrawNetworkProfilerPanel,
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
