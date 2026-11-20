#include "ConsolePanel.h"
#include "EditorPanelPluginApi.h"
#include "ModuleApiExport.h"
#include <imgui.h>
#include <cstring>
#include <iterator>

namespace
{
    using namespace Editor;

    void BindConsolePanelImGui(const PanelDrawContextV2& context)
    {
        ImGui::SetAllocatorFunctions(context.Allocate, context.Free, context.AllocatorData);
        ImGui::SetCurrentContext(context.ImGui);
    }

    void* CreateConsolePanel(const void* state, std::uint64_t stateSize)
    {
        auto* panel = new ConsolePanel();
        if (stateSize > 0 && !panel->RestorePluginState(state, static_cast<std::size_t>(stateSize)))
        {
            delete panel;
            return nullptr;
        }
        return panel;
    }

    void DestroyConsolePanel(void* instance)
    {
        delete static_cast<ConsolePanel*>(instance);
    }

    std::uint64_t GetConsolePanelStateSize(const void* instance)
    {
        return static_cast<const ConsolePanel*>(instance)->CapturePluginState().size();
    }

    bool CaptureConsolePanelState(const void* instance, void* state, std::uint64_t stateSize)
    {
        const auto snapshot = static_cast<const ConsolePanel*>(instance)->CapturePluginState();
        if (snapshot.size() != stateSize || (!state && stateSize > 0))
            return false;
        if (!snapshot.empty())
            std::memcpy(state, snapshot.data(), snapshot.size());
        return true;
    }

    void UpdateConsolePanel(void* instance, float dt)
    {
        static_cast<ConsolePanel*>(instance)->Update(dt);
    }

    void DrawConsolePanel(void* instance, const PanelDrawContextV2& context)
    {
        BindConsolePanelImGui(context);
        const bool rightMouseButtonDown = context.Host && context.Host->IsRightMouseButtonDown &&
            context.Host->IsRightMouseButtonDown();
        static_cast<ConsolePanel*>(instance)->Draw(rightMouseButtonDown);
    }

    void SaveConsoleWorkspace(const void* instance, void* context, SetWorkspaceFloatV2 setFloat)
    {
        if (context && setFloat)
            setFloat(context, "Console.DetailHeight", static_cast<const ConsolePanel*>(instance)->GetDetailViewHeight());
    }

    void RestoreConsoleWorkspace(void* instance, const void* context, GetWorkspaceFloatV2 getFloat)
    {
        if (context && getFloat)
            static_cast<ConsolePanel*>(instance)->SetDetailViewHeight(getFloat(context, "Console.DetailHeight", 160.0f));
    }

    const PanelRegistrationV2 Panels[] = {
        {
            .Id = L"Console",
            .TitleKey = "Console.Title",
            .InitialWidth = 0.0f,
            .InitialHeight = 0.0f,
            .WindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse,
            .StateVersion = 1,
            .Create = &CreateConsolePanel,
            .Destroy = &DestroyConsolePanel,
            .GetStateSize = &GetConsolePanelStateSize,
            .CaptureState = &CaptureConsolePanelState,
            .Update = &UpdateConsolePanel,
            .Draw = &DrawConsolePanel,
            .SaveWorkspace = &SaveConsoleWorkspace,
            .RestoreWorkspace = &RestoreConsoleWorkspace,
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
