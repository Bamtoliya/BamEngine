#pragma once

#include "ImGuiInterface.h"
#include "EditorPanelPluginApi.h"
#include "PluginModule.h"
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
        DrawPanelV1 Draw;
    };

    explicit PluginPanelModule(const wchar_t* dllName);

    bool Reload();
    bool Adopt(PluginModule& candidate);
    const Panel* FindPanel(std::wstring_view id) const;
    const std::vector<Panel>& GetPanels() const { return m_Panels; }

private:
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
    void Draw() override;

private:
    std::shared_ptr<PluginPanelModule> m_Module;
    std::string m_WindowId;
    EPanelMenuCategory m_Category;
};

END