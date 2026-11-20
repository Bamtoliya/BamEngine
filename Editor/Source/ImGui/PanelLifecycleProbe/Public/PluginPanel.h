#pragma once

#include "ImGuiInterface.h"
#include "EditorPanelPluginApi.h"
#include "PluginModule.h"
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

BEGIN(Editor)

class PluginPanelModule final
{
public:
    struct Panel
    {
        std::wstring Id;
        std::string TitleKey;
        DrawPanelV1 DrawV1 = nullptr;
        PanelRegistrationV2 ApiV2{};
        void* Instance = nullptr;
    };

    explicit PluginPanelModule(const wchar_t* dllName);
    ~PluginPanelModule();

    bool Reload();
    bool Adopt(PluginModule& candidate);
    const Panel* FindPanel(std::wstring_view id) const;
    const std::vector<Panel>& GetPanels() const { return m_Panels; }
    void Update(std::wstring_view id, float dt);
    void Draw(std::wstring_view id, const PanelHostServicesV2& host);
    void CaptureWorkspace(std::wstring_view id, EditorWorkspace& workspace) const;
    void RestoreWorkspace(std::wstring_view id, const EditorWorkspace& workspace);

private:
    void DestroyPanels(std::vector<Panel>& panels) const;

    std::wstring m_DllName;
    PluginModule m_Module;
    std::vector<Panel> m_Panels;
};

class PluginPanel final : public IImGui
{
public:
    PluginPanel(std::shared_ptr<PluginPanelModule> module, const std::wstring& id,
        std::string windowId, EPanelMenuCategory category);

    EPanelMenuCategory GetPanelMenuCategory() const override { return m_Category; }
    string GetDisplayName() const override;
    void Update(f32 dt) override;
    void Draw() override;
    void CaptureState(EditorWorkspace& workspace) const override;
    void RestoreState(const EditorWorkspace& workspace) override;

private:
    std::shared_ptr<PluginPanelModule> m_Module;
    std::string m_WindowId;
    EPanelMenuCategory m_Category;
};

END