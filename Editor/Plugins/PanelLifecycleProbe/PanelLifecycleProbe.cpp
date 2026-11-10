#include "EditorPanelPluginApi.h"
#include "ModuleApiExport.h"

namespace
{
    void DrawFrameProbe(const Editor::PanelDrawContextV1& context)
    {
        static std::uint32_t drawCount = 0;
        context.TextInteger("PanelLifecycleProbe.Frame", context.FrameNumber);
        context.TextInteger("PanelLifecycleProbe.DrawCount", drawCount += 5);
    }

    void DrawSecondProbe(const Editor::PanelDrawContextV1& context)
    {
        static std::uint32_t drawCount = 0;
        context.TextInteger("PanelLifecycleProbe.Frame", context.FrameNumber);
        context.TextInteger("PanelLifecycleProbe.DrawCount", drawCount += 100);
    }

    const Editor::PanelRegistrationV1 Panels[] = {
        { L"Editor.BuiltIn.PanelLifecycleProbe", "PanelLifecycleProbe.Title", &DrawFrameProbe },
        { L"Editor.BuiltIn.PanelLifecycleProbe.Second", "PanelLifecycleProbe.SecondTitle", &DrawSecondProbe },
    };

    const Editor::PluginApiV1 PluginApi{
        sizeof(Editor::PluginApiV1),
        sizeof(Editor::PanelRegistrationV1),
        sizeof(Editor::PanelDrawContextV1),
        static_cast<std::uint32_t>(sizeof(Panels) / sizeof(Panels[0])),
        Panels,
    };
}

EXPORT_PLUGIN_API_ENTRY(BamGetPluginV1, Editor::PluginApiV1, PluginApi);