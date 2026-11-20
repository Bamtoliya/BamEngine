#include "EditorPanelPluginApi.h"
#include "ModuleApiExport.h"
#include <imgui.h>
#include <string_view>

static_assert(std::string_view(Editor::PluginSdkCompatibility) == "bam-panel-v1-win64-msvc-1951-debug", "Build this plugin with the same compiler, architecture and configuration as the Editor");

namespace
{
    using namespace Editor;
    struct Panel {};
    void* Create(const void*, std::uint64_t) { return new Panel(); }
    void Destroy(void* instance) { delete static_cast<Panel*>(instance); }
    std::uint64_t StateSize(const void*) { return 0; }
    bool CaptureState(const void*, void*, std::uint64_t size) { return size == 0; }
    void Update(void*, float) {}
    void Draw(void*, const PanelDrawContextV2& context)
    {
        ImGui::SetAllocatorFunctions(context.Allocate, context.Free, context.AllocatorData);
        ImGui::SetCurrentContext(context.ImGui);
        ImGui::TextUnformatted("TestPanel");
    }
    void SaveWorkspace(const void*, void*, SetWorkspaceFloatV2) {}
    void RestoreWorkspace(void*, const void*, GetWorkspaceFloatV2) {}

    const PanelRegistrationV2 Panels[] = {{
        L"Editor.User.Test", "TestPanel", 640.0f, 480.0f, 0, 1,
        &Create, &Destroy, &StateSize, &CaptureState, &Update, &Draw, &SaveWorkspace, &RestoreWorkspace
    }};
    const PluginApiV2 Api{sizeof(PluginApiV2), sizeof(PanelRegistrationV2), sizeof(PanelDrawContextV2),
        sizeof(PanelHostServicesV2), 1, Panels};
}

EXPORT_PLUGIN_API_ENTRY(BamGetPluginV2, Editor::PluginApiV2, Api)
