#include "EditorPanelPluginApi.h"
#include "ModuleApiExport.h"

namespace
{
    void DrawFrameProbe(const Editor::PanelDrawContextV1& context)
    {
        static std::uint32_t drawCount = 0;
        context.TextInteger("PanelLifecycleProbe.Frame", context.FrameNumber);
        context.TextInteger("PanelLifecycleProbe.DrawCount", ++drawCount);
    }

    const Editor::PanelRegistrationV1 Panels[] = {
        { L"Editor.BuiltIn.PanelLifecycleProbe", "PanelLifecycleProbe.Title", &DrawFrameProbe },
    };

    const Editor::EditorPluginApiV1 PluginApi{
        sizeof(Editor::EditorPluginApiV1),
        sizeof(Editor::PanelRegistrationV1),
        sizeof(Editor::PanelDrawContextV1),
        1,
        Panels,
    };
}

EXPORT_PLUGIN_API_ENTRY(BamGetEditorPluginV1, Editor::EditorPluginApiV1, PluginApi);