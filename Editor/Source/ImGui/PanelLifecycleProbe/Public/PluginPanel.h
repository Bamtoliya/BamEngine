#pragma once

#include "ImGuiInterface.h"
#include "EditorPanelPluginApi.h"
#include "EditorPluginModule.h"
#include <string>

BEGIN(Editor)

class PluginPanel final : public IImGui
{
public:
    PluginPanel(const wchar_t* id, const wchar_t* dllName, const char* windowId, EPanelMenuCategory category);

    EPanelMenuCategory GetPanelMenuCategory() const override { return m_Category; }
    bool ShouldUpdateWhenClosed() const override { return m_ReloadPending; }

    void Draw() override;
    void Update(f32 dt) override;
    void Free() override;
    void RequestReload() { m_ReloadPending = true; }

private:
    EResult OnInitialize(void* arg) override;
    EResult ReloadModule();

    std::wstring m_DllName;
    std::string m_WindowId;
    EPanelMenuCategory m_Category;

    EditorPluginModule m_Module;
    DrawPanelV1 m_Draw = nullptr;
    bool m_ReloadPending = false;
};

END